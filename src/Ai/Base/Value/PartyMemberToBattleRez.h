/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_PARTYMEMBERTOBATTLEREZ_H
#define _PLAYERBOT_PARTYMEMBERTOBATTLEREZ_H

#include "PartyMemberValue.h"

class PlayerbotAI;
class Unit;

// Target value used by the druid Rebirth (battle rez) action only. Unlike the
// shared "party member to resurrect" value (master -> healer -> tank -> other),
// this prioritises keeping the raid standing during combat:
//   main tank -> off/assist tank -> any other tank -> healers -> rez-capable druids.
// DPS are intentionally NOT eligible, so a precious combat rez is never spent on
// them. Dead non-healer druids are only considered when their own Rebirth is off
// cooldown, so rezzing them actually restores battle-rez capacity.
class PartyMemberToBattleRez : public PartyMemberValue
{
public:
    PartyMemberToBattleRez(PlayerbotAI* botAI, std::string const name = "party member to battle rez")
        : PartyMemberValue(botAI, name)
    {
    }

protected:
    Unit* Calculate() override;
};

#endif
