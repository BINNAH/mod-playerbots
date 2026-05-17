/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license.
 */

// MSVC 14.38 <chrono> ICE workaround — see EmblemVendorCache.cpp.
#include <chrono>

#include "EmblemShopUpgrades.h"
#include "EmblemVendorCache.h"

#include "Playerbots.h"

#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StatsWeightCalculator.h"

#include <algorithm>
#include <unordered_map>

namespace
{
    bool BotCanAfford(Player* bot, ItemExtendedCostEntry const* ec)
    {
        if (!ec)
            return false;

        for (uint32 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
            if (ec->reqitem[i] && bot->GetItemCount(ec->reqitem[i], false) < ec->reqitemcount[i])
                return false;

        if (ec->reqhonorpoints && bot->GetHonorPoints() < ec->reqhonorpoints)
            return false;
        if (ec->reqarenapoints && bot->GetArenaPoints() < ec->reqarenapoints)
            return false;
        if (ec->reqpersonalarenarating
            && bot->GetMaxPersonalArenaRatingRequirement(ec->reqarenaslot) < ec->reqpersonalarenarating)
            return false;

        return true;
    }

    bool ItemIsUsableByBot(Player* bot, ItemTemplate const* p)
    {
        if (p->RequiredLevel > bot->GetLevel())
            return false;
        if (p->AllowableClass && !(p->AllowableClass & bot->getClassMask()))
            return false;
        if (p->AllowableRace && !(p->AllowableRace & bot->getRaceMask()))
            return false;
        return true;
    }
}

std::vector<UpgradeOption> EmblemShopUpgrades::FindUpgrades(Player* bot, uint32 currencyItemId)
{
    std::vector<UpgradeOption> out;
    if (!bot)
        return out;

    auto const& offerings = sEmblemVendorCache.GetOfferingsByCurrencyItem(currencyItemId);
    if (offerings.empty())
        return out;

    StatsWeightCalculator calc(bot);
    calc.SetItemSetBonus(true);     // tier set bonuses matter for emblem gear
    calc.SetOverflowPenalty(true);  // honour stat caps when scoring

    // Best-per-slot dedup. Keyed by ItemTemplate::InventoryType so rings/trinkets
    // collapse to one entry; the user can re-shop after equipping the first.
    std::unordered_map<uint32, UpgradeOption> bestPerSlot;

    for (auto const& off : offerings)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(off.itemId);
        if (!proto)
            continue;

        // Gear only — skip gems, consumables, mounts, tabards, etc. for v1.
        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON)
            continue;

        if (!ItemIsUsableByBot(bot, proto))
            continue;

        uint8 dstSlot = bot->FindEquipSlot(proto, NULL_SLOT, true);
        if (dstSlot == NULL_SLOT)
            continue;

        float newScore = calc.CalculateItem(proto->ItemId);
        if (newScore <= 0.f)
            continue;  // scorer says this item is irrelevant to bot's spec

        uint32 replacesId = 0;
        float oldScore = 0.f;
        if (Item* equipped = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot))
        {
            if (ItemTemplate const* oldProto = equipped->GetTemplate())
            {
                oldScore = calc.CalculateItem(oldProto->ItemId);
                replacesId = oldProto->ItemId;
            }
        }

        float gain = newScore - oldScore;
        if (gain <= 0.f)
            continue;  // not actually an upgrade

        ItemExtendedCostEntry const* ec = sItemExtendedCostStore.LookupEntry(off.extendedCostId);

        UpgradeOption candidate;
        candidate.itemId         = off.itemId;
        candidate.extendedCostId = off.extendedCostId;
        candidate.vendorEntry    = off.vendorEntry;
        candidate.scoreGain      = gain;
        candidate.replacesItemId = replacesId;
        candidate.canAfford      = BotCanAfford(bot, ec);
        candidate.invType        = proto->InventoryType;

        auto it = bestPerSlot.find(candidate.invType);
        if (it == bestPerSlot.end() || candidate.scoreGain > it->second.scoreGain)
            bestPerSlot[candidate.invType] = candidate;
    }

    out.reserve(bestPerSlot.size());
    for (auto& kv : bestPerSlot)
        out.push_back(kv.second);

    std::sort(out.begin(), out.end(),
        [](UpgradeOption const& a, UpgradeOption const& b) { return a.scoreGain > b.scoreGain; });

    (void)currencyItemId; // referenced for future per-currency tuning; unused right now
    return out;
}
