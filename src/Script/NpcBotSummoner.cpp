/*
 * Custom CreatureScript: Botmaster NPC (entry 600001)
 *
 * Lists all characters on the player's account (minus the player themselves)
 * as gossip options. Clicking a name calls PlayerbotMgr::AddPlayerBot with
 * the master's account id, which is exactly what `.playerbots bot add <name>`
 * does. An extra footer option logs out every currently-summoned bot.
 */

// MSVC 14.38 <chrono> ICE workaround — see EmblemVendorCache.cpp.
#include <chrono>

#include "Playerbots.h"

#include "CharacterCache.h"
#include "Chat.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "EmblemShopMgr.h"
#include "EmblemShopUpgrades.h"
#include "EmblemVendorCache.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

#include <cmath>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint32 BOTMASTER_ENTRY        = 600001;
    constexpr uint32 BOTMASTER_TEXT_ID      = 60002;
    constexpr uint32 MAX_BOTS_SHOWN         = 24;

    // Action ID space:
    //   GOSSIP_ACTION_INFO_DEF + <lowGuid>      -> summon that bot
    //   ACTION_DISMISS_ALL                       -> log out every summoned bot
    //   ACTION_CLOSE                             -> close the menu
    constexpr uint32 ACTION_DISMISS_ALL     = GOSSIP_ACTION_INFO_DEF + 0xFFFFFD;
    constexpr uint32 ACTION_CLOSE           = GOSSIP_ACTION_INFO_DEF + 0xFFFFFF;
    constexpr uint32 ACTION_NOOP            = GOSSIP_ACTION_INFO_DEF + 0xFFFFFE;

    char const* ClassName(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:      return "Warrior";
            case CLASS_PALADIN:      return "Paladin";
            case CLASS_HUNTER:       return "Hunter";
            case CLASS_ROGUE:        return "Rogue";
            case CLASS_PRIEST:       return "Priest";
            case CLASS_SHAMAN:       return "Shaman";
            case CLASS_MAGE:         return "Mage";
            case CLASS_WARLOCK:      return "Warlock";
            case CLASS_DRUID:        return "Druid";
            case CLASS_DEATH_KNIGHT: return "Death Knight";
            default:                 return "Unknown";
        }
    }
}

class npc_botmaster : public CreatureScript
{
public:
    npc_botmaster() : CreatureScript("npc_botmaster") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        uint32 accountId = player->GetSession()->GetAccountId();
        std::string masterName = player->GetName();

        // Already-summoned bots for this master (to mark them in the list).
        std::set<ObjectGuid> onlineBots;
        if (PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player))
        {
            for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
                onlineBots.insert(it->first);
        }

        // Build the list of account IDs to source bots from: master + linked.
        // The linked-accounts table lives in the playerbots DB and is keyed
        // bidirectionally, so a single query gets every account linked to us.
        // Gated on allowTrustedAccountBots to mirror PlayerbotMgr's own
        // addaccount security check (PlayerbotMgr.cpp:702).
        std::ostringstream accountIn;
        accountIn << accountId;
        if (sPlayerbotAIConfig.allowTrustedAccountBots)
        {
            QueryResult linkResult = PlayerbotsDatabase.Query(
                "SELECT linked_account_id FROM playerbots_account_links WHERE account_id = {}",
                accountId);
            if (linkResult)
            {
                do
                {
                    accountIn << "," << linkResult->Fetch()[0].Get<uint32>();
                }
                while (linkResult->NextRow());
            }
        }

        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, name, class, level FROM characters "
            "WHERE account IN ({}) AND name <> '{}' "
            "ORDER BY level DESC, name ASC",
            accountIn.str(), masterName);

        uint32 shown = 0;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                ObjectGuid::LowType lowGuid = fields[0].Get<uint32>();
                std::string botName        = fields[1].Get<std::string>();
                uint8 botClass             = fields[2].Get<uint8>();
                uint8 botLevel             = fields[3].Get<uint8>();

                ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
                bool isOnline = onlineBots.count(botGuid) > 0
                              || ObjectAccessor::FindConnectedPlayer(botGuid) != nullptr;

                std::ostringstream label;
                label << botName
                      << " (Lv " << uint32(botLevel) << " " << ClassName(botClass) << ")";
                if (isOnline)
                    label << "  [in world]";

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, label.str(),
                                 GOSSIP_SENDER_MAIN,
                                 GOSSIP_ACTION_INFO_DEF + lowGuid);

                if (++shown >= MAX_BOTS_SHOWN)
                    break;
            }
            while (result->NextRow());
        }

        if (shown == 0)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "You have no other characters on this account.",
                             GOSSIP_SENDER_MAIN, ACTION_NOOP);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "Dismiss all my summoned bots",
                             GOSSIP_SENDER_MAIN, ACTION_DISMISS_ALL);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         GOSSIP_SENDER_MAIN, ACTION_CLOSE);

        SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/,
                        uint32 /*sender*/, uint32 action) override
    {
        if (action == ACTION_CLOSE || action == ACTION_NOOP)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        ChatHandler handler(player->GetSession());

        if (action == ACTION_DISMISS_ALL)
        {
            PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
            if (!mgr)
            {
                handler.PSendSysMessage("Bot system is not available for you yet.");
                CloseGossipMenuFor(player);
                return true;
            }

            std::vector<ObjectGuid> toLogout;
            for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
                toLogout.push_back(it->first);

            for (ObjectGuid g : toLogout)
                mgr->LogoutPlayerBot(g);

            handler.PSendSysMessage("Dismissed {} bot(s).", uint32(toLogout.size()));
            CloseGossipMenuFor(player);
            return true;
        }

        // Otherwise: action encodes the bot's low GUID.
        ObjectGuid::LowType lowGuid = action - GOSSIP_ACTION_INFO_DEF;
        ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);

        std::string botName;
        sCharacterCache->GetCharacterNameByGuid(botGuid, botName);
        if (botName.empty())
            botName = "(unknown)";

        // Re-verify account ownership at click time (defense in depth — the
        // gossip list could be stale if the player swaps account-linked toons).
        // Accept master's own account OR a trusted-linked account, same rule
        // mod-playerbots itself applies to `.playerbots bot addaccount`.
        uint32 botAccount    = sCharacterCache->GetCharacterAccountIdByGuid(botGuid);
        uint32 masterAccount = player->GetSession()->GetAccountId();
        if (botAccount != masterAccount)
        {
            PlayerbotMgr* botMgr = GET_PLAYERBOT_MGR(player);
            bool linked = sPlayerbotAIConfig.allowTrustedAccountBots && botMgr
                       && botMgr->IsAccountLinked(masterAccount, botAccount);
            if (!linked)
            {
                handler.PSendSysMessage("{} is not on your account or any linked account.", botName);
                CloseGossipMenuFor(player);
                return true;
            }
        }

        if (ObjectAccessor::FindConnectedPlayer(botGuid))
        {
            handler.PSendSysMessage("{} is already in the world.", botName);
            CloseGossipMenuFor(player);
            return true;
        }

        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
        if (!mgr)
        {
            handler.PSendSysMessage("Bot system is not available for you yet.");
            CloseGossipMenuFor(player);
            return true;
        }

        mgr->AddPlayerBot(botGuid, masterAccount);
        handler.PSendSysMessage("Summoning {}...", botName);
        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_npc_botmaster()
{
    new npc_botmaster();
}
