#ifndef _PLAYERBOT_RAIDTOCSTRATEGY_H
#define _PLAYERBOT_RAIDTOCSTRATEGY_H

#include "Multiplier.h"
#include "Strategy.h"

class RaidTocStrategy : public Strategy
{
public:
    RaidTocStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "toc"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

// When a bot is targeting a Snobold Vassal it should only DAMAGE it — never
// taunt it (vehicle-mounted, un-tankable) or crowd-control it (immune). This
// zeroes out those wasted actions so the raid just nukes the snobold down.
class GormokSnoboldFocusMultiplier : public Multiplier
{
public:
    GormokSnoboldFocusMultiplier(PlayerbotAI* ai) : Multiplier(ai, "gormok snobold focus") {}
    float GetValue(Action* action) override;
};

// Keep melee DPS on Gormok in phase 1. The generic "dps assist" / "dps aoe"
// target pickers keep grabbing loose Snobolds, which fights the melee-focus rule
// and starves the rotation (the bot ping-pongs targets and never swings). Zeroing
// those pickers for melee while Gormok is up removes the competing pull, so the
// focus action acquires Gormok once and the rotation runs. Ranged (who peel to
// Snobolds) and tanks/healers are left untouched.
class GormokKeepMeleeOnBossMultiplier : public Multiplier
{
public:
    GormokKeepMeleeOnBossMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "gormok keep melee on boss") {}
    float GetValue(Action* action) override;
};

// Phase-2 twin of the keep-melee-on-boss rule: while Dreadscale is alive, zero
// the generic "dps assist" / "dps aoe" pickers for DPS so they stop grabbing
// Acidmaw and fighting the Dreadscale focus (the ping-pong left bots re-targeting
// every tick instead of attacking — "standing around"). The focus action acquires
// Dreadscale once; once it dies the gate lifts and DPS roll onto Acidmaw normally.
class WormsFocusDreadscaleMultiplier : public Multiplier
{
public:
    WormsFocusDreadscaleMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "worms focus dreadscale") {}
    float GetValue(Action* action) override;
};

// Hold Bloodlust/Heroism and the major DPS burst cooldowns/trinkets through
// Gormok (phase 1) so they're saved for the Dreadscale burn in phase 2. While
// Gormok is alive these actions are zeroed; once he's dead the gate lifts and
// the raid dumps everything on Dreadscale.
class TocSaveCooldownsForWormsMultiplier : public Multiplier
{
public:
    TocSaveCooldownsForWormsMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "toc save cooldowns for worms") {}
    float GetValue(Action* action) override;
};

#endif
