/*
 * EmblemShopUpgrades
 *
 * Ranks emblem-vendor offerings against a bot's current gear so the shop UI
 * can show "+47 [Tier Chest] replaces [Old Chest]" lines sorted by score-gain.
 *
 * Uses StatsWeightCalculator (the same scorer mod-playerbots uses for autogear
 * and equip-upgrade decisions). Item-set bonuses are ENABLED here — emblem
 * gear is mostly tier sets and the 2pc/4pc bonuses are the whole reason to
 * buy it, so excluding them would mis-rank the catalog.
 */

#ifndef _PLAYERBOT_EMBLEM_SHOP_UPGRADES_H
#define _PLAYERBOT_EMBLEM_SHOP_UPGRADES_H

#include "Define.h"

#include <vector>

class Player;

struct UpgradeOption
{
    uint32 itemId;
    uint32 extendedCostId;
    uint32 vendorEntry;
    float  scoreGain;        // newScore - oldScore (> 0 by filter)
    uint32 replacesItemId;   // 0 = slot was empty
    bool   canAfford;
    uint32 invType;          // ItemTemplate::InventoryType, for slot grouping
};

namespace EmblemShopUpgrades
{
    // Returns upgrades the bot can equip, paid in `currencyItemId`, sorted by
    // score gain descending. Best-per-InventoryType dedup keeps the list tight.
    // Affordability is annotated, not filtered — unaffordable upgrades still
    // appear (greyed in the UI) so the user sees the full ladder.
    std::vector<UpgradeOption> FindUpgrades(Player* bot, uint32 currencyItemId);
}

#endif
