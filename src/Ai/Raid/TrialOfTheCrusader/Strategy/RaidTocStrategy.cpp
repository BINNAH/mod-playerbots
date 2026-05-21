#include "RaidTocStrategy.h"

void RaidTocStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Gormok the Impaler (Northrend Beasts phase 1)
    //
    triggers.push_back(new TriggerNode(
        "gormok near fire bomb",
        { NextAction("gormok move away fire bomb", ACTION_EMERGENCY + 1) }));

    triggers.push_back(new TriggerNode(
        "gormok snobold up",
        { NextAction("gormok attack snobold", ACTION_RAID + 1) }));

    // Off-tank taunts Gormok off the active tank when Impale stacks pile up.
    triggers.push_back(new TriggerNode(
        "gormok impale tank swap",
        { NextAction("gormok tank swap taunt", ACTION_RAID + 2) }));

    // Ranged/healers fan onto a ring so Snobold Fire Bombs can't chain.
    triggers.push_back(new TriggerNode(
        "gormok spread",
        { NextAction("gormok spread", ACTION_RAID) }));

    //
    // Acidmaw & Dreadscale (Northrend Beasts phase 2)
    //
    // Paralytic Toxin ramps to a full paralysis -> run to a Burning Bile carrier.
    triggers.push_back(new TriggerNode(
        "worms paralytic toxin",
        { NextAction("worms run to burning bile", ACTION_EMERGENCY + 2) }));

    triggers.push_back(new TriggerNode(
        "worms slime pool",
        { NextAction("worms avoid slime pool", ACTION_EMERGENCY + 1) }));

    //
    // Icehowl (Northrend Beasts phase 3)
    //
    // Trample contact = raid-wide enrage, so clearing the lane outranks everything.
    triggers.push_back(new TriggerNode(
        "icehowl charge",
        { NextAction("icehowl dodge charge", ACTION_EMERGENCY + 3) }));
}
