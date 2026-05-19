/*
 * Custom CreatureScript: Bot Creator NPC (entry 600002)
 *
 * Five-step gossip wizard: race -> class -> spec -> gender -> name.
 * Spawns a new playerbot character on the master's account, or on a
 * <MASTER>_ALTS_<n> alt account if the master is full at the 10-char cap.
 * Alt accounts get auto-linked back to master via playerbots_account_links
 * so the existing Botmaster NPC picks them up for free.
 *
 * Level/spec/gear are applied on first bot login via a paired PlayerScript
 * (BotCreatorApplyOnLoginScript) — see g_pendingApply.
 */

// MSVC 14.38 <chrono> ICE workaround — see EmblemVendorCache.cpp.
#include <chrono>

#include "Playerbots.h"

#include "AccountMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint32 BOTCREATOR_ENTRY   = 600002;
    constexpr uint32 BOTCREATOR_TEXT_ID = 60003;

    // Gossip sender codes — disambiguate which menu page a click came from.
    constexpr uint32 SENDER_HELLO        = 200;
    constexpr uint32 SENDER_PICK_RACE    = 201;
    constexpr uint32 SENDER_PICK_CLASS   = 202;
    constexpr uint32 SENDER_PICK_SPEC    = 203;
    constexpr uint32 SENDER_PICK_GENDER  = 204;
    constexpr uint32 SENDER_NAME_PROMPT  = 205;
    constexpr uint32 SENDER_RETRY        = 206;

    // Control codes — chosen outside any real race/class/spec/gender id range.
    constexpr uint32 ACTION_BACK   = 0xF0000001;
    constexpr uint32 ACTION_CLOSE  = 0xF0000002;
    constexpr uint32 ACTION_HELLO  = 0xF0000003;
    constexpr uint32 ACTION_RETRY  = 0xF0000004;

    // We bias spec ids by +1 to keep 0 free as a sentinel for "not chosen".
    constexpr uint32 SPEC_ACTION_BIAS = 1;

    // Per-master pending state as they walk through the wizard.
    struct PendingMenu
    {
        uint8 race    = 0;
        uint8 cls     = 0;
        int8  specNo  = -1;     // 0/1/2
        uint8 gender  = GENDER_MALE;
    };
    std::unordered_map<ObjectGuid, PendingMenu> g_pendingMenu;

    // Bot guids that need level/spec/gear applied on first login.
    struct PendingApply
    {
        uint32 level  = 1;
        int8   specNo = -1;
    };
    std::unordered_map<ObjectGuid, PendingApply> g_pendingApply;

    // ---- Config helpers -----------------------------------------------------

    uint32 CfgMaxBotsPerMaster()
    {
        return uint32(sConfigMgr->GetOption<int32>("AiPlayerbot.BotCreator.MaxBotsPerMaster", 50));
    }
    uint32 CfgMaxAltAccounts()
    {
        return uint32(sConfigMgr->GetOption<int32>("AiPlayerbot.BotCreator.MaxAltAccounts", 5));
    }
    std::string CfgAltAccountPassword()
    {
        return sConfigMgr->GetOption<std::string>("AiPlayerbot.BotCreator.AltAccountPassword", "tbots");
    }

    // ---- Display-name helpers ----------------------------------------------

    char const* ClassName(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return "Warrior";
            case CLASS_PALADIN:      return "Paladin";
            case CLASS_HUNTER:       return "Hunter";
            case CLASS_ROGUE:        return "Rogue";
            case CLASS_PRIEST:       return "Priest";
            case CLASS_DEATH_KNIGHT: return "Death Knight";
            case CLASS_SHAMAN:       return "Shaman";
            case CLASS_MAGE:         return "Mage";
            case CLASS_WARLOCK:      return "Warlock";
            case CLASS_DRUID:        return "Druid";
            default:                 return "Unknown";
        }
    }

    char const* RaceName(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:         return "Human";
            case RACE_ORC:           return "Orc";
            case RACE_DWARF:         return "Dwarf";
            case RACE_NIGHTELF:      return "Night Elf";
            case RACE_UNDEAD_PLAYER: return "Undead";
            case RACE_TAUREN:        return "Tauren";
            case RACE_GNOME:         return "Gnome";
            case RACE_TROLL:         return "Troll";
            case RACE_BLOODELF:      return "Blood Elf";
            case RACE_DRAENEI:       return "Draenei";
            default:                 return "Unknown";
        }
    }

    // Spec names laid out in tab order (0/1/2 maps to talent tree page).
    char const* SpecName(uint8 cls, uint8 specNo)
    {
        static char const* warrior[]  = {"Arms",          "Fury",         "Protection"};
        static char const* paladin[]  = {"Holy",          "Protection",   "Retribution"};
        static char const* hunter[]   = {"Beast Mastery", "Marksmanship", "Survival"};
        static char const* rogue[]    = {"Assassination", "Combat",       "Subtlety"};
        static char const* priest[]   = {"Discipline",    "Holy",         "Shadow"};
        static char const* dk[]       = {"Blood",         "Frost",        "Unholy"};
        static char const* shaman[]   = {"Elemental",     "Enhancement",  "Restoration"};
        static char const* mage[]     = {"Arcane",        "Fire",         "Frost"};
        static char const* warlock[]  = {"Affliction",    "Demonology",   "Destruction"};
        static char const* druid[]    = {"Balance",       "Feral",        "Restoration"};

        if (specNo > 2) return "Unknown";
        switch (cls)
        {
            case CLASS_WARRIOR:      return warrior[specNo];
            case CLASS_PALADIN:      return paladin[specNo];
            case CLASS_HUNTER:       return hunter[specNo];
            case CLASS_ROGUE:        return rogue[specNo];
            case CLASS_PRIEST:       return priest[specNo];
            case CLASS_DEATH_KNIGHT: return dk[specNo];
            case CLASS_SHAMAN:       return shaman[specNo];
            case CLASS_MAGE:         return mage[specNo];
            case CLASS_WARLOCK:      return warlock[specNo];
            case CLASS_DRUID:        return druid[specNo];
            default:                 return "Unknown";
        }
    }

    // Faction-ordered race iteration so the menu always presents races in the
    // same visual order regardless of how the enum happens to be numbered.
    std::vector<uint8> RacesForFaction(bool alliance)
    {
        if (alliance)
            return {RACE_HUMAN, RACE_DWARF, RACE_NIGHTELF, RACE_GNOME, RACE_DRAENEI};
        else
            return {RACE_ORC, RACE_UNDEAD_PLAYER, RACE_TAUREN, RACE_TROLL, RACE_BLOODELF};
    }

    // Class iteration order: same as the character-create UI.
    std::vector<uint8> AllPlayableClasses()
    {
        return {
            CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
            CLASS_PRIEST, CLASS_DEATH_KNIGHT, CLASS_SHAMAN, CLASS_MAGE,
            CLASS_WARLOCK, CLASS_DRUID
        };
    }

    // ---- Account / cap helpers ---------------------------------------------

    // Returns master account id. We treat the gossip caller's account as the
    // root of the bot family; alt accounts are derived from its username.
    uint32 MasterAccountId(Player* player) { return player->GetSession()->GetAccountId(); }

    std::string MasterAccountName(uint32 accountId)
    {
        std::string name;
        AccountMgr::GetName(accountId, name);
        return name;
    }

    // Suffix appended to master's username to derive alt-account usernames.
    // Uppercased because AccountMgr::CreateAccount uppercases everything it
    // stores (AccountMgr.cpp:54-56), and we want the LIKE match to line up.
    std::string AltUsername(std::string const& masterName, uint32 index)
    {
        std::ostringstream o;
        o << masterName << "_ALTS_" << index;
        std::string out = o.str();
        std::transform(out.begin(), out.end(), out.begin(), ::toupper);
        return out;
    }

    // Walk masterName_ALTS_1, _2, ... in order. Returns the list of (id, n)
    // for alt accounts that currently exist. Used both to count and to find a
    // landing spot for a new character.
    std::vector<std::pair<uint32, uint32>> EnumerateExistingAlts(std::string const& masterName)
    {
        std::vector<std::pair<uint32, uint32>> alts;
        // Walk a generous range; we stop after a gap to avoid scanning forever.
        uint32 misses = 0;
        for (uint32 n = 1; n <= 100 && misses < 3; ++n)
        {
            std::string altName = AltUsername(masterName, n);
            uint32 id = AccountMgr::GetId(altName);
            if (id)
            {
                alts.emplace_back(id, n);
                misses = 0;
            }
            else
            {
                ++misses;
            }
        }
        return alts;
    }

    // Total bots that belong to this master family (master account + all alts).
    uint32 CountTotalBots(uint32 masterAccountId, std::string const& masterName)
    {
        uint32 total = AccountMgr::GetCharactersCount(masterAccountId);
        // Master's *own* player toon counts in that number. Subtract one for
        // the human if they have a character on the master account (they
        // almost always do — they're talking to the NPC right now).
        if (total > 0)
            total -= 1;
        for (auto const& [id, n] : EnumerateExistingAlts(masterName))
            total += AccountMgr::GetCharactersCount(id);
        return total;
    }

    // ---- Faction / class / race legality -----------------------------------

    bool IsPlayerAlliance(Player* player) { return player->GetTeamId() == TEAM_ALLIANCE; }

    // Inline copy of RandomPlayerbotFactory::IsValidRaceClassCombination
    // (private in that header — keep the rule local to avoid touching upstream
    // visibility). Combined with the server-config disable masks below.
    bool RaceClassPlayable(uint8 race, uint8 cls)
    {
        uint32 expansion = sWorld->getIntConfig(CONFIG_EXPANSION);
        if (expansion < EXPANSION_THE_BURNING_CRUSADE &&
            (race == RACE_BLOODELF || race == RACE_DRAENEI))
            return false;
        if (expansion < EXPANSION_WRATH_OF_THE_LICH_KING && cls == CLASS_DEATH_KNIGHT)
            return false;
        if (!sObjectMgr->GetPlayerInfo(race, cls))
            return false;

        if ((1u << (race - 1)) & sWorld->getIntConfig(CONFIG_CHARACTER_CREATING_DISABLED_RACEMASK))
            return false;
        if ((1u << (cls - 1)) & sWorld->getIntConfig(CONFIG_CHARACTER_CREATING_DISABLED_CLASSMASK))
            return false;

        return true;
    }

    // ---- Random-appearance picker (same loop as RandomPlayerbotFactory) ----
    //
    // The CharacterCreateInfo "skin" field is actually the skin tint that
    // pairs with the chosen face — i.e. face.second, not a separate sample
    // from SECTION_TYPE_SKIN. RandomPlayerbotFactory does the same and even
    // leaves a "not used" comment on the standalone skinColor sample.
    struct Appearance { uint8 skin, face, hairStyle, hairColor, facialHair; };

    Appearance RollAppearance(uint8 race, uint8 gender)
    {
        std::vector<uint8> facialHairTypes;
        std::vector<std::pair<uint8, uint8>> faces, hairs;
        for (CharSectionsEntry const* cs : sCharSectionsStore)
        {
            if (cs->Race != race || cs->Gender != gender)
                continue;
            switch (cs->GenType)
            {
                case SECTION_TYPE_FACE:        faces.emplace_back(cs->Type, cs->Color); break;
                case SECTION_TYPE_FACIAL_HAIR: facialHairTypes.push_back(cs->Type); break;
                case SECTION_TYPE_HAIR:        hairs.emplace_back(cs->Type, cs->Color); break;
                default: break;
            }
        }

        Appearance a{};
        if (!faces.empty())
        {
            auto f = faces[urand(0, faces.size() - 1)];
            a.skin = f.second;
            a.face = f.first;
        }
        if (!hairs.empty())
        {
            auto h = hairs[urand(0, hairs.size() - 1)];
            a.hairStyle = h.first;
            a.hairColor = h.second;
        }

        bool excludeFacial = (race == RACE_TAUREN) || (race == RACE_DRAENEI) ||
            (gender == GENDER_FEMALE && race != RACE_NIGHTELF && race != RACE_UNDEAD_PLAYER);
        a.facialHair = excludeFacial || facialHairTypes.empty()
            ? 0 : facialHairTypes[urand(0, facialHairTypes.size() - 1)];
        return a;
    }

    // ---- Account selection / creation --------------------------------------
    //
    // Returns the account id to land the new character on, or 0 if we have to
    // create a new alt account first (in which case `outNeedsAsyncWait` is
    // set true and the user should be told to retry shortly).
    //
    // The "needs async wait" path is the cold path: creating an account hits
    // LoginDatabase asynchronously and the account id is not visible to
    // AccountMgr::GetId until the queue drains. Rather than block the world
    // thread, we kick off the create and ask the user to come back.
    uint32 PickOrCreateAccountForBot(Player* master, bool& outNeedsAsyncWait,
                                     std::string& outError)
    {
        outNeedsAsyncWait = false;
        outError.clear();

        uint32 masterAcc = MasterAccountId(master);
        std::string masterName = MasterAccountName(masterAcc);

        // Master account has space?
        if (AccountMgr::GetCharactersCount(masterAcc) < 10)
            return masterAcc;

        // Walk existing alts for free slots, in stable order.
        auto alts = EnumerateExistingAlts(masterName);
        std::sort(alts.begin(), alts.end(),
                  [](auto const& a, auto const& b) { return a.second < b.second; });
        for (auto const& [id, n] : alts)
            if (AccountMgr::GetCharactersCount(id) < 10)
                return id;

        // All existing alts full. Need a new one — but check the alt cap first.
        if (alts.size() >= CfgMaxAltAccounts())
        {
            outError = "You have hit the alt-account cap. Dismiss or delete some bots first.";
            return 0;
        }

        // Find next free index.
        uint32 nextN = 1;
        for (auto const& [id, n] : alts)
            nextN = std::max(nextN, n + 1);

        std::string altName = AltUsername(masterName, nextN);
        if (altName.size() > MAX_ACCOUNT_STR)
        {
            outError = "Your account name is too long to derive an alt — sorry.";
            return 0;
        }

        // 17-char password cap (MAX_PASS_STR = 16); config default "tbots" fits.
        std::string altPass = CfgAltAccountPassword();
        if (altPass.size() > MAX_PASS_STR)
            altPass = altPass.substr(0, MAX_PASS_STR);

        AccountOpResult r = sAccountMgr->CreateAccount(altName, altPass);
        if (r != AOR_OK)
        {
            outError = "Failed to create alt account (error code).";
            return 0;
        }

        // CreateAccount writes async; the new account isn't visible to
        // AccountMgr::GetId yet. Ask the user to come back in a moment.
        outNeedsAsyncWait = true;
        return 0;
    }

    // ---- Linking helper ----------------------------------------------------
    void LinkAlt(uint32 masterAcc, uint32 altAcc)
    {
        if (masterAcc == altAcc) return;
        PlayerbotsDatabase.Execute(
            "INSERT IGNORE INTO playerbots_account_links (account_id, linked_account_id) VALUES ({}, {})",
            masterAcc, altAcc);
        PlayerbotsDatabase.Execute(
            "INSERT IGNORE INTO playerbots_account_links (account_id, linked_account_id) VALUES ({}, {})",
            altAcc, masterAcc);
    }

    // ---- Menu renderers ----------------------------------------------------

    void ShowHello(Player* player, Creature* creature);

    void ShowRacePicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        for (uint8 race : RacesForFaction(IsPlayerAlliance(player)))
        {
            if ((1u << (race - 1)) & sWorld->getIntConfig(CONFIG_CHARACTER_CREATING_DISABLED_RACEMASK))
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, RaceName(race),
                             SENDER_PICK_RACE, race);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_PICK_RACE, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_PICK_RACE, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    void ShowClassPicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        auto it = g_pendingMenu.find(player->GetGUID());
        uint8 race = (it != g_pendingMenu.end()) ? it->second.race : 0;

        uint32 shown = 0;
        for (uint8 cls : AllPlayableClasses())
        {
            if (!RaceClassPlayable(race, cls))
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, ClassName(cls),
                             SENDER_PICK_CLASS, cls);
            ++shown;
        }
        if (shown == 0)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "No classes available for that race.",
                             SENDER_PICK_CLASS, ACTION_BACK);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_PICK_CLASS, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_PICK_CLASS, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    void ShowSpecPicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        auto it = g_pendingMenu.find(player->GetGUID());
        uint8 cls = (it != g_pendingMenu.end()) ? it->second.cls : 0;

        for (uint8 specNo = 0; specNo < 3; ++specNo)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, SpecName(cls, specNo),
                             SENDER_PICK_SPEC, SPEC_ACTION_BIAS + specNo);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_PICK_SPEC, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_PICK_SPEC, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    void ShowGenderPicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Male",
                         SENDER_PICK_GENDER, GENDER_MALE + 1);    // +1: 0 is unsafe
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Female",
                         SENDER_PICK_GENDER, GENDER_FEMALE + 1);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_PICK_GENDER, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_PICK_GENDER, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    void ShowNamePrompt(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        auto it = g_pendingMenu.find(player->GetGUID());
        std::ostringstream summary;
        if (it != g_pendingMenu.end())
        {
            PendingMenu const& p = it->second;
            summary << (p.gender == GENDER_MALE ? "Male " : "Female ")
                    << RaceName(p.race) << " " << SpecName(p.cls, p.specNo)
                    << " " << ClassName(p.cls) << " — name?";
        }
        else
        {
            summary << "Name?";
        }

        // coded=true opens the client's text-input dialog. The typed string
        // arrives in OnGossipSelectCode as the `code` arg.
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         "Type a name for your bot (1-12 characters)",
                         SENDER_NAME_PROMPT, 1,
                         summary.str(), 0, true);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_NAME_PROMPT, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_NAME_PROMPT, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    void ShowHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        uint32 masterAcc = MasterAccountId(player);
        std::string masterName = MasterAccountName(masterAcc);
        uint32 totalBots = CountTotalBots(masterAcc, masterName);
        uint32 altCount  = uint32(EnumerateExistingAlts(masterName).size());

        std::ostringstream summary;
        summary << "Status: " << totalBots << " / " << CfgMaxBotsPerMaster() << " bots, "
                << altCount << " / " << CfgMaxAltAccounts() << " alt accounts.";
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, summary.str(),
                         SENDER_HELLO, ACTION_CLOSE);

        if (totalBots < CfgMaxBotsPerMaster())
        {
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Create a new bot ->",
                             SENDER_HELLO, ACTION_HELLO);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "You have reached the per-master bot cap. Dismiss some first.",
                             SENDER_HELLO, ACTION_CLOSE);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_HELLO, ACTION_CLOSE);
        SendGossipMenuFor(player, BOTCREATOR_TEXT_ID, creature->GetGUID());
    }

    // ---- The actual creation step ------------------------------------------

    // Returns the new bot's GUID on success, or an empty ObjectGuid on failure.
    // On failure, `outError` carries a user-facing message.
    ObjectGuid TryCreateBot(Player* master, PendingMenu const& choices,
                            std::string const& name, std::string& outError)
    {
        outError.clear();
        ChatHandler dbg(master->GetSession());
        dbg.PSendSysMessage("[botcreator] TryCreateBot start name='{}' race={} class={} spec={} gender={}",
                            name, choices.race, choices.cls, choices.specNo, choices.gender);

        // 1. Name validation — same checks as the official create-char path.
        std::string n = name;
        if (!normalizePlayerName(n))
        {
            outError = "That name has invalid characters.";
            dbg.PSendSysMessage("[botcreator] FAIL: normalizePlayerName");
            return ObjectGuid::Empty;
        }
        if (ObjectMgr::CheckPlayerName(n, true) != CHAR_NAME_SUCCESS)
        {
            outError = "That name is too long, too short, reserved, or has profanity.";
            dbg.PSendSysMessage("[botcreator] FAIL: CheckPlayerName");
            return ObjectGuid::Empty;
        }
        // Already-taken?
        if (sCharacterCache->GetCharacterGuidByName(n))
        {
            outError = "That name is already taken.";
            dbg.PSendSysMessage("[botcreator] FAIL: name already taken");
            return ObjectGuid::Empty;
        }
        dbg.PSendSysMessage("[botcreator] name OK -> '{}'", n);

        // 2. Cap re-check (the user could have been at the limit when they
        // opened the menu but added more bots in another window, or vice versa).
        uint32 masterAcc = MasterAccountId(master);
        std::string masterName = MasterAccountName(masterAcc);
        if (CountTotalBots(masterAcc, masterName) >= CfgMaxBotsPerMaster())
        {
            outError = "You have hit the per-master bot cap.";
            dbg.PSendSysMessage("[botcreator] FAIL: bot cap hit");
            return ObjectGuid::Empty;
        }

        // 3. Pick or create the target account.
        bool needsAsync = false;
        uint32 targetAcc = PickOrCreateAccountForBot(master, needsAsync, outError);
        if (needsAsync)
        {
            outError = "Preparing a new alt account — please try again in 5 seconds.";
            dbg.PSendSysMessage("[botcreator] FAIL: alt account being created async, retry");
            return ObjectGuid::Empty;
        }
        if (!targetAcc)
        {
            if (outError.empty())
                outError = "No account slot available.";
            dbg.PSendSysMessage("[botcreator] FAIL: no targetAcc, err='{}'", outError);
            return ObjectGuid::Empty;
        }
        dbg.PSendSysMessage("[botcreator] targetAcc={} (master={})", targetAcc, masterAcc);

        // Auto-link if the bot is going on an alt account.
        if (targetAcc != masterAcc)
            LinkAlt(masterAcc, targetAcc);

        // 4. Build the Player object on a synthetic WorldSession bound to the
        // *target* account (mirrors RandomPlayerbotFactory.cpp:700-701).
        WorldSession* session = new WorldSession(
            targetAcc, "", 0x0, nullptr, SEC_PLAYER,
            EXPANSION_WRATH_OF_THE_LICH_KING,
            time_t(0), LOCALE_enUS, 0, false, false, 0, true);

        Appearance app = RollAppearance(choices.race, choices.gender);

        std::unique_ptr<CharacterCreateInfo> info = std::make_unique<CharacterCreateInfo>(
            n, choices.race, choices.cls, choices.gender,
            app.skin, app.face, app.hairStyle, app.hairColor, app.facialHair);

        Player* bot = new Player(session);
        bot->GetMotionMaster()->Initialize();
        ObjectGuid::LowType lowGuid = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();
        if (!bot->Create(lowGuid, info.get()))
        {
            outError = "Player::Create failed (race/class combo or DBC error).";
            dbg.PSendSysMessage("[botcreator] FAIL: Player::Create returned false");
            bot->CleanupsBeforeDelete();
            delete bot;
            delete session;
            return ObjectGuid::Empty;
        }
        dbg.PSendSysMessage("[botcreator] Player::Create OK lowGuid={}", lowGuid);

        bot->setCinematic(2);
        bot->SetAtLoginFlag(AT_LOGIN_NONE);
        if (choices.cls == CLASS_DEATH_KNIGHT)
            bot->learnSpell(50977, false);  // DK starter

        // 5. Persist. SaveToDB(true, false) issues the INSERT into characters;
        // the cache entry is what makes ObjectMgr name lookup work afterwards.
        bot->SaveToDB(true, false);
        sCharacterCache->AddCharacterCacheEntry(
            bot->GetGUID(), targetAcc, bot->GetName(),
            bot->getGender(), bot->getRace(), bot->getClass(), bot->GetLevel());
        dbg.PSendSysMessage("[botcreator] SaveToDB + cache entry done");

        ObjectGuid botGuid = bot->GetGUID();

        // 6. Queue level/spec/gear apply for when the bot logs in.
        g_pendingApply[botGuid] = PendingApply{ master->GetLevel(), choices.specNo };

        // 7. Tear down the temporary objects. The bot's permanent Player
        // object will be reconstructed by AddPlayerBot's async loader.
        bot->CleanupsBeforeDelete();
        delete bot;
        delete session;

        // 8. Summon. AddPlayerBot is async-load; our OnLogin script will fire
        // once the bot is in-world and consume the g_pendingApply entry.
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
        if (!mgr)
        {
            dbg.PSendSysMessage("[botcreator] FAIL: GET_PLAYERBOT_MGR returned null");
            return ObjectGuid::Empty;
        }
        dbg.PSendSysMessage("[botcreator] calling AddPlayerBot...");
        mgr->AddPlayerBot(botGuid, masterAcc);
        dbg.PSendSysMessage("[botcreator] AddPlayerBot returned (async; bot will log in shortly)");

        return botGuid;
    }

}  // namespace

