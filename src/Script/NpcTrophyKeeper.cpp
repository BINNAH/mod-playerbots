/*
 * Custom CreatureScript: Gimped Trophy Keeper (entry 600003)
 *
 * Shows the player their boss kill counts via a tier-organized gossip menu.
 * Tiers: 7 (Naxxramas), 8 (Ulduar), 9 (Trial of the Crusader),
 *        10 (Icecrown Citadel). Adding more tiers = extend the Tiers() array.
 *
 * Data source: AC's achievement criteria store. The "Statistics" tab in the
 * in-game Achievements UI is fed by the same data. For each boss we ask
 * sAchievementMgr->GetSpecialAchievementCriteriaByType(KILL_CREATURE, id)
 * for the criteria that count kills of that creature.
 *
 * Counting model: each KILL_CREATURE kill increments *every* matching
 * criterion's counter by 1 (PROGRESS_ACCUMULATE, miscValue2=1). A single
 * raid boss is referenced by multiple criteria (boss-specific stat + the
 * raid-completion sub-criterion + sometimes more), so summing across
 * criteria multiplies the real count. The data layout below splits each
 * boss into "kill credit groups" — each group is a set of creature IDs
 * whose deaths constitute ONE encounter completion (same difficulty,
 * possibly multiple units that die together). We take MAX within a group
 * and SUM across groups.
 *
 * Examples:
 *   Single-unit Naxx boss with 10p and 25p difficulty entries
 *     -> 2 groups: { {10p_id}, {25p_id} }, sum gives total kills.
 *   Four Horsemen (4 units die together) per difficulty
 *     -> 2 groups: { {4 ids}, {4 ids} }, MAX within = encounter count,
 *        sum gives total kills.
 *   ToC / ICC boss with 4 difficulty entries (10N/25N/10H/25H)
 *     -> 4 groups of 1 id each, sum gives total kills across all modes.
 *
 * Lives in mod-playerbots/Script just because that module is already wired
 * into the build; the script itself doesn't touch any playerbots API.
 */

#include "AchievementMgr.h"
#include "Chat.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

namespace
{
    constexpr uint32 TROPHY_KEEPER_TEXT_ID  = 60008;

    // Action ID layout (must not collide with each other; values are arbitrary
    // as long as they're stable):
    constexpr uint32 ACTION_BACK            = GOSSIP_ACTION_INFO_DEF + 1000;
    constexpr uint32 ACTION_CLOSE           = GOSSIP_ACTION_INFO_DEF + 1001;
    constexpr uint32 ACTION_TIER_BASE       = GOSSIP_ACTION_INFO_DEF + 100;   // + tierIdx
    constexpr uint32 ACTION_BOSS_BASE       = GOSSIP_ACTION_INFO_DEF + 10000; // + tierIdx*256 + bossIdx

    struct BossEntry
    {
        char const* displayName;
        // Each inner vector is a "kill credit group": a set of creature
        // IDs whose deaths fire together for ONE encounter completion
        // (multi-unit fights like Four Horsemen, or a single boss in one
        // difficulty). MAX within a group, SUM across groups.
        std::vector<std::vector<uint32>> groups;
    };

    struct TierEntry
    {
        char const* displayName;
        std::vector<BossEntry> bosses;
    };

    // -------------------------------------------------------------------
    // Tier 7 -- Naxxramas (10p / 25p)
    // -------------------------------------------------------------------
    // Four Horsemen: WotLK replaces Mograine with Baron Rivendare and uses
    // four real units (Rivendare/Zeliek/Korth'azz/Blaumeux) per difficulty.
    // All four die when the encounter ends, so listing all four in one
    // group and taking MAX gives the encounter kill count.
    TierEntry const& Naxx()
    {
        static const TierEntry t = {
            "Tier 7 -- Naxxramas",
            {
                { "Anub'Rekhan",            { {15956}, {29249} } },
                { "Grand Widow Faerlina",   { {15953}, {29268} } },
                { "Maexxna",                { {15952}, {29278} } },
                { "Noth the Plaguebringer", { {15954}, {29615} } },
                { "Heigan the Unclean",     { {15936}, {29701} } },
                { "Loatheb",                { {16011}, {29718} } },
                { "Instructor Razuvious",   { {16061}, {29940} } },
                { "Gothik the Harvester",   { {16060}, {29955} } },
                { "The Four Horsemen",      { {30549, 16063, 16064, 16065},
                                              {30600, 30602, 30603, 30601} } },
                { "Patchwerk",              { {16028}, {29324} } },
                { "Grobbulus",              { {15931}, {29373} } },
                { "Gluth",                  { {15932}, {29417} } },
                { "Thaddius",               { {15928}, {29448} } },
                { "Sapphiron",              { {15989}, {29991} } },
                { "Kel'Thuzad",             { {15990}, {30061} } },
            }
        };
        return t;
    }

