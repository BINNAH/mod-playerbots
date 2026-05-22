/*
 * Emblem tier chain (WotLK).
 *
 * The emblem exchange NPC lets a player trade a higher-tier emblem DOWN to any
 * lower tier 1:1 (Frost -> Triumph -> Conquest -> Valor -> Heroism). So a cost
 * priced in a lower emblem can always be paid with a higher one. These helpers
 * model that so the shop can spend e.g. Emblem of Triumph on ilvl-200 gear that
 * normally costs Conquest/Valor, without the player manually converting.
 *
 * Header-only (only <cstdint>/<vector>). Tier tokens (Trophy of the Crusade,
 * Primordial Saronite, …), honor and arena points are NOT part of this chain.
 */
#ifndef _PLAYERBOT_EMBLEM_TIERS_H
#define _PLAYERBOT_EMBLEM_TIERS_H

#include "Define.h"

#include <vector>

namespace EmblemTiers
{
    // Ascending tier order (lowest first).
    inline std::vector<uint32> const& Chain()
    {
        static std::vector<uint32> const chain = {
            40752,  // Emblem of Heroism
            40753,  // Emblem of Valor
            45624,  // Emblem of Conquest
            47241,  // Emblem of Triumph
            49426   // Emblem of Frost
        };
        return chain;
    }

    // Index of an emblem in the chain, or -1 if it isn't an emblem.
    inline int IndexOf(uint32 itemId)
    {
        std::vector<uint32> const& c = Chain();
        for (size_t i = 0; i < c.size(); ++i)
            if (c[i] == itemId)
                return (int)i;
        return -1;
    }

    inline bool IsEmblem(uint32 itemId) { return IndexOf(itemId) >= 0; }

    // The emblem plus every LOWER tier (what currency `emblemId` can be spent
    // on). Used to widen the shop listing.
    inline std::vector<uint32> AtOrBelow(uint32 emblemId)
    {
        std::vector<uint32> out;
        int idx = IndexOf(emblemId);
        if (idx < 0)
            return out;
        std::vector<uint32> const& c = Chain();
        for (int i = 0; i <= idx; ++i)
            out.push_back(c[i]);
        return out;
    }

    // The emblem plus every HIGHER tier, lowest-first (the pool that can pay a
    // cost priced in `emblemId`). Deduct from the front so the exact tier is
    // spent before higher ones.
    inline std::vector<uint32> AtOrAbove(uint32 emblemId)
    {
        std::vector<uint32> out;
        int idx = IndexOf(emblemId);
        if (idx < 0)
            return out;
        std::vector<uint32> const& c = Chain();
        for (int i = idx; i < (int)c.size(); ++i)
            out.push_back(c[i]);
        return out;
    }
}

#endif
