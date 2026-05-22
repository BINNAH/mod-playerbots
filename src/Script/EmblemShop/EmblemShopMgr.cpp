/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license.
 */

// See EmblemVendorCache.cpp for the MSVC 14.38 <chrono> ICE workaround.
#include <chrono>

#include "EmblemShopMgr.h"
#include "EmblemTiers.h"

#include "Playerbots.h"

#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"

#include <algorithm>
#include <sstream>

EmblemShopMgr& EmblemShopMgr::Instance()
{
    static EmblemShopMgr instance;
    return instance;
}

namespace
{
    std::string ItemNameOr(uint32 itemId, char const* fallbackPrefix)
    {
        if (ItemTemplate const* p = sObjectMgr->GetItemTemplate(itemId))
            return p->Name1;
        std::ostringstream o;
        o << fallbackPrefix << " " << itemId;
        return o.str();
    }

    // Spendable count toward a cost in `reqItemId`: an emblem can be paid with
    // itself or any higher-tier emblem (1:1 down-conversion); other tokens are
    // exact.
    uint32 SpendableCount(Player* bot, uint32 reqItemId)
    {
        if (EmblemTiers::IsEmblem(reqItemId))
        {
            uint32 total = 0;
            for (uint32 e : EmblemTiers::AtOrAbove(reqItemId))
                total += bot->GetItemCount(e, false);
            return total;
        }
        return bot->GetItemCount(reqItemId, false);
    }

    // Destroy `count` toward a cost in `reqItemId`. For an emblem, consume the
    // exact tier first, then higher tiers ascending (preserving the highest).
    void DeductCost(Player* bot, uint32 reqItemId, uint32 count)
    {
        if (!EmblemTiers::IsEmblem(reqItemId))
        {
            bot->DestroyItemCount(reqItemId, count, true);
            return;
        }

        uint32 remaining = count;
        for (uint32 e : EmblemTiers::AtOrAbove(reqItemId))
        {
            if (remaining == 0)
                break;
            uint32 have = bot->GetItemCount(e, false);
            uint32 take = std::min(have, remaining);
            if (take)
            {
                bot->DestroyItemCount(e, take, true);
                remaining -= take;
            }
        }
    }
}

bool EmblemShopMgr::BuyForBot(Player* bot, uint32 itemId, uint32 extendedCostId, std::string& errOut)
{
    if (!bot)
    {
        errOut = "Bot is not online.";
        return false;
    }

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
    {
        std::ostringstream o; o << "Unknown item id " << itemId << ".";
        errOut = o.str();
        return false;
    }

    ItemExtendedCostEntry const* ec = sItemExtendedCostStore.LookupEntry(extendedCostId);
    if (!ec)
    {
        std::ostringstream o; o << "Unknown extended-cost id " << extendedCostId << ".";
        errOut = o.str();
        return false;
    }

    if (proto->RequiredLevel > bot->GetLevel())
    {
        std::ostringstream o;
        o << bot->GetName() << " is too low level for " << proto->Name1
          << " (needs " << proto->RequiredLevel << ", is " << uint32(bot->GetLevel()) << ").";
        errOut = o.str();
        return false;
    }

    // Comprehensive usability gate: class/race masks AND weapon/armor type
    // proficiency. CanUseItem(proto) covers class/race/RequiredSkill/spell, but
    // weapon-type proficiency (thrown/bow/gun, plate/mail, …) is normally only
    // enforced at equip time via the item's GetSkill() — so without this a
    // caster could buy throwing knives (AllowableClass is often 0 = "any").
    if (bot->CanUseItem(proto) != EQUIP_ERR_OK ||
        (proto->GetSkill() != 0 && bot->GetSkillValue(proto->GetSkill()) == 0))
    {
        errOut = proto->Name1 + " is not usable by " + bot->GetName() + ".";
        return false;
    }

    // Currency: token-item requirements.
    for (uint32 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
    {
        if (!ec->reqitem[i])
            continue;

        uint32 have = SpendableCount(bot, ec->reqitem[i]);
        if (have < ec->reqitemcount[i])
        {
            std::ostringstream o;
            o << bot->GetName() << " needs " << ec->reqitemcount[i]
              << "x " << ItemNameOr(ec->reqitem[i], "token")
              << " (has " << have << ").";
            errOut = o.str();
            return false;
        }
    }

    if (ec->reqhonorpoints && bot->GetHonorPoints() < ec->reqhonorpoints)
    {
        std::ostringstream o;
        o << bot->GetName() << " needs " << ec->reqhonorpoints
          << " honor (has " << bot->GetHonorPoints() << ").";
        errOut = o.str();
        return false;
    }

    if (ec->reqarenapoints && bot->GetArenaPoints() < ec->reqarenapoints)
    {
        std::ostringstream o;
        o << bot->GetName() << " needs " << ec->reqarenapoints
          << " arena points (has " << bot->GetArenaPoints() << ").";
        errOut = o.str();
        return false;
    }

    if (ec->reqpersonalarenarating
        && bot->GetMaxPersonalArenaRatingRequirement(ec->reqarenaslot) < ec->reqpersonalarenarating)
    {
        std::ostringstream o;
        o << bot->GetName() << " lacks the personal arena rating ("
          << ec->reqpersonalarenarating << ") for this item.";
        errOut = o.str();
        return false;
    }

    // Bag space — quantity is always 1 for emblem gear.
    ItemPosCountVec dest;
    InventoryResult msg = bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
    if (msg != EQUIP_ERR_OK)
    {
        errOut = bot->GetName() + "'s bags are full.";
        return false;
    }

    // All checks passed — deduct in fixed order: token items, point pools, then
    // store. Emblem costs may be paid with higher-tier emblems (down-conversion).
    for (uint32 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        if (ec->reqitem[i])
            DeductCost(bot, ec->reqitem[i], ec->reqitemcount[i]);

    if (ec->reqhonorpoints)
        bot->ModifyHonorPoints(-int32(ec->reqhonorpoints));
    if (ec->reqarenapoints)
        bot->ModifyArenaPoints(-int32(ec->reqarenapoints));

    Item* item = bot->StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
    if (!item)
    {
        LOG_ERROR("playerbots",
            "EmblemShopMgr: StoreNewItem returned null for bot {} itemId {} after currency deducted!",
            bot->GetName(), itemId);
        errOut = "Internal error storing item — please check server log.";
        return false;
    }

    bot->SendNewItem(item, 1, true, false);

    // Nudge the bot to auto-equip the new piece on its next AI tick. Same
    // hook BuyAction uses after a gold purchase (BuyAction.cpp:178). Without
    // this the item sits in bags until the maintenance strategy picks it up.
    if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
        ai->DoSpecificAction("equip upgrades packet action");

    return true;
}
