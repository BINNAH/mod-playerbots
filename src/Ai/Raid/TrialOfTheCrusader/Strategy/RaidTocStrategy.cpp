#include "RaidTocStrategy.h"

#include "Playerbots.h"
#include "RaidBossHelpers.h"      // GetFirstAliveUnitByEntry
#include "RaidTocBossHelper.h"
// Action classes we suppress against snobolds (taunts + crowd control) and the
// DPS burst cooldowns / trinkets we hold for the Dreadscale burn.
#include "ChooseTargetActions.h"  // DpsAssistAction / DpsAoeAction (keep melee on the boss)
#include "DKActions.h"            // CastDarkCommandAction, CastArmyOfTheDead/SummonGargoyle
#include "DruidActions.h"         // CastHibernateAction / CastHibernateCcAction, CastForceOfNature
#include "DruidBearActions.h"     // CastGrowlAction
#include "GenericSpellActions.h"  // CastCrowdControlSpellAction, CastBerserking/BloodFury, UseTrinket
#include "HunterActions.h"        // CastFreezingTrap / CastWyvernStingAction, CastRapidFire/Readiness
#include "MageActions.h"          // CastPolymorphAction, CastIcyVeins/ColdSnap/ArcanePower/PoM/Combustion
#include "PaladinActions.h"       // CastHandOfReckoningAction, CastAvengingWrath
#include "RogueActions.h"         // CastAdrenalineRushAction / CastBladeFlurryAction
#include "RogueOpeningActions.h"  // CastSapAction
#include "ShamanActions.h"        // CastBloodlust/Heroism/ElementalMastery/FeralSpirit/FireElementalTotem
#include "WarlockActions.h"       // CastMetamorphosisAction
#include "WarriorActions.h"       // CastTauntAction

float GormokSnoboldFocusMultiplier::GetValue(Action* action)
{
    Unit* target = botAI->GetUnit(bot->GetTarget());
    if (!target || target->GetEntry() != NPC_SNOBOLD_VASSAL)
        return 1.0f;

    // Targeting a snobold: allow only damage. Kill taunts (it rides a player and
    // can't be tanked) and crowd control (it's immune) so the GCD goes to nuking.
    if (dynamic_cast<CastTauntAction*>(action) ||
        dynamic_cast<CastDarkCommandAction*>(action) ||
        dynamic_cast<CastGrowlAction*>(action) ||
        dynamic_cast<CastHandOfReckoningAction*>(action) ||
        dynamic_cast<CastCrowdControlSpellAction*>(action) ||
        dynamic_cast<CastPolymorphAction*>(action) ||
        dynamic_cast<CastFreezingTrap*>(action) ||
        dynamic_cast<CastWyvernStingAction*>(action) ||
        dynamic_cast<CastSapAction*>(action) ||
        dynamic_cast<CastHibernateAction*>(action) ||
        dynamic_cast<CastHibernateCcAction*>(action))
        return 0.0f;

    return 1.0f;
}

float TocSaveCooldownsForWormsMultiplier::GetValue(Action* action)
{
    // Only gate while Gormok (phase 1) is still up. Once the worms are out the
    // raid should dump everything on Dreadscale, so the gate lifts entirely.
    if (!GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER))
        return 1.0f;

    // Hold Bloodlust/Heroism regardless of role — it's a raid-wide, once-per-
    // fight cooldown (Sated/Exhaustion locks it for 10 min), so it must land on
    // the Dreadscale burn, not on Gormok.
    if (dynamic_cast<CastBloodlustAction*>(action) ||
        dynamic_cast<CastHeroismAction*>(action))
        return 0.0f;

    // The rest are DPS burst cooldowns/trinkets — only gate them on DPS so
    // tanks/healers keep their defensive cooldowns available in phase 1.
    if (!botAI->IsDps(bot))
        return 1.0f;

    if (dynamic_cast<CastMetamorphosisAction*>(action) ||
        dynamic_cast<CastAdrenalineRushAction*>(action) ||
        dynamic_cast<CastBladeFlurryAction*>(action) ||
        dynamic_cast<CastIcyVeinsAction*>(action) ||
        dynamic_cast<CastColdSnapAction*>(action) ||
        dynamic_cast<CastArcanePowerAction*>(action) ||
        dynamic_cast<CastPresenceOfMindAction*>(action) ||
        dynamic_cast<CastCombustionAction*>(action) ||
        dynamic_cast<CastRapidFireAction*>(action) ||
        dynamic_cast<CastReadinessAction*>(action) ||
        dynamic_cast<CastAvengingWrathAction*>(action) ||
        dynamic_cast<CastElementalMasteryAction*>(action) ||
        dynamic_cast<CastFeralSpiritAction*>(action) ||
        dynamic_cast<CastFireElementalTotemAction*>(action) ||
        dynamic_cast<CastFireElementalTotemMeleeAction*>(action) ||
        dynamic_cast<CastForceOfNatureAction*>(action) ||
        dynamic_cast<CastArmyOfTheDeadAction*>(action) ||
        dynamic_cast<CastSummonGargoyleAction*>(action) ||
        dynamic_cast<CastBerserkingAction*>(action) ||
        dynamic_cast<CastBloodFuryAction*>(action) ||
        dynamic_cast<UseTrinketAction*>(action))
        return 0.0f;

    return 1.0f;
}

