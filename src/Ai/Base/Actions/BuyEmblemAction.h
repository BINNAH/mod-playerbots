/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license.
 */

#ifndef _PLAYERBOT_BUYEMBLEMACTION_H
#define _PLAYERBOT_BUYEMBLEMACTION_H

#include "InventoryAction.h"

class PlayerbotAI;

class BuyEmblemAction : public InventoryAction
{
public:
    BuyEmblemAction(PlayerbotAI* botAI) : InventoryAction(botAI, "buyemblem") {}

    bool Execute(Event event) override;
};

#endif
