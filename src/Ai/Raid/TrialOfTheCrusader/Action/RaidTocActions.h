#ifndef _PLAYERBOT_RAIDTOCACTIONS_H
#define _PLAYERBOT_RAIDTOCACTIONS_H

#include "Action.h"
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

// The more-Impaled off-tank stops attacking so it doesn't pull the boss back.
class GormokOffTankBackoffAction : public Action
{
public:
    GormokOffTankBackoffAction(PlayerbotAI* ai)
        : Action(ai, "gormok off tank backoff") {}
    bool Execute(Event event) override;
};

// Snap a tank that's wandered onto a Snobold back to Gormok.
class GormokTankOffSnoboldAction : public AttackAction
{
public:
    GormokTankOffSnoboldAction(PlayerbotAI* ai)
        : AttackAction(ai, "gormok tank off snobold") {}
    bool Execute(Event event) override;
};

// Keep melee DPS locked on Gormok (re-acquire him if they've drifted off) so
// their rotation pumps the boss; ranged + incidental cleave handle the Snobold.
class GormokMeleeFocusBossAction : public AttackAction
{
public:
    GormokMeleeFocusBossAction(PlayerbotAI* ai)
        : AttackAction(ai, "gormok melee focus boss") {}
    bool Execute(Event event) override;
};

// Keep ranged/healers spread apart (spacing-based).
class GormokSpreadAction : public MovementAction
{
public:
    GormokSpreadAction(PlayerbotAI* ai)
        : MovementAction(ai, "gormok spread") {}
    bool Execute(Event event) override;
};

// Run the Snobold riding me into Gormok's center so the raid can kill it.
class GormokSnobbledRunInAction : public MovementAction
{
public:
    GormokSnobbledRunInAction(PlayerbotAI* ai)
        : MovementAction(ai, "gormok snobbled run in") {}
    bool Execute(Event event) override;
};

// ---- Acidmaw & Dreadscale (Northrend Beasts phase 2) ----

// Focus DPS onto Dreadscale until it dies (burn it before Acidmaw). Snaps the
// bot's target back to Dreadscale whenever it has drifted onto Acidmaw/an add.
class WormsFocusDreadscaleAction : public AttackAction
{
public:
    WormsFocusDreadscaleAction(PlayerbotAI* ai)
        : AttackAction(ai, "worms focus dreadscale") {}
    bool Execute(Event event) override;
};

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

// Worm tank: stand on the far side of the worm from the raid so its frontal spew
// (Molten/Acid Spew) cone points away from everyone.
class WormsTankFaceAwayAction : public MovementAction
{
public:
    WormsTankFaceAwayAction(PlayerbotAI* ai)
        : MovementAction(ai, "worms tank face away") {}
    bool Execute(Event event) override;
};

// Ranged/healers step out of a Burning Bile carrier's 10y fire AoE.
class WormsAvoidBurningBileAction : public MovementAction
{
public:
    WormsAvoidBurningBileAction(PlayerbotAI* ai)
        : MovementAction(ai, "worms avoid burning bile") {}
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

// ---- Gormok: paladin-tank emergency Impale clear ----

// Cast Hand of Protection on self — the physical immunity strips the Impale bleed.
class GormokSelfBopClearImpaleAction : public Action
{
public:
    GormokSelfBopClearImpaleAction(PlayerbotAI* ai)
        : Action(ai, "gormok self bop clear impale") {}
    bool Execute(Event event) override;
};

// Remove the self-cast BoP right away so the tank keeps threat.
class GormokRemoveSelfBopAction : public Action
{
public:
    GormokRemoveSelfBopAction(PlayerbotAI* ai)
        : Action(ai, "gormok remove self bop") {}
    bool Execute(Event event) override;
};

#endif
