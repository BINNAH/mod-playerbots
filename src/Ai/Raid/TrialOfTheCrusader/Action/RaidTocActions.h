#ifndef _PLAYERBOT_RAIDTOCACTIONS_H
#define _PLAYERBOT_RAIDTOCACTIONS_H

#include "AttackAction.h"
#include "MovementActions.h"

class PlayerbotAI;

// ---- Gormok the Impaler (Northrend Beasts phase 1) ----

class GormokMoveAwayFireBombAction : public MovementAction
{
public:
    GormokMoveAwayFireBombAction(PlayerbotAI* ai)
        : MovementAction(ai, "gormok move away fire bomb") {}
    bool Execute(Event event) override;
};

class GormokAttackSnoboldAction : public AttackAction
{
public:
    GormokAttackSnoboldAction(PlayerbotAI* ai)
        : AttackAction(ai, "gormok attack snobold") {}
    bool Execute(Event event) override;
};

// Off-tank taunts Gormok off the active tank to bleed off their Impale stacks.
class GormokTankSwapTauntAction : public AttackAction
{
public:
    GormokTankSwapTauntAction(PlayerbotAI* ai)
        : AttackAction(ai, "gormok tank swap taunt") {}
    bool Execute(Event event) override;
};

// Move ranged/healers onto their assigned spread ring slot.
class GormokSpreadAction : public MovementAction
{
public:
    GormokSpreadAction(PlayerbotAI* ai)
        : MovementAction(ai, "gormok spread") {}
    bool Execute(Event event) override;
};

// ---- Acidmaw & Dreadscale (Northrend Beasts phase 2) ----

// Run to the nearest ally carrying Burning Bile to neutralize Paralytic Toxin.
class WormsRunToBurningBileAction : public MovementAction
{
public:
    WormsRunToBurningBileAction(PlayerbotAI* ai)
        : MovementAction(ai, "worms run to burning bile") {}
    bool Execute(Event event) override;
};

class WormsAvoidSlimePoolAction : public MovementAction
{
public:
    WormsAvoidSlimePoolAction(PlayerbotAI* ai)
        : MovementAction(ai, "worms avoid slime pool") {}
    bool Execute(Event event) override;
};

// ---- Icehowl (Northrend Beasts phase 3) ----

// Step perpendicular off Icehowl's Trample charge lane.
class IcehowlDodgeChargeAction : public MovementAction
{
public:
    IcehowlDodgeChargeAction(PlayerbotAI* ai)
        : MovementAction(ai, "icehowl dodge charge") {}
    bool Execute(Event event) override;
};

#endif
