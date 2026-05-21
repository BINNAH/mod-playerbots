#include "RaidNaxxStrategy.h"

#include "RaidNaxxMultipliers.h"

void RaidNaxxStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Grobbulus
    triggers.push_back(new TriggerNode("mutating injection melee",
        { NextAction("grobbulus move away", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection ranged",
        { NextAction("grobbulus go behind the boss", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("mutating injection removed",
        { NextAction("grobbulus move center", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("grobbulus cloud",
        { NextAction("rotate grobbulus", ACTION_RAID + 1) }
    ));

    // Heigan the Unclean
    // Fast-dance: everyone dances to the predicted safe section using the
    // tight fast-phase schedule (first eruption +7s, every 4s). Highest
    // priority since the cadence leaves little slack.
    triggers.push_back(new TriggerNode("heigan fast dance",
        { NextAction("heigan dance", ACTION_RAID + 3) }
    ));

    // Slow-dance, tank + melee: stay parked on the SE platform. Eruption
    // GOs aren't placed there, so it dodges the AOE entirely while the
    // tank keeps Heigan in melee.
    triggers.push_back(new TriggerNode("heigan slow dance platform",
        { NextAction("heigan platform", ACTION_RAID + 2) }
    ));

    // Slow-dance, ranged: dance to the predicted safe section. Action
    // returns false once in the right wedge so DPS / heal rotations run
    // unimpeded between eruptions.
    triggers.push_back(new TriggerNode("heigan slow dance ranged",
        { NextAction("heigan dance", ACTION_RAID + 2) }
    ));

    // Kel'Thuzad
    // Pet control sits at +3 above position/choose-target — it runs first each
    // tick so the react-state flip happens before the bot's own target pick
    // (which the action then forwards to pets in P1).
    triggers.push_back(
        new TriggerNode("kel'thuzad",
        {
            NextAction("kel'thuzad control pet", ACTION_RAID + 3),
            NextAction("kel'thuzad position", ACTION_RAID + 2),
            NextAction("kel'thuzad choose target", ACTION_RAID + 1)
        })
    );

    // Anub'Rekhan
    triggers.push_back(new TriggerNode("anub'rekhan",
        { NextAction("anub'rekhan position", ACTION_RAID + 1) }
    ));

     // Grand Widow Faerlina
     triggers.push_back(new TriggerNode("faerlina",
        { NextAction("avoid aoe", ACTION_RAID + 1) }
    ));

    // Maexxna
    triggers.push_back(
        new TriggerNode("maexxna",
        {
            NextAction("rear flank", ACTION_RAID + 1),
            NextAction("avoid aoe", ACTION_RAID + 1)
        })
    );

    // DPS swap to the Web Wrap NPC the moment one spawns. +2 priority sits
    // above the default attacker selection so the bot leaves Maexxna; the
    // trigger falls off as soon as the wrap is dead (and the victim freed).
    triggers.push_back(new TriggerNode("maexxna web wrap",
        { NextAction("maexxna attack web wrap", ACTION_RAID + 2) }
    ));

    // Sub-30% Web Spray survival chain. Each trigger fires only while frenzied
    // and in the short window before the next Web Spray, so these long-cooldown
    // saves land BEFORE the raid-wide stun and carry the tank through it.
    // ACTION_RAID + 5 sits above the boss positioning/web-wrap nodes but below
    // emergency healing (ACTION_EMERGENCY), and the casts' own isUseful guards
    // stop them double-firing once the buff is up.
    triggers.push_back(new TriggerNode("maexxna pre web spray hand of sacrifice",
        { NextAction("hand of sacrifice on main tank", ACTION_RAID + 5) }
    ));

    triggers.push_back(new TriggerNode("maexxna pre web spray guardian spirit",
        { NextAction("guardian spirit on main tank", ACTION_RAID + 5) }
    ));

    // One node, one defensive per tank class — only the action matching the
    // bot's class resolves, the rest no-op, so the main tank pops exactly one.
    triggers.push_back(new TriggerNode("maexxna pre web spray tank defensive",
        {
            NextAction("shield wall", ACTION_RAID + 5),
            NextAction("icebound fortitude", ACTION_RAID + 5),
            NextAction("survival instincts", ACTION_RAID + 5),
            NextAction("divine protection", ACTION_RAID + 5)
        }
    ));

    // Patchwerk
    //triggers.push_back(new TriggerNode("patchwerk tank",
    //    { NextAction("tank face", ACTION_RAID + 2) }
    //));

    //triggers.push_back(new TriggerNode("patchwerk ranged",
    //    { NextAction("patchwerk ranged position", ACTION_RAID + 2) }
    //));

    //triggers.push_back(new TriggerNode("patchwerk non-tank",
    //    { NextAction("rear flank", ACTION_RAID + 1) }
    //));

    // Thaddius
    triggers.push_back(new TriggerNode("thaddius phase pet",
        { NextAction("thaddius attack nearest pet", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase pet lose aggro",
        { NextAction("taunt spell", ACTION_RAID + 2) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase transition",
        { NextAction("thaddius move to platform", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("thaddius phase thaddius",
        { NextAction("thaddius move polarity", ACTION_RAID + 1) }
    ));

    // Instructor Razuvious
    triggers.push_back(new TriggerNode("razuvious tank",
        { NextAction("razuvious use obedience crystal", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("razuvious nontank",
        { NextAction("razuvious target", ACTION_RAID + 1) }
    ));

    // four horsemen
    // Void-zone avoidance is at ACTION_RAID + 4 so it overrides the attract
    // and attack-in-order actions below; otherwise the bot's preset spot pulls
    // it straight back into Lady Blaumeux's puddle every tick. Mark bleed-off
    // at +3 sits below void avoid (acute damage wins) but above the normal
    // positioning at +1.
    triggers.push_back(new TriggerNode("four horsemen void zone",
        { NextAction("four horsemen avoid void zone", ACTION_RAID + 4) }
    ));

    triggers.push_back(new TriggerNode("four horsemen healer high mark",
        { NextAction("four horsemen healer bleed off mark", ACTION_RAID + 3) }
    ));

    triggers.push_back(new TriggerNode("four horsemen attractors",
        { NextAction("four horsemen attract alternatively", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("four horsemen except attractors",
        { NextAction("four horsemen attack in order", ACTION_RAID + 1) }
    ));

    // Opening burst: every bot pops its personal damage-reduction cooldown.
    // One node, one entry per class — only the action matching the bot's class
    // resolves, the rest no-op (so each bot fires exactly what it has). Curated
    // to instant, non-locking mitigation: nothing here stops the bot from
    // healing/DPSing or sheds threat (no Ice Block / Divine Shield / Dispersion).
    // Sits at +2 — above normal positioning/attack (+1) so it fires promptly,
    // but below mark bleed-off (+3) and void-zone avoid (+4) so survival
    // movement still wins the tick.
    triggers.push_back(new TriggerNode("four horsemen opening defensive",
        {
            NextAction("shield wall", ACTION_RAID + 2),
            NextAction("last stand", ACTION_RAID + 2),
            NextAction("icebound fortitude", ACTION_RAID + 2),
            NextAction("vampiric blood", ACTION_RAID + 2),
            NextAction("survival instincts", ACTION_RAID + 2),
            NextAction("barkskin", ACTION_RAID + 2),
            NextAction("divine protection", ACTION_RAID + 2),
            NextAction("shamanistic rage", ACTION_RAID + 2)
        }
    ));

    // sapphiron
    triggers.push_back(new TriggerNode("sapphiron ground",
        { NextAction("sapphiron ground position", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("sapphiron flight",
        { NextAction("sapphiron flight position", ACTION_RAID + 1) }
    ));

    // First alive paladin flips to Frost Resistance Aura for the fight: helps
    // with the constant Frost Aura tick, Chill, and softens Frost Breath. The
    // trigger/action (shared boss-aura subsystem) self-gate to paladins who
    // know the spell, in a raid group, and aren't already running it.
    triggers.push_back(new TriggerNode("sapphiron frost resistance trigger",
        { NextAction("sapphiron frost resistance action", ACTION_RAID) }
    ));

    // Gluth
    triggers.push_back(
        new TriggerNode("gluth",
        {
            NextAction("gluth choose target", ACTION_RAID + 1),
            NextAction("gluth position", ACTION_RAID + 1),
            NextAction("gluth slowdown", ACTION_RAID)
        })
    );

    triggers.push_back(new TriggerNode("gluth main tank mortal wound",
        { NextAction("taunt spell", ACTION_RAID + 1) }
    ));

    // Loatheb
    triggers.push_back(
        new TriggerNode("loatheb",
        {
            NextAction("loatheb position", ACTION_RAID + 1),
            NextAction("loatheb choose target", ACTION_RAID + 1)
        })
    );

    // Noth the Plaguebringer
    // Off-tank sweeps up the summoned adds and drags them onto the main tank /
    // Noth so the raid can cleave them. +2 sits above default attacker
    // selection and movement so the gather/taunt/reposition wins.
    triggers.push_back(new TriggerNode("noth add tank",
        { NextAction("noth tank adds", ACTION_RAID + 2) }
    ));

}

void RaidNaxxStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GrobbulusMultiplier(botAI));
    multipliers.push_back(new HeiganDanceMultiplier(botAI));
    multipliers.push_back(new LoathebGenericMultiplier(botAI));
    multipliers.push_back(new ThaddiusGenericMultiplier(botAI));
    multipliers.push_back(new SapphironGenericMultiplier(botAI));
    multipliers.push_back(new InstructorRazuviousGenericMultiplier(botAI));
    multipliers.push_back(new KelthuzadGenericMultiplier(botAI));
    multipliers.push_back(new AnubrekhanGenericMultiplier(botAI));
    multipliers.push_back(new FourHorsemenGenericMultiplier(botAI));
    // multipliers.push_back(new GothikGenericMultiplier(botAI));
    multipliers.push_back(new GluthGenericMultiplier(botAI));
    multipliers.push_back(new MaexxnaGenericMultiplier(botAI));
}
