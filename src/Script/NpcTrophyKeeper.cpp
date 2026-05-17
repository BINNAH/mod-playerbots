/*
 * Custom CreatureScript: Gimped Trophy Keeper (entry 600003)
 *
 * Shows the player their boss kill counts via a tier-organized gossip menu.
 * Starts with Naxxramas (Tier 7). Adding more tiers = extend the kTiers array.
 *
 * Data source: AC's achievement criteria store. The "Statistics" tab in the
 * in-game Achievements UI is fed by the same data. For each boss we ask
 * sAchievementMgr->GetSpecialAchievementCriteriaByType(KILL_CREATURE, id)
 * for the criteria that count kills of that creature, then sum the player's
 * progress across them -- which combines 10/25-player counts into one number.
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
        uint32 creatureId10;        // 10-player creature entry
        uint32 creatureId25;        // 25-player creature entry, or 0 if same as 10
    };

    struct TierEntry
    {
        char const* displayName;
        std::vector<BossEntry> bosses;
    };

    // Naxxramas (WotLK). Creature IDs verified against creature_template.
    // The Four Horsemen kill credit goes through Highlord Mograine (16062);
    // he has no difficulty_entry_1, so the same entry covers 10 and 25.
    TierEntry const& Naxx()
    {
        static const TierEntry t = {
            "Tier 7 -- Naxxramas",
            {
                { "Anub'Rekhan",            15956, 29249 },
                { "Grand Widow Faerlina",   15953, 29268 },
                { "Maexxna",                15952, 29278 },
                { "Noth the Plaguebringer", 15954, 29615 },
                { "Heigan the Unclean",     15936, 29701 },
                { "Loatheb",                16011, 29718 },
                { "Instructor Razuvious",   16061, 29940 },
                { "Gothik the Harvester",   16060, 29955 },
                { "The Four Horsemen",      16062, 0     },
                { "Patchwerk",              16028, 29324 },
                { "Grobbulus",              15931, 29373 },
                { "Gluth",                  15932, 29417 },
                { "Thaddius",               15928, 29448 },
                { "Sapphiron",              15989, 29991 },
                { "Kel'Thuzad",             15990, 30061 },
            }
        };
        return t;
    }

    std::vector<TierEntry const*> const& Tiers()
    {
        static const std::vector<TierEntry const*> v = { &Naxx() };
        return v;
    }

    uint32 GetKillsForCreatureId(Player* player, uint32 creatureId)
    {
        if (!creatureId)
            return 0;

        AchievementCriteriaEntryList const* list =
            sAchievementMgr->GetSpecialAchievementCriteriaByType(
                ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE, creatureId);
        if (!list)
            return 0;

        AchievementMgr* mgr = player->GetAchievementMgr();
        if (!mgr)
            return 0;

        uint32 total = 0;
        for (AchievementCriteriaEntry const* criterion : *list)
            if (CriteriaProgress* prog = mgr->GetCriteriaProgress(criterion))
                total += prog->counter;
        return total;
    }

    uint32 GetBossKills(Player* player, BossEntry const& boss)
    {
        return GetKillsForCreatureId(player, boss.creatureId10)
             + GetKillsForCreatureId(player, boss.creatureId25);
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
                    handler.PSendSysMessage("|cff00ff00{}|r -- {} kill{} (combined 10/25).",
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
