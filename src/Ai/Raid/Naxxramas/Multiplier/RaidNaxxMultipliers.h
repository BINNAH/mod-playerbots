
#ifndef _PLAYERBOT_RAIDNAXXMULTIPLIERS_H
#define _PLAYERBOT_RAIDNAXXMULTIPLIERS_H

#include "Multiplier.h"
#include "RaidNaxxBossHelper.h"

class GrobbulusMultiplier : public Multiplier
{
public:
    GrobbulusMultiplier(PlayerbotAI* ai) : Multiplier(ai, "grobbulus") {}

public:
    virtual float GetValue(Action* action);
};

class HeiganDanceMultiplier : public Multiplier
{
public:
    HeiganDanceMultiplier(PlayerbotAI* ai) : Multiplier(ai, "heigan dance") {}

public:
    virtual float GetValue(Action* action);
};

class LoathebGenericMultiplier : public Multiplier
{
public:
    LoathebGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "loatheb generic") {}

public:
    virtual float GetValue(Action* action);
};

class ThaddiusGenericMultiplier : public Multiplier
{
public:
    ThaddiusGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thaddius generic"), helper(ai) {}

public:
    virtual float GetValue(Action* action);

private:
    ThaddiusBossHelper helper;
};

class SapphironGenericMultiplier : public Multiplier
{
public:
    SapphironGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sapphiron generic"), helper(ai) {}

    virtual float GetValue(Action* action);

private:
    SapphironBossHelper helper;
};

class InstructorRazuviousGenericMultiplier : public Multiplier
{
public:
    InstructorRazuviousGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "instructor razuvious generic"), helper(ai) {}
    virtual float GetValue(Action* action);

private:
    RazuviousBossHelper helper;
};

class KelthuzadGenericMultiplier : public Multiplier
{
public:
    KelthuzadGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kelthuzad generic"), helper(ai) {}
    virtual float GetValue(Action* action);

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanGenericMultiplier : public Multiplier
{
public:
    AnubrekhanGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "anubrekhan generic") {}

public:
    virtual float GetValue(Action* action);
};

class FourHorsemenGenericMultiplier : public Multiplier
{
public:
    FourHorsemenGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "four horsemen generic"), helper(ai) {}

public:
    virtual float GetValue(Action* action);

private:
    FourHorsemenBossHelper helper;
};

// class GothikGenericMultiplier : public Multiplier
// {
// public:
//     GothikGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gothik generic") {}

// public:
//     virtual float GetValue(Action* action);
// };

class GluthGenericMultiplier : public Multiplier
{
public:
    GluthGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gluth generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    GluthBossHelper helper;
};

// Maexxna does two jobs:
//  1. Web Wrap target lock — while a Web Wrap NPC is alive, suppress the default
//     DpsAssist target re-selection so the web-wrap action's swap onto the wrap
//     sticks instead of flickering back to the boss every tick.
//  2. Cooldown banking — hold the raid's burst cooldowns (and, with a low-HP
//     safety valve, the tanks' defensives) until the boss enters her sub-30%
//     Frenzy, then release everything for the burn.
class MaexxnaGenericMultiplier : public Multiplier
{
public:
    MaexxnaGenericMultiplier(PlayerbotAI* ai) : Multiplier(ai, "maexxna generic"), helper(ai) {}
    float GetValue(Action* action) override;

private:
    // Tanks stop holding their defensives if they fall to this HP% before 30%,
    // so a bot never dies sitting on an unused cooldown.
    static constexpr float TANK_DEFENSIVE_HP_FLOOR = 50.0f;
    MaexxnaBossHelper helper;
};

#endif