    // -------------------------------------------------------------------
    // Tier 8 -- Ulduar (10p / 25p)
    // -------------------------------------------------------------------
    // Assembly of Iron (Iron Council): 3 council members (Steelbreaker,
    // Molgeim, Brundir) all die per kill. Same group treatment as Four
    // Horsemen.
    //
    // Mimiron: the encounter has 4 phase units (Mimiron 33350, Leviathan
    // MkII 33432, VX-001 33651, Aerial Command Unit 33670). AC's code
    // manually fires KILL_CREATURE for Leviathan MkII (33432) when the
    // encounter completes (boss_mimiron.cpp:732), which is the canonical
    // kill credit. We use just that.
    TierEntry const& Ulduar()
    {
        static const TierEntry t = {
            "Tier 8 -- Ulduar",
            {
                { "Flame Leviathan",        { {33113}, {34003} } },
                { "Ignis the Furnace Master", { {33118}, {33190} } },
                { "Razorscale",             { {33186}, {33724} } },
                { "XT-002 Deconstructor",   { {33293}, {33885} } },
                { "Assembly of Iron",       { {32867, 32927, 32857},
                                              {33693, 33692, 33694} } },
                { "Kologarn",               { {32930}, {33909} } },
                { "Auriaya",                { {33515}, {34175} } },
                { "Hodir",                  { {32845}, {32846} } },
                { "Thorim",                 { {32865}, {33147} } },
                { "Freya",                  { {32906}, {33360} } },
                { "Mimiron",                { {33432}, {34106} } },
                { "General Vezax",          { {33271}, {33449} } },
                { "Yogg-Saron",             { {33288}, {33955} } },
                { "Algalon the Observer",   { {32871}, {33070} } },
            }
        };
        return t;
    }

    // -------------------------------------------------------------------
    // Tier 9 -- Trial of the Crusader (10N / 25N / 10H / 25H)
    // -------------------------------------------------------------------
    // ToC introduces 4 separate creature difficulty entries per boss
    // (normal + heroic for each raid size), so most bosses get 4 groups.
    //
    // Northrend Beasts: one encounter, three phases — Gormok dies, then
    // Acidmaw + Dreadscale, then Icehowl. All four units die per encounter
    // completion, so we list them in one group per difficulty.
    //
    // Twin Val'kyr: both Lightbane and Darkbane die per kill, one group
    // per difficulty.
    //
    // Faction Champions intentionally omitted: kill credit fires via a
    // self-cast spell (68184) on Tirion, not via KILL_CREATURE on any
    // single unit, so the in-game Statistics tab is the cleanest source
    // for that one.
    TierEntry const& ToC()
    {
        static const TierEntry t = {
            "Tier 9 -- Trial of the Crusader",
            {
                { "Northrend Beasts",       { {34796, 35144, 34799, 34797},
                                              {35438, 35511, 35514, 35447},
                                              {35439, 35512, 35515, 35448},
                                              {35440, 35513, 35516, 35449} } },
                { "Lord Jaraxxus",          { {34780}, {35216}, {35268}, {35269} } },
                { "Twin Val'kyr",           { {34497, 34496},
                                              {35350, 35347},
                                              {35351, 35348},
                                              {35352, 35349} } },
                { "Anub'arak",              { {34564}, {34566}, {35615}, {35616} } },
            }
        };
        return t;
    }

    // -------------------------------------------------------------------
    // Tier 10 -- Icecrown Citadel (10N / 25N / 10H / 25H)
    // -------------------------------------------------------------------
    // Gunship Battle: the encounter ends when the enemy commander dies
    // (Muradin Bronzebeard for Horde players, High Overlord Saurfang for
    // Alliance). We list both per difficulty; for a solo player only one
    // ever counts.
    //
    // Blood Prince Council: Valanar, Keleseth, Taldaram all die per kill.
    //
    // Valithria Dreamwalker: it's a healing encounter; AC fires kill
    // credit when she's healed to 100%. Difficulty entries exist for 10N
    // (36789) and 25N (38174); 10H/25H reuse separate entries 38589 /
    // 38590 (the (1)-suffixed creature_template rows).
    TierEntry const& ICC()
    {
        static const TierEntry t = {
            "Tier 10 -- Icecrown Citadel",
            {
                { "Lord Marrowgar",         { {36612}, {37957}, {37958}, {37959} } },
                { "Lady Deathwhisper",      { {36855}, {38106}, {38296}, {38297} } },
                { "Icecrown Gunship Battle", { {36948, 36939},
                                              {38157, 38156},
                                              {38639, 38637},
                                              {38640, 38638} } },
                { "Deathbringer Saurfang",  { {37813}, {38402}, {38582}, {38583} } },
                { "Festergut",              { {36626}, {37504}, {37505}, {37506} } },
                { "Rotface",                { {36627}, {38390}, {38549}, {38550} } },
                { "Professor Putricide",    { {36678}, {38431}, {38585}, {38586} } },
                { "Blood Prince Council",   { {37970, 37972, 37973},
                                              {38401, 38399, 38400},
                                              {38784, 38769, 38771},
                                              {38785, 38770, 38772} } },
                { "Blood-Queen Lana'thel",  { {37955}, {38434}, {38435}, {38436} } },
                { "Valithria Dreamwalker",  { {36789}, {38174}, {38589}, {38590} } },
                { "Sindragosa",             { {36853}, {38265}, {38266}, {38267} } },
                { "The Lich King",          { {36597}, {39166}, {39167}, {39168} } },
            }
        };
        return t;
    }