// ---------------------------------------------------------------------------
// CreatureScript: the NPC itself.
// ---------------------------------------------------------------------------
class npc_botcreator : public CreatureScript
{
public:
    npc_botcreator() : CreatureScript("npc_botcreator") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        g_pendingMenu.erase(player->GetGUID());  // reset wizard state on entry
        ShowHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 sender, uint32 action) override
    {
        if (action == ACTION_CLOSE) { CloseGossipMenuFor(player); return true; }
        if (action == ACTION_HELLO) { ShowRacePicker(player, creature); return true; }

        switch (sender)
        {
            case SENDER_PICK_RACE:
                if (action == ACTION_BACK) { ShowHello(player, creature); return true; }
                g_pendingMenu[player->GetGUID()].race = uint8(action);
                ShowClassPicker(player, creature);
                return true;

            case SENDER_PICK_CLASS:
                if (action == ACTION_BACK) { ShowRacePicker(player, creature); return true; }
                g_pendingMenu[player->GetGUID()].cls = uint8(action);
                ShowSpecPicker(player, creature);
                return true;

            case SENDER_PICK_SPEC:
                if (action == ACTION_BACK) { ShowClassPicker(player, creature); return true; }
                g_pendingMenu[player->GetGUID()].specNo = int8(action - SPEC_ACTION_BIAS);
                ShowGenderPicker(player, creature);
                return true;

            case SENDER_PICK_GENDER:
                if (action == ACTION_BACK) { ShowSpecPicker(player, creature); return true; }
                g_pendingMenu[player->GetGUID()].gender = uint8(action - 1);
                ShowNamePrompt(player, creature);
                return true;

            case SENDER_NAME_PROMPT:
                if (action == ACTION_BACK) { ShowGenderPicker(player, creature); return true; }
                // Any non-control click here should re-open the name prompt.
                ShowNamePrompt(player, creature);
                return true;

            case SENDER_RETRY:
                // From an error-state "try a different name" prompt.
                ShowNamePrompt(player, creature);
                return true;

            default:
                CloseGossipMenuFor(player);
                return true;
        }
    }

    // Fires when the user submits the coded-input popup.
    bool OnGossipSelectCode(Player* player, Creature* creature,
                            uint32 sender, uint32 action,
                            char const* code) override
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "[botcreator] OnGossipSelectCode fired: sender={} action={} code='{}'",
            sender, action, code ? code : "(null)");

        if (sender != SENDER_NAME_PROMPT)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        auto it = g_pendingMenu.find(player->GetGUID());
        if (it == g_pendingMenu.end())
        {
            ChatHandler(player->GetSession())
                .PSendSysMessage("Wizard state lost — please start over.");
            ShowHello(player, creature);
            return true;
        }

        std::string name = code ? std::string(code) : std::string();
        std::string err;
        ObjectGuid newGuid = TryCreateBot(player, it->second, name, err);
        if (!newGuid)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Could not create bot: {}", err);
            // Leave the wizard state intact so they can try a different name.
            ShowNamePrompt(player, creature);
            return true;
        }

        g_pendingMenu.erase(it);

        ChatHandler(player->GetSession()).PSendSysMessage(
            "Summoning {} — they'll arrive in a moment and be set to level {} "
            "with talents applied.", name.c_str(), uint32(player->GetLevel()));

        CloseGossipMenuFor(player);
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerScript: applies the queued level/spec/gear when the new bot logs in.
//
// The bot's first login happens asynchronously after AddPlayerBot completes
// its DB load. By then we have a real, in-world Player* — only point at which
// PlayerbotFactory's Randomize and InitTalentsBySpecNo can run cleanly.
// ---------------------------------------------------------------------------
class BotCreatorApplyOnLoginScript : public PlayerScript
{
public:
    BotCreatorApplyOnLoginScript()
        : PlayerScript("BotCreatorApplyOnLoginScript", { PLAYERHOOK_ON_LOGIN }) {}

    void OnPlayerLogin(Player* bot) override
    {
        auto it = g_pendingApply.find(bot->GetGUID());
        if (it == g_pendingApply.end())
            return;

        PendingApply apply = it->second;
        g_pendingApply.erase(it);

        // Level the bot up to match the master's level at creation time.
        if (apply.level > bot->GetLevel())
            bot->GiveLevel(apply.level);

        // Gear, skills, spells. The factory is the same path the random-bot
        // pipeline uses post-login.
        PlayerbotFactory factory(bot, apply.level);
        factory.Randomize(false);

        // Talents are reset and re-applied to the chosen spec last so the
        // factory's default-spec choice doesn't win.
        if (apply.specNo >= 0)
            PlayerbotFactory::InitTalentsBySpecNo(bot, apply.specNo, true);
    }
};

void AddSC_npc_botcreator()
{
    new npc_botcreator();
    new BotCreatorApplyOnLoginScript();
}
