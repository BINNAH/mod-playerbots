/*
 * EmblemVendorCache
 *
 * In-memory catalog of every npc_vendor row whose ExtendedCost references the
 * ItemExtendedCost.dbc. Indexed by the currency item id used to pay (Badge of
 * Justice 29434, Emblem of Heroism 40752, Valor 40753, Conquest 45624, Triumph
 * 47241, Frost 49426, plus tier-token tokens like Trophy of the Crusade).
 *
 * Loaded once at WORLDHOOK_ON_STARTUP (after DBCs + WorldDatabase are ready),
 * reload-able via Load().
 *
 * This is the "shopping list" side of the bot token-spender feature: it does
 * NOT execute transactions. EmblemShopMgr handles the buy itself by reading
 * from this catalog and applying the deduction directly to a bot's inventory,
 * bypassing the vendor-distance check (the bot does not have to be standing at
 * the vendor — the catalog is treated as a global menu).
 */

#ifndef _PLAYERBOT_EMBLEM_VENDOR_CACHE_H
#define _PLAYERBOT_EMBLEM_VENDOR_CACHE_H

#include "Define.h"

#include <unordered_map>
#include <vector>

class EmblemVendorCache
{
public:
    struct Offering
    {
        uint32 vendorEntry;
        uint32 itemId;
        uint32 extendedCostId;
    };

    static EmblemVendorCache& Instance();

    void Load();

    std::vector<Offering> const& GetOfferingsByCurrencyItem(uint32 currencyItemId) const;
    std::vector<Offering> const& AllOfferings() const { return _all; }

    // Every currency item id that any vendor accepts (Frost, Triumph, tier
    // tokens, Trophy of the Crusade, Primordial Saronite, …). Used by the
    // shop UI to list spendable currencies the bot has > 0 of.
    std::vector<uint32> GetAllCurrencyIds() const;

    bool IsLoaded() const { return _loaded; }
    std::size_t Size() const { return _all.size(); }

private:
    EmblemVendorCache() = default;
    EmblemVendorCache(EmblemVendorCache const&) = delete;
    EmblemVendorCache& operator=(EmblemVendorCache const&) = delete;

    std::vector<Offering> _all;
    std::unordered_map<uint32, std::vector<Offering>> _byCurrency;
    bool _loaded = false;
};

#define sEmblemVendorCache EmblemVendorCache::Instance()

#endif
