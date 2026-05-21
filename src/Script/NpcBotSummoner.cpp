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
    //   ACTION_SUMMON_ALL                        -> summon every offline bot
    //   ACTION_DISMISS_ALL                       -> log out every summoned bot
    //   ACTION_CLOSE                             -> close the menu
    constexpr uint32 ACTION_SUMMON_ALL      = GOSSIP_ACTION_INFO_DEF + 0xFFFFFC;
    constexpr uint32 ACTION_DISMISS_ALL     = GOSSIP_ACTION_INFO_DEF + 0xFFFFFD;
    constexpr uint32 ACTION_CLOSE           = GOSSIP_ACTION_INFO_DEF + 0xFFFFFF;
    constexpr uint32 ACTION_NOOP            = GOSSIP_ACTION_INFO_DEF + 0xFFFFFE;

    // -----------------------------------------------------------------------
    // Token-shop flow (new)
    //
    // Disambiguated from the summon flow by `sender` (the summon flow uses
    // GOSSIP_SENDER_MAIN). Within each shop sender the action is either a
    // payload (bot low-guid / currency item id / item id) or a control code
    // from the 0xF0000000+ range that real low-guids and item ids never reach.
    // -----------------------------------------------------------------------
    constexpr uint32 SENDER_SHOP_PICK_BOT      = 100;
    constexpr uint32 SENDER_SHOP_PICK_CURRENCY = 101;
    constexpr uint32 SENDER_SHOP_ITEM_LIST     = 102;
    constexpr uint32 SENDER_SHOP_CONFIRM       = 103;

    constexpr uint32 ACTION_SHOP_ENTRY    = GOSSIP_ACTION_INFO_DEF + 0xFFFEFF;
    constexpr uint32 ACTION_CTRL_BACK     = 0xF0000001; // up one menu
    constexpr uint32 ACTION_CTRL_PREV     = 0xF0000002; // previous page
    constexpr uint32 ACTION_CTRL_NEXT     = 0xF0000003; // next page
    constexpr uint32 ACTION_CTRL_CLOSE    = 0xF000000F; // close menu

    constexpr uint32 SHOP_ITEMS_PER_PAGE  = 14;

    struct ShopState
    {
        ObjectGuid botGuid;
        uint32     currencyItemId = 0;
        uint32     page           = 0;
        std::vector<UpgradeOption> upgrades; // cached per visit, refreshed per buy
    };

    // Per-master state. The Botmaster is the only entry point so we key off
    // the master player guid; cleared when they re-enter the shop top menu.
    std::unordered_map<ObjectGuid, ShopState> g_shopState;

    // Item-link with quality color, e.g. "|cffa335ee|Hitem:50625:0:...|h[Name]|h|r".
    // Renders as a clickable tooltip-bearing link in 3.3.5a gossip text.
    std::string ItemLink(uint32 itemId)
    {
        if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(itemId))
            return ChatHelper::FormatItem(p);
        std::ostringstream o;
        o << "item:" << itemId;
        return o.str();
    }

    // Display name for a currency id (handles synthetic honor/arena).
    std::string CurrencyName(uint32 currencyId)
    {
        if (currencyId == EmblemVendorCache::SYNTH_CURRENCY_HONOR)
            return "Honor Points";
        if (currencyId == EmblemVendorCache::SYNTH_CURRENCY_ARENA)
            return "Arena Points";
        if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(currencyId))
            return p->Name1;
        return "tokens";
    }

    // How much of a currency the bot holds (handles synthetic honor/arena).
    uint32 CurrencyHeld(Player* bot, uint32 currencyId)
    {
        if (currencyId == EmblemVendorCache::SYNTH_CURRENCY_HONOR)
            return bot->GetHonorPoints();
        if (currencyId == EmblemVendorCache::SYNTH_CURRENCY_ARENA)
            return bot->GetArenaPoints();
        return bot->GetItemCount(currencyId, false);
    }

    // "75 [Emblem of Frost] + 1 [Trophy of the Crusade]" style cost summary.
    std::string FormatCostSummary(ItemExtendedCostEntry const* ec)
    {
        if (!ec)
            return "(unknown cost)";

        std::ostringstream o;
        bool first = true;

        for (uint32 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        {
            if (!ec->reqitem[i])
                continue;
            if (!first) o << " + ";
            o << ec->reqitemcount[i] << " " << ItemLink(ec->reqitem[i]);
            first = false;
        }
        if (ec->reqhonorpoints)
        {
            if (!first) o << " + ";
            o << ec->reqhonorpoints << " Honor";
            first = false;
        }
        if (ec->reqarenapoints)
        {
            if (!first) o << " + ";
            o << ec->reqarenapoints << " Arena";
            first = false;
        }
        if (ec->reqpersonalarenarating)
        {
            if (!first) o << " + ";
            o << "rated " << ec->reqpersonalarenarating;
            first = false;
        }
        return o.str();
    }

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

    // Chat-window hyperlinks are hover-able for tooltips in 3.3.5a; gossip
    // option text isn't. So we mirror the current page's items into the
    // master's chat so they can hover-inspect before buying.
    void WhisperVisibleUpgradeLinks(Player* player, ShopState const& state)
    {
        if (state.upgrades.empty())
            return;

        ChatHandler h(player->GetSession());

        uint32 const totalPages = (uint32(state.upgrades.size()) + SHOP_ITEMS_PER_PAGE - 1)
                                  / SHOP_ITEMS_PER_PAGE;
        uint32 const start = state.page * SHOP_ITEMS_PER_PAGE;
        uint32 const end   = std::min(start + SHOP_ITEMS_PER_PAGE, uint32(state.upgrades.size()));

        h.PSendSysMessage("=== Upgrades (page {}/{}) — hover for tooltips ===",
                          state.page + 1, totalPages);

        for (uint32 i = start; i < end; ++i)
        {
            UpgradeOption const& opt = state.upgrades[i];
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(opt.itemId);
            if (!p)
                continue;

            // Format: "[Item] -> [Replacement] (25)" — drops emblem link/name
            // and score prefix to keep the line short and tooltip-focused.
            // Empty slot becomes "[Item] (25)" with no arrow.
            // Cost number: prefer item-cost slot 0, fall back to honor/arena.
            ItemExtendedCostEntry const* ec = sItemExtendedCostStore.LookupEntry(opt.extendedCostId);
            uint32 costNum = 0;
            if (ec)
            {
                for (uint32 j = 0; j < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++j)
                    if (ec->reqitem[j] && ec->reqitemcount[j] > 0) { costNum = ec->reqitemcount[j]; break; }
                if (!costNum && ec->reqhonorpoints) costNum = ec->reqhonorpoints;
                if (!costNum && ec->reqarenapoints) costNum = ec->reqarenapoints;
            }

            std::ostringstream line;
            line << ChatHelper::FormatItem(p);
            if (opt.replacesItemId)
                line << " -> " << ItemLink(opt.replacesItemId);
            line << " (" << costNum << ")";

            h.PSendSysMessage("{}", line.str());
        }
    }

    // Build the SQL "IN (...)" list of account ids the master can pull bots
    // from: their own account plus any trusted-linked accounts. Gated on
    // allowTrustedAccountBots to mirror PlayerbotMgr's own addaccount check.
    std::string BuildAccountInList(uint32 masterAccount)
    {
        std::ostringstream out;
        out << masterAccount;
        if (sPlayerbotAIConfig.allowTrustedAccountBots)
        {
            QueryResult linkResult = PlayerbotsDatabase.Query(
                "SELECT linked_account_id FROM playerbots_account_links WHERE account_id = {}",
                masterAccount);
            if (linkResult)
            {
                do
                {
                    out << "," << linkResult->Fetch()[0].Get<uint32>();
                }
                while (linkResult->NextRow());
            }
        }
        return out.str();
    }

    // -----------------------------------------------------------------------
    // Shop renderers — each one builds a gossip menu page.
    // -----------------------------------------------------------------------

    void ShowShopBotPicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
        uint32 listed = 0;
        if (mgr)
        {
            for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
            {
                Player* bot = it->second;
                if (!bot)
                    continue;

                std::ostringstream label;
                label << bot->GetName()
                      << " (Lv " << uint32(bot->GetLevel()) << " "
                      << ClassName(bot->getClass()) << ")";

                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label.str(),
                                 SENDER_SHOP_PICK_BOT, bot->GetGUID().GetCounter());
                ++listed;
            }
        }

        if (listed == 0)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "No bots are summoned. Summon one first, then come back.",
                             SENDER_SHOP_PICK_BOT, ACTION_CTRL_BACK);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back",
                         SENDER_SHOP_PICK_BOT, ACTION_CTRL_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_SHOP_PICK_BOT, ACTION_CTRL_CLOSE);

        SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());
    }

    void ShowShopCurrencyPicker(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        auto it = g_shopState.find(player->GetGUID());
        if (it == g_shopState.end())
        {
            ShowShopBotPicker(player, creature);
            return;
        }

        Player* bot = ObjectAccessor::FindConnectedPlayer(it->second.botGuid);
        if (!bot)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("That bot is no longer online.");
            g_shopState.erase(it);
            ShowShopBotPicker(player, creature);
            return;
        }

        // Show every currency the bot has > 0 of that any vendor accepts.
        std::vector<uint32> ids = sEmblemVendorCache.GetAllCurrencyIds();
        std::sort(ids.begin(), ids.end()); // stable display order

        uint32 listed = 0;
        for (uint32 currencyId : ids)
        {
            uint32 have = CurrencyHeld(bot, currencyId);
            if (have == 0)
                continue;

            // Skip currencies we can't even name (missing item template, not honor/arena).
            std::string name = CurrencyName(currencyId);
            if (name == "tokens")
                continue;

            std::ostringstream label;
            label << name << "  (" << have << ")";
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, label.str(),
                             SENDER_SHOP_PICK_CURRENCY, currencyId);
            ++listed;
        }

        if (listed == 0)
        {
            std::ostringstream o;
            o << bot->GetName() << " has no spendable tokens.";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, o.str(),
                             SENDER_SHOP_PICK_CURRENCY, ACTION_CTRL_BACK);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back to bot pick",
                         SENDER_SHOP_PICK_CURRENCY, ACTION_CTRL_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_SHOP_PICK_CURRENCY, ACTION_CTRL_CLOSE);

        SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());
    }

    void ShowShopItemList(Player* player, Creature* creature, bool refresh)
    {
        ClearGossipMenuFor(player);

        auto stateIt = g_shopState.find(player->GetGUID());
        if (stateIt == g_shopState.end())
        {
            ShowShopBotPicker(player, creature);
            return;
        }
        ShopState& state = stateIt->second;

        Player* bot = ObjectAccessor::FindConnectedPlayer(state.botGuid);
        if (!bot)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("That bot is no longer online.");
            g_shopState.erase(stateIt);
            ShowShopBotPicker(player, creature);
            return;
        }

        if (refresh || state.upgrades.empty())
            state.upgrades = EmblemShopUpgrades::FindUpgrades(bot, state.currencyItemId);

        std::string curName = CurrencyName(state.currencyItemId);

        if (state.upgrades.empty())
        {
            std::ostringstream o;
            o << "No upgrades available for " << bot->GetName() << " using " << curName << ".";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, o.str(),
                             SENDER_SHOP_ITEM_LIST, ACTION_CTRL_BACK);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back to currency pick",
                             SENDER_SHOP_ITEM_LIST, ACTION_CTRL_BACK);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                             SENDER_SHOP_ITEM_LIST, ACTION_CTRL_CLOSE);
            SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());
            return;
        }

        uint32 const total = uint32(state.upgrades.size());
        uint32 const totalPages = (total + SHOP_ITEMS_PER_PAGE - 1) / SHOP_ITEMS_PER_PAGE;
        if (state.page >= totalPages)
            state.page = totalPages - 1;

        uint32 const start = state.page * SHOP_ITEMS_PER_PAGE;
        uint32 const end   = std::min(start + SHOP_ITEMS_PER_PAGE, total);

        for (uint32 i = start; i < end; ++i)
        {
            UpgradeOption const& opt = state.upgrades[i];
            ItemTemplate const* p = sObjectMgr->GetItemTemplate(opt.itemId);
            if (!p)
                continue;

            int32 gainInt = int32(std::round(opt.scoreGain));
            std::ostringstream label;
            label << (opt.canAfford ? "+" : "[need more] +") << gainInt
                  << "  " << ChatHelper::FormatItem(p);

            // Direct click → confirm popup → buy. popupText shows the receipt;
            // popupMoney=0, coded=false → yes/no prompt only.
            ItemExtendedCostEntry const* ec = sItemExtendedCostStore.LookupEntry(opt.extendedCostId);

            std::ostringstream popup;
            popup << "Buy " << p->Name1 << " for " << FormatCostSummary(ec) << "?";
            if (opt.replacesItemId)
            {
                if (ItemTemplate const* old = sObjectMgr->GetItemTemplate(opt.replacesItemId))
                    popup << "\n\nReplaces: " << old->Name1 << " (moved to bags).";
            }
            else
            {
                popup << "\n\nFills an empty slot.";
            }

            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label.str(),
                             SENDER_SHOP_CONFIRM, opt.itemId,
                             popup.str(), 0, false);
        }

        // Footer with pagination + nav.
        std::ostringstream header;
        header << "-- " << bot->GetName() << " spending " << curName
               << " (page " << (state.page + 1) << "/" << totalPages << ") --";
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, header.str(),
                         SENDER_SHOP_ITEM_LIST, ACTION_CTRL_BACK);

        if (state.page > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Prev page",
                             SENDER_SHOP_ITEM_LIST, ACTION_CTRL_PREV);
        if (state.page + 1 < totalPages)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Next page ->",
                             SENDER_SHOP_ITEM_LIST, ACTION_CTRL_NEXT);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back to currency pick",
                         SENDER_SHOP_ITEM_LIST, ACTION_CTRL_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_SHOP_ITEM_LIST, ACTION_CTRL_CLOSE);

        SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());

        // Mirror current page's items into chat for hover-tooltip access.
        WhisperVisibleUpgradeLinks(player, state);
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

        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, name, class, level FROM characters "
            "WHERE account IN ({}) AND name <> '{}' "
            "ORDER BY level DESC, name ASC",
            BuildAccountInList(accountId), masterName);

        // Fetch rows first so we know offline-count for the top header before
        // we start emitting menu items.
        struct BotRow
        {
            ObjectGuid::LowType lowGuid;
            std::string         name;
            uint8               cls;
            uint8               lvl;
            bool                isOnline;
        };
        std::vector<BotRow> rows;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                BotRow r{};
                r.lowGuid = fields[0].Get<uint32>();
                r.name    = fields[1].Get<std::string>();
                r.cls     = fields[2].Get<uint8>();
                r.lvl     = fields[3].Get<uint8>();

                ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(r.lowGuid);
                r.isOnline = onlineBots.count(botGuid) > 0
                          || ObjectAccessor::FindConnectedPlayer(botGuid) != nullptr;

                rows.push_back(std::move(r));
            }
            while (result->NextRow());
        }

        uint32 offlineCount = 0;
        uint32 onlineCount  = 0;
        for (BotRow const& r : rows)
            (r.isOnline ? onlineCount : offlineCount) += 1;

        // ---- Top-level actions (placed before the individual bot list) ----
        if (offlineCount > 0)
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Summon all my bots",
                             GOSSIP_SENDER_MAIN, ACTION_SUMMON_ALL);

        if (onlineCount > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Dismiss all my summoned bots",
                             GOSSIP_SENDER_MAIN, ACTION_DISMISS_ALL);

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Shop with my bots' tokens ->",
                         GOSSIP_SENDER_MAIN, ACTION_SHOP_ENTRY);

        // ---- Individual bots ----
        uint32 shown = 0;
        for (BotRow const& r : rows)
        {
            std::ostringstream label;
            label << r.name
                  << " (Lv " << uint32(r.lvl) << " " << ClassName(r.cls) << ")";
            if (r.isOnline)
                label << "  [in world]";

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label.str(),
                             GOSSIP_SENDER_MAIN,
                             GOSSIP_ACTION_INFO_DEF + r.lowGuid);

            if (++shown >= MAX_BOTS_SHOWN)
                break;
        }

        if (rows.empty())
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "You have no other characters on this account.",
                             GOSSIP_SENDER_MAIN, ACTION_NOOP);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         GOSSIP_SENDER_MAIN, ACTION_CLOSE);

        SendGossipMenuFor(player, BOTMASTER_TEXT_ID, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 sender, uint32 action) override
    {
        // ---- Shop flow dispatch (early return) ---------------------------
        if (action == ACTION_SHOP_ENTRY)
        {
            // Reset state on fresh entry so stale data from a prior visit doesn't leak.
            g_shopState.erase(player->GetGUID());
            ShowShopBotPicker(player, creature);
            return true;
        }

        if (sender == SENDER_SHOP_PICK_BOT)
        {
            if (action == ACTION_CTRL_CLOSE) { CloseGossipMenuFor(player); return true; }
            if (action == ACTION_CTRL_BACK)  { return OnGossipHello(player, creature); }

            // action = bot low guid
            ObjectGuid::LowType lowGuid = action;
            ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
            Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid);
            if (!bot)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("That bot is no longer online.");
                ShowShopBotPicker(player, creature);
                return true;
            }

            ShopState& s = g_shopState[player->GetGUID()];
            s.botGuid        = botGuid;
            s.currencyItemId = 0;
            s.page           = 0;
            s.upgrades.clear();

            ShowShopCurrencyPicker(player, creature);
            return true;
        }

        if (sender == SENDER_SHOP_PICK_CURRENCY)
        {
            if (action == ACTION_CTRL_CLOSE) { CloseGossipMenuFor(player); return true; }
            if (action == ACTION_CTRL_BACK)  { ShowShopBotPicker(player, creature); return true; }

            ShopState& s = g_shopState[player->GetGUID()];
            s.currencyItemId = action;     // action = currency item id
            s.page           = 0;
            s.upgrades.clear();

            ShowShopItemList(player, creature, /*refresh=*/true);
            return true;
        }

        if (sender == SENDER_SHOP_ITEM_LIST)
        {
            if (action == ACTION_CTRL_CLOSE) { CloseGossipMenuFor(player); return true; }
            if (action == ACTION_CTRL_BACK)  { ShowShopCurrencyPicker(player, creature); return true; }
            if (action == ACTION_CTRL_PREV)
            {
                auto it = g_shopState.find(player->GetGUID());
                if (it != g_shopState.end() && it->second.page > 0)
                    --it->second.page;
                ShowShopItemList(player, creature, /*refresh=*/false);
                return true;
            }
            if (action == ACTION_CTRL_NEXT)
            {
                auto it = g_shopState.find(player->GetGUID());
                if (it != g_shopState.end())
                    ++it->second.page;
                ShowShopItemList(player, creature, /*refresh=*/false);
                return true;
            }
            // Item clicks go straight to the popup → SENDER_SHOP_CONFIRM, so
            // any action arriving here is unexpected. Redraw defensively.
            ShowShopItemList(player, creature, /*refresh=*/false);
            return true;
        }

        if (sender == SENDER_SHOP_CONFIRM)
        {
            // action = item id (chosen by the user, confirmed via popup)
            uint32 itemId = action;

            auto it = g_shopState.find(player->GetGUID());
            if (it == g_shopState.end())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Shop session expired — please re-open.");
                CloseGossipMenuFor(player);
                return true;
            }

            Player* bot = ObjectAccessor::FindConnectedPlayer(it->second.botGuid);
            if (!bot)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("That bot is no longer online.");
                g_shopState.erase(it);
                ShowShopBotPicker(player, creature);
                return true;
            }

            // Look up the exact cost — should always be found since we sourced
            // the itemId from the cached upgrade list a moment ago.
            uint32 chosenCost = 0;
            for (UpgradeOption const& opt : it->second.upgrades)
            {
                if (opt.itemId == itemId) { chosenCost = opt.extendedCostId; break; }
            }

            ChatHandler handler(player->GetSession());
            if (!chosenCost)
            {
                handler.PSendSysMessage("That item is no longer in the upgrade list — refreshing.");
                ShowShopItemList(player, creature, /*refresh=*/true);
                return true;
            }

            std::string err;
            if (!sEmblemShopMgr.BuyForBot(bot, itemId, chosenCost, err))
            {
                handler.PSendSysMessage("Could not buy: {}", err);
            }
            else
            {
                if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(itemId))
                    handler.PSendSysMessage("{} bought {}.", bot->GetName(), p->Name1);
            }

            // Refresh list so the just-bought item disappears (or moves down
            // the ladder) and remaining options re-score against the new gear.
            ShowShopItemList(player, creature, /*refresh=*/true);
            return true;
        }

        // ---- Existing summon flow (unchanged) ----------------------------
        if (action == ACTION_CLOSE || action == ACTION_NOOP)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        ChatHandler handler(player->GetSession());

        if (action == ACTION_SUMMON_ALL)
        {
            PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player);
            if (!mgr)
            {
                handler.PSendSysMessage("Bot system is not available for you yet.");
                CloseGossipMenuFor(player);
                return true;
            }

            uint32 masterAccount = player->GetSession()->GetAccountId();
            QueryResult result = CharacterDatabase.Query(
                "SELECT guid FROM characters WHERE account IN ({}) AND name <> '{}'",
                BuildAccountInList(masterAccount), player->GetName());

            uint32 requested = 0;
            if (result)
            {
                do
                {
                    ObjectGuid::LowType lowGuid = result->Fetch()[0].Get<uint32>();
                    ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(lowGuid);
                    if (ObjectAccessor::FindConnectedPlayer(botGuid))
                        continue;  // already in world
                    mgr->AddPlayerBot(botGuid, masterAccount);
                    ++requested;
                }
                while (result->NextRow());
            }

            handler.PSendSysMessage("Summoning {} bot(s)...", requested);
            CloseGossipMenuFor(player);
            return true;
        }

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
