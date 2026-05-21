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

// Ranged/healers are off their assigned spread ring slot.
class GormokSpreadTrigger : public Trigger
{
public:
    GormokSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "gormok spread") {}
    bool IsActive() override;
};

// ---- Acidmaw & Dreadscale (Northrend Beasts phase 2) ----

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

// ---- Icehowl (Northrend Beasts phase 3) ----

// Icehowl is about to Trample-charge; everyone must clear the lane.
class IcehowlChargeTrigger : public Trigger
{
public:
    IcehowlChargeTrigger(PlayerbotAI* ai) : Trigger(ai, "icehowl charge") {}
    bool IsActive() override;
};

#endif
