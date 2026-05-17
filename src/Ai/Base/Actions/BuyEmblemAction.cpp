/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license.
 */

#include "BuyEmblemAction.h"

#include "ChatHelper.h"
#include "EmblemShopMgr.h"
#include "EmblemVendorCache.h"
#include "Event.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Playerbots.h"

#include <cstdlib>
#include <sstream>

bool BuyEmblemAction::Execute(Event event)
{
    std::string const param = event.getParam();
    if (param.empty())
    {
        botAI->TellMaster("Usage: buyemblem <itemId | [item-link]>");
        return false;
    }

    // Accept either a chat-pasted item link or a bare numeric id.
    uint32 itemId = 0;
    ItemIds parsed = chat->parseItems(param);
    if (!parsed.empty())
        itemId = *parsed.begin();
    else
        itemId = static_cast<uint32>(std::atoi(param.c_str()));

    if (!itemId)
    {
        botAI->TellMaster("Could not parse an item id from: " + param);
        return false;
    }

    // Find the first matching offering in the catalog. (Same item from multiple
    // vendors will all share the same ExtendedCost in practice.)
    uint32 chosenCost = 0;
    for (auto const& off : sEmblemVendorCache.AllOfferings())
    {
        if (off.itemId == itemId)
        {
            chosenCost = off.extendedCostId;
            break;
        }
    }
    if (!chosenCost)
    {
        std::ostringstream o;
        o << "Item " << itemId << " is not sold by any token vendor I know of.";
        botAI->TellMaster(o.str());
        return false;
    }

    std::string err;
    if (!sEmblemShopMgr.BuyForBot(bot, itemId, chosenCost, err))
    {
        botAI->TellMaster(err);
        return false;
    }

    ItemTemplate const* p = sObjectMgr->GetItemTemplate(itemId);
    std::ostringstream out;
    out << "Bought " << chat->FormatItem(p);
    botAI->TellMaster(out.str());
    return true;
}
