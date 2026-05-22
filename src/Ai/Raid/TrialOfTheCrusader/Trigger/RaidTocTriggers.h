#ifndef _PLAYERBOT_RAIDTOCTRIGGERS_H
#define _PLAYERBOT_RAIDTOCTRIGGERS_H

#include "PlayerbotAI.h"
#include "Trigger.h"

// ---- Gormok the Impaler (Northrend Beasts phase 1) ----

class GormokNearFireBombTrigger : public Trigger
{
public:
    GormokNearFireBombTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok near fire bomb") {}
    bool IsActive() override;
};

class GormokSnoboldUpTrigger : public Trigger
{
public:
    GormokSnoboldUpTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok snobold up") {}
    bool IsActive() override;
};

// Off-tank should taunt Gormok off the active tank when Impale stacks get high.
class GormokImpaleTankSwapTrigger : public Trigger
{
public:
    GormokImpaleTankSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok impale tank swap") {}
    bool IsActive() override;
};

// I'm the more-Impaled off-tank — hold off on threat so a swap to the active
// tank actually sticks (the boss doesn't snap back to me).
class GormokOffTankBackoffTrigger : public Trigger
{
public:
    GormokOffTankBackoffTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok off tank backoff") {}
    bool IsActive() override;
};

// A tank has targeted a Snobold — tanks must stay on Gormok, never chase them.
class GormokTankOffSnoboldTrigger : public Trigger
{
public:
    GormokTankOffSnoboldTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok tank off snobold") {}
    bool IsActive() override;
};

// Melee DPS isn't on Gormok — pull them back so they pump the boss (ranged kill
// the Snobolds). Paired with the keep-melee-on-boss multiplier (suppresses the
// generic dps-assist that otherwise drags melee onto Snobolds).
class GormokMeleeFocusBossTrigger : public Trigger
{
public:
    GormokMeleeFocusBossTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok melee focus boss") {}
    bool IsActive() override;
};

// Ranged/healers are off their assigned spread ring slot.
class GormokSpreadTrigger : public Trigger
{
public:
    GormokSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok spread") {}
    bool IsActive() override;
};

// I have a Snobold riding me — run it into the raid so everyone can kill it.
class GormokSnobbledRunInTrigger : public Trigger
{
public:
    GormokSnobbledRunInTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok snobbled run in") {}
    bool IsActive() override;
};

// ---- Acidmaw & Dreadscale (Northrend Beasts phase 2) ----

// I'm a DPS and Dreadscale is alive but not my current target — focus it down.
class WormsFocusDreadscaleTrigger : public Trigger
{
public:
    WormsFocusDreadscaleTrigger(PlayerbotAI* ai) : Trigger(ai, "worms focus dreadscale") {}
    bool IsActive() override;
};

// I'm afflicted with Paralytic Toxin and an ally has Burning Bile to cure me.
class WormsParalyticToxinTrigger : public Trigger
{
public:
    WormsParalyticToxinTrigger(PlayerbotAI* ai) : Trigger(ai, "worms paralytic toxin") {}
    bool IsActive() override;
};

// A Slime Pool is dangerously close.
class WormsSlimePoolTrigger : public Trigger
{
public:
    WormsSlimePoolTrigger(PlayerbotAI* ai) : Trigger(ai, "worms slime pool") {}
    bool IsActive() override;
};

// I'm tanking a worm and it's facing the raid — its spew cone would hit them.
class WormsTankFaceAwayTrigger : public Trigger
{
public:
    WormsTankFaceAwayTrigger(PlayerbotAI* ai) : Trigger(ai, "worms tank face away") {}
    bool IsActive() override;
};

// A Burning Bile carrier (10y fire AoE) is too close and I don't need curing.
class WormsAvoidBurningBileTrigger : public Trigger
{
public:
    WormsAvoidBurningBileTrigger(PlayerbotAI* ai) : Trigger(ai, "worms avoid burning bile") {}
    bool IsActive() override;
};

// ---- Icehowl (Northrend Beasts phase 3) ----

// Icehowl is about to Trample-charge; everyone must clear the lane.
class IcehowlChargeTrigger : public Trigger
{
public:
    IcehowlChargeTrigger(PlayerbotAI* ai) : Trigger(ai, "icehowl charge") {}
    bool IsActive() override;
};

// ---- Gormok: paladin-tank emergency Impale clear ----

// Paladin tank with a dangerous Impale stack should BoP itself to wipe the bleed.
class GormokImpaleSelfBopTrigger : public Trigger
{
public:
    GormokImpaleSelfBopTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok impale self bop") {}
    bool IsActive() override;
};

// Drop that self-BoP again immediately once it has done its job.
class GormokRemoveSelfBopTrigger : public Trigger
{
public:
    GormokRemoveSelfBopTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok remove self bop") {}
    bool IsActive() override;
};

#endif