float GormokKeepMeleeOnBossMultiplier::GetValue(Action* action)
{
    // Melee DPS only — ranged peel to Snobolds, tanks/healers do their own thing.
    if (botAI->IsTank(bot) || !botAI->IsDps(bot) || !botAI->IsMelee(bot))
        return 1.0f;

    if (!GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER))
        return 1.0f;

    // Suppress the generic target pickers so they stop dragging melee onto loose
    // Snobolds; "gormok melee focus boss" keeps them on Gormok instead.
    if (dynamic_cast<DpsAssistAction*>(action) ||
        dynamic_cast<DpsAoeAction*>(action))
        return 0.0f;

    return 1.0f;
}

float WormsFocusDreadscaleMultiplier::GetValue(Action* action)
{
    // DPS only — tanks hold the worms, healers heal.
    if (botAI->IsTank(bot) || !botAI->IsDps(bot))
        return 1.0f;

    // Only while Dreadscale is alive (the focus window). Once it dies, dps-assist
    // resumes so the raid naturally rolls onto Acidmaw.
    if (!GetFirstAliveUnitByEntry(botAI, NPC_DREADSCALE))
        return 1.0f;

    // Suppress the generic target pickers so they stop dragging DPS onto Acidmaw;
    // "worms focus dreadscale" keeps everyone on Dreadscale instead.
    if (dynamic_cast<DpsAssistAction*>(action) ||
        dynamic_cast<DpsAoeAction*>(action))
        return 0.0f;

    return 1.0f;
}

void RaidTocStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GormokSnoboldFocusMultiplier(botAI));
    multipliers.push_back(new GormokKeepMeleeOnBossMultiplier(botAI));
    multipliers.push_back(new WormsFocusDreadscaleMultiplier(botAI));
    multipliers.push_back(new TocSaveCooldownsForWormsMultiplier(botAI));
}

void RaidTocStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    //
    // Gormok the Impaler (Northrend Beasts phase 1)
    //
    triggers.push_back(new TriggerNode(
        "gormok near fire bomb",
        { NextAction("gormok move away fire bomb", ACTION_EMERGENCY + 1) }));

    // Ranged DPS peel to Snobolds; melee stay on Gormok and cleave them down.
    triggers.push_back(new TriggerNode(
        "gormok snobold up",
        { NextAction("gormok attack snobold", ACTION_RAID + 1) }));

    // Off-tank taunts Gormok off the active tank when Impale stacks pile up.
    triggers.push_back(new TriggerNode(
        "gormok impale tank swap",
        { NextAction("gormok tank swap taunt", ACTION_RAID + 2) }));

    // The more-Impaled off-tank stops generating threat so the swap sticks
    // (below the taunt at +2, above the threat rotation).
    triggers.push_back(new TriggerNode(
        "gormok off tank backoff",
        { NextAction("gormok off tank backoff", ACTION_RAID) }));

    // Any tank that drifts onto a Snobold snaps back to Gormok (never chase
    // them). Just under the back-off so a resting off-tank keeps resting.
    triggers.push_back(new TriggerNode(
        "gormok tank off snobold",
        { NextAction("gormok tank off snobold", ACTION_RAID - 1) }));

    // Melee DPS lock onto Gormok and pump (ranged kill the Snobolds). Self-limiting
    // — only re-acquires the boss; the keep-melee-on-boss multiplier suppresses the
    // generic dps-assist so melee don't ping-pong between Gormok and a Snobold.
    triggers.push_back(new TriggerNode(
        "gormok melee focus boss",
        { NextAction("gormok melee focus boss", ACTION_RAID + 1) }));

    // Paladin-tank emergency backup: BoP self to wipe a runaway Impale stack,
    // then drop the BoP again immediately so we keep tanking.
    triggers.push_back(new TriggerNode(
        "gormok impale self bop",
        { NextAction("gormok self bop clear impale", ACTION_EMERGENCY + 1) }));
    triggers.push_back(new TriggerNode(
        "gormok remove self bop",
        { NextAction("gormok remove self bop", ACTION_EMERGENCY + 2) }));

    // Ranged/healers fan onto a ring so Snobold Fire Bombs can't chain.
    triggers.push_back(new TriggerNode(
        "gormok spread",
        { NextAction("gormok spread", ACTION_RAID) }));

    // Whoever has a Snobold riding them runs it into Gormok's center so the
    // whole raid can kill it. Above spread/snobold-attack, below emergencies.
    triggers.push_back(new TriggerNode(
        "gormok snobbled run in",
        { NextAction("gormok snobbled run in", ACTION_RAID + 4) }));

    //
    // Acidmaw & Dreadscale (Northrend Beasts phase 2)
    //
    // Burn Dreadscale first: DPS lock onto it until it dies, then roll to
    // Acidmaw. Low relevance (target nudge) so survival movement always wins.
    triggers.push_back(new TriggerNode(
        "worms focus dreadscale",
        { NextAction("worms focus dreadscale", ACTION_RAID + 1) }));

    // Paralytic Toxin ramps to a full paralysis -> run to a Burning Bile carrier.
    triggers.push_back(new TriggerNode(
        "worms paralytic toxin",
        { NextAction("worms run to burning bile", ACTION_EMERGENCY + 2) }));

    triggers.push_back(new TriggerNode(
        "worms slime pool",
        { NextAction("worms avoid slime pool", ACTION_EMERGENCY + 1) }));

    // Worm tank points the frontal spew (Molten/Acid Spew) away from the raid by
    // standing on the far side of the worm. Below slime-pool dodging (survival
    // first), above normal tank positioning. Self-limiting via the angle gate.
    triggers.push_back(new TriggerNode(
        "worms tank face away",
        { NextAction("worms tank face away", ACTION_RAID + 2) }));

    // Ranged/healers (who aren't being cured) step out of Burning Bile's 10y
    // fire AoE — the dominant phase-2 damage. Above spread/snobold, below the
    // life-threatening emergencies.
    triggers.push_back(new TriggerNode(
        "worms avoid burning bile",
        { NextAction("worms avoid burning bile", ACTION_RAID + 3) }));

    //
    // Icehowl (Northrend Beasts phase 3)
    //
    // Trample contact = raid-wide enrage, so clearing the lane outranks everything.
    triggers.push_back(new TriggerNode(
        "icehowl charge",
        { NextAction("icehowl dodge charge", ACTION_EMERGENCY + 3) }));

    //
    // Lord Jaraxxus
    //
    // Mages Spellsteal Nether Power off the boss whenever a stack is up — strips
    // his +20% spell damage AND grants the mage a copy. Above the normal rotation
    // (preempts a filler cast) but below survival, so a mage standing in Legion
    // Flame dodges first. Self-limiting: CanCastSpell gates it to one cast per GCD
    // and it goes quiet the instant the boss has no Nether Power left.
    triggers.push_back(new TriggerNode(
        "jaraxxus nether power",
        { NextAction("jaraxxus steal nether power", ACTION_RAID + 2) }));

    // Everyone steps out of Legion Flame ground-fire patches (the one real "don't
    // stand in it" mechanic here). Emergency-tier, same as the Gormok fire bomb.
    triggers.push_back(new TriggerNode(
        "jaraxxus legion flame",
        { NextAction("jaraxxus avoid legion flame", ACTION_EMERGENCY + 1) }));
}