    std::vector<TierEntry const*> const& Tiers()
    {
        static const std::vector<TierEntry const*> v = {
            &Naxx(), &Ulduar(), &ToC(), &ICC()
        };
        return v;
    }

    // Within a single kill-credit group, take MAX across criteria for any
    // listed creature ID. Each encounter completion increments every
    // matching criterion by 1, so MAX is the true count; for multi-unit
    // encounters whose units all die together, MAX across units is also
    // the encounter count (not sum, which would multiply).
    uint32 GetMaxKillsForGroup(std::vector<uint32> const& ids, AchievementMgr* mgr)
    {
        uint32 best = 0;
        for (uint32 creatureId : ids)
        {
            AchievementCriteriaEntryList const* list =
                sAchievementMgr->GetSpecialAchievementCriteriaByType(
                    ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE, creatureId);
            if (!list)
                continue;

            for (AchievementCriteriaEntry const* criterion : *list)
                if (CriteriaProgress* prog = mgr->GetCriteriaProgress(criterion))
                    if (prog->counter > best)
                        best = prog->counter;
        }
        return best;
    }

    uint32 GetBossKills(Player* player, BossEntry const& boss)
    {
        AchievementMgr* mgr = player->GetAchievementMgr();
        if (!mgr)
            return 0;

        uint32 total = 0;
        for (std::vector<uint32> const& group : boss.groups)
            total += GetMaxKillsForGroup(group, mgr);
        return total;
    }

    void ShowRootMenu(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        std::vector<TierEntry const*> const& tiers = Tiers();
        for (size_t i = 0; i < tiers.size(); ++i)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, tiers[i]->displayName,
                             GOSSIP_SENDER_MAIN, ACTION_TIER_BASE + uint32(i));

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         GOSSIP_SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player, TROPHY_KEEPER_TEXT_ID, creature->GetGUID());
    }

    void ShowTierMenu(Player* player, Creature* creature, uint32 tierIdx)
    {
        std::vector<TierEntry const*> const& tiers = Tiers();
        if (tierIdx >= tiers.size())
        {
            ShowRootMenu(player, creature);
            return;
        }

        TierEntry const& tier = *tiers[tierIdx];
        ClearGossipMenuFor(player);
        for (size_t i = 0; i < tier.bosses.size(); ++i)
        {
            BossEntry const& b = tier.bosses[i];
            uint32 count = GetBossKills(player, b);

            std::ostringstream label;
            label << b.displayName << "   [" << count << "]";
            // Action encodes both tier and boss index so OnGossipSelect
            // can route back without per-player state.
            uint32 action = ACTION_BOSS_BASE + (tierIdx * 256) + uint32(i);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label.str(),
                             GOSSIP_SENDER_MAIN, action);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<-- Back",
                         GOSSIP_SENDER_MAIN, ACTION_BACK);
        SendGossipMenuFor(player, TROPHY_KEEPER_TEXT_ID, creature->GetGUID());
    }
}

class npc_trophy_keeper : public CreatureScript
{
public:
    npc_trophy_keeper() : CreatureScript("npc_trophy_keeper") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowRootMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 /*sender*/, uint32 action) override
    {
        if (action == ACTION_CLOSE)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (action == ACTION_BACK)
        {
            ShowRootMenu(player, creature);
            return true;
        }

        if (action >= ACTION_TIER_BASE && action < ACTION_TIER_BASE + 256)
        {
            uint32 tierIdx = action - ACTION_TIER_BASE;
            ShowTierMenu(player, creature, tierIdx);
            return true;
        }

        if (action >= ACTION_BOSS_BASE)
        {
            uint32 packed = action - ACTION_BOSS_BASE;
            uint32 tierIdx = packed / 256;
            uint32 bossIdx = packed % 256;

            std::vector<TierEntry const*> const& tiers = Tiers();
            if (tierIdx < tiers.size() && bossIdx < tiers[tierIdx]->bosses.size())
            {
                BossEntry const& b = tiers[tierIdx]->bosses[bossIdx];
                uint32 count = GetBossKills(player, b);
                ChatHandler handler(player->GetSession());
                if (count == 0)
                    handler.PSendSysMessage("|cff00ff00{}|r -- no kills yet.",
                                             b.displayName);
                else
                    handler.PSendSysMessage("|cff00ff00{}|r -- {} kill{} (all difficulties combined).",
                                             b.displayName, count,
                                             count == 1 ? "" : "s");
                // Re-show the tier menu so counts refresh and the user can keep browsing.
                ShowTierMenu(player, creature, tierIdx);
                return true;
            }
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_npc_trophy_keeper()
{
    new npc_trophy_keeper();
}
