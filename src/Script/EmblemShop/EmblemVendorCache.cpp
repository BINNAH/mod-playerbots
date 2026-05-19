/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license.
 */

// MSVC 14.38.33130 has a known C1001 ICE in <chrono> when it is pulled in
// transitively through these AC headers in some orders. Including <chrono>
// up-front parses it cleanly before the templated AC headers can confuse it.
#include <chrono>

#include "EmblemVendorCache.h"

#include "Playerbots.h"

#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"

EmblemVendorCache& EmblemVendorCache::Instance()
{
    static EmblemVendorCache instance;
    return instance;
}

void EmblemVendorCache::Load()
{
    _all.clear();
    _byCurrency.clear();
    _loaded = false;

    QueryResult result = WorldDatabase.Query(
        "SELECT entry, item, ExtendedCost FROM npc_vendor WHERE ExtendedCost > 0");

    if (!result)
    {
        LOG_INFO("playerbots", "EmblemVendorCache: no npc_vendor rows with ExtendedCost > 0.");
        _loaded = true;
        return;
    }

    uint32 added = 0;
    uint32 skippedMissingDbc = 0;
    uint32 skippedMissingItem = 0;

    do
    {
        Field* f = result->Fetch();
        uint32 vendorEntry    = f[0].Get<uint32>();
        uint32 itemId         = f[1].Get<uint32>();
        uint32 extendedCostId = f[2].Get<uint32>();

        ItemExtendedCostEntry const* ec = sItemExtendedCostStore.LookupEntry(extendedCostId);
        if (!ec)
        {
            ++skippedMissingDbc;
            continue;
        }

        if (!sObjectMgr->GetItemTemplate(itemId))
        {
            ++skippedMissingItem;
            continue;
        }

        Offering off{ vendorEntry, itemId, extendedCostId };
        _all.push_back(off);

        // A single cost entry can reference up to 5 currency items (e.g. tier
        // helm = N Frost + 1 Trophy of the Crusade), so the offering is indexed
        // under each non-zero currency slot.
        for (uint32 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        {
            if (ec->reqitem[i] != 0)
                _byCurrency[ec->reqitem[i]].push_back(off);
        }

        // Also index under synthetic ids for honor/arena so PvP gear shows up
        // in the picker when the bot has > 0 of either.
        if (ec->reqhonorpoints > 0)
            _byCurrency[SYNTH_CURRENCY_HONOR].push_back(off);
        if (ec->reqarenapoints > 0)
            _byCurrency[SYNTH_CURRENCY_ARENA].push_back(off);

        ++added;
    } while (result->NextRow());

    _loaded = true;

    LOG_INFO("playerbots",
        "EmblemVendorCache: indexed {} offerings across {} currency item ids "
        "(skipped {} missing DBC, {} missing item template).",
        added, uint32(_byCurrency.size()), skippedMissingDbc, skippedMissingItem);
}

std::vector<EmblemVendorCache::Offering> const&
EmblemVendorCache::GetOfferingsByCurrencyItem(uint32 currencyItemId) const
{
    static std::vector<Offering> const empty;
    auto it = _byCurrency.find(currencyItemId);
    if (it == _byCurrency.end())
        return empty;
    return it->second;
}

std::vector<uint32> EmblemVendorCache::GetAllCurrencyIds() const
{
    std::vector<uint32> ids;
    ids.reserve(_byCurrency.size());
    for (auto const& kv : _byCurrency)
        ids.push_back(kv.first);
    return ids;
}
