
#ifndef _PLAYERBOT_RAIDNAXXTRIGGERS_H
#define _PLAYERBOT_RAIDNAXXTRIGGERS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "PlayerbotAIConfig.h"
#include "RaidNaxxBossHelper.h"
#include "Trigger.h"

class MutatingInjectionTrigger : public HasAuraTrigger
{
public:
    MutatingInjectionTrigger(PlayerbotAI* ai) : HasAuraTrigger(ai, "mutating injection", 1) {}
};

class MutatingInjectionMeleeTrigger : public MutatingInjectionTrigger
{
public:
    MutatingInjectionMeleeTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class MutatingInjectionRangedTrigger : public MutatingInjectionTrigger
{
public:
    MutatingInjectionRangedTrigger(PlayerbotAI* ai) : MutatingInjectionTrigger(ai) {}
    bool IsActive() override;
};

class AuraRemovedTrigger : public Trigger
{
public:
    AuraRemovedTrigger(PlayerbotAI* botAI, std::string name) : Trigger(botAI, name, 1)
    {
        this->prev_check = false;
    }
    virtual bool IsActive() override;

protected:
    bool prev_check;
};

class MutatingInjectionRemovedTrigger : public HasNoAuraTrigger
{
public:
    MutatingInjectionRemovedTrigger(PlayerbotAI* ai) : HasNoAuraTrigger(ai, "mutating injection") {}
    virtual bool IsActive();
};

class GrobbulusCloudTrigger : public Trigger
{
public:
    GrobbulusCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "grobbulus cloud event"), last_cloud_ms(0) {}
    bool IsActive() override;

private:
    uint32 last_cloud_ms;
    static constexpr uint32 CloudRotationDelayMs = 15000;
};

// Fires while Heigan is in the fast-dance phase (channeling Plague Cloud).
// Used by every bot — they all collapse onto the master and let the player
// pilot the dance for them.
class HeiganFastDanceTrigger : public Trigger
{
public:
    HeiganFastDanceTrigger(PlayerbotAI* ai) : Trigger(ai, "heigan fast dance") {}
    bool IsActive() override;
};

// Slow phase, main tank only. The tank explicitly parks on the platform so
// the boss settles there in melee. Melee DPS aren't pinned by this trigger
// — they chase the boss normally and end up on the platform with the tank,
// which avoids the ping-pong conflict between "stand at platform XY" and
// "chase the boss's actual position".
class HeiganSlowDancePlatformTrigger : public Trigger
{
public:
    HeiganSlowDancePlatformTrigger(PlayerbotAI* ai) : Trigger(ai, "heigan slow dance platform") {}
    bool IsActive() override;
};

// Slow phase, ranged bots only. They have to dance — no platform escape
// because they need cast range on the boss without bunching into melee.
class HeiganSlowDanceRangedTrigger : public Trigger
{
public:
    HeiganSlowDanceRangedTrigger(PlayerbotAI* ai) : Trigger(ai, "heigan slow dance ranged") {}
    bool IsActive() override;
};

class RazuviousTankTrigger : public Trigger
{
public:
    RazuviousTankTrigger(PlayerbotAI* ai) : Trigger(ai, "instructor razuvious tank"), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class RazuviousNontankTrigger : public Trigger
{
public:
    RazuviousNontankTrigger(PlayerbotAI* ai) : Trigger(ai, "instructor razuvious non-tank"), helper(ai) {}
    bool IsActive() override;

private:
    RazuviousBossHelper helper;
};

class KelthuzadTrigger : public Trigger
{
public:
    KelthuzadTrigger(PlayerbotAI* ai) : Trigger(ai, "kel'thuzad trigger"), helper(ai) {}
    bool IsActive() override;

private:
    KelthuzadBossHelper helper;
};

class AnubrekhanTrigger : public Trigger
{
public:
    AnubrekhanTrigger(PlayerbotAI* ai) : Trigger(ai, "anub'rekhan") {}
    bool IsActive() override;
};

 class FaerlinaTrigger : public Trigger
 {
 public:
     FaerlinaTrigger(PlayerbotAI* ai) : Trigger(ai, "faerlina") {}
     bool IsActive() override;
 };

class MaexxnaTrigger : public Trigger
{
public:
    MaexxnaTrigger(PlayerbotAI* ai) : Trigger(ai, "maexxna") {}
    bool IsActive() override;
};

// DPS only — fires while a Web Wrap NPC is alive in the room so DPS swap onto
// it to free the wrapped raid member. Tanks stay on the boss, healers keep the
// raid up.
class MaexxnaWebWrapTrigger : public Trigger
{
public:
    MaexxnaWebWrapTrigger(PlayerbotAI* ai) : Trigger(ai, "maexxna web wrap"), helper(ai) {}
    bool IsActive() override;

private:
    MaexxnaBossHelper helper;
};

// Sub-30% (Frenzy) only: the three triggers below fire in the short window just
// before each Web Spray (MaexxnaBossHelper::WebSprayImminent) so the protective
// cooldown is active when the raid-wide stun lands. Reactive healing can't save
// the tank during the stun, so everything must be pre-cast.

// Holy paladin → Hand of Sacrifice on the main tank.
class MaexxnaPreWebSprayHandOfSacrificeTrigger : public Trigger
{
public:
    MaexxnaPreWebSprayHandOfSacrificeTrigger(PlayerbotAI* ai)
        : Trigger(ai, "maexxna pre web spray hand of sacrifice"), helper(ai) {}
    bool IsActive() override;

private:
    MaexxnaBossHelper helper;
};

// Holy priest → Guardian Spirit on the main tank.
class MaexxnaPreWebSprayGuardianSpiritTrigger : public Trigger
{
public:
    MaexxnaPreWebSprayGuardianSpiritTrigger(PlayerbotAI* ai)
        : Trigger(ai, "maexxna pre web spray guardian spirit"), helper(ai) {}
    bool IsActive() override;

private:
    MaexxnaBossHelper helper;
};

// Main tank → pop its own big defensive (Shield Wall / Icebound Fortitude /
// Survival Instincts / Divine Protection) before the stun rather than reacting
// to the HP dip after it, when the tank is itself stunned and can't act.
class MaexxnaPreWebSprayTankDefensiveTrigger : public Trigger
{
public:
    MaexxnaPreWebSprayTankDefensiveTrigger(PlayerbotAI* ai)
        : Trigger(ai, "maexxna pre web spray tank defensive"), helper(ai) {}
    bool IsActive() override;

private:
    MaexxnaBossHelper helper;
};

//class PatchwerkTankTrigger : public Trigger
//{
//public:
//    PatchwerkTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk tank") {}
//    bool IsActive() override;
//};
//
//class PatchwerkNonTankTrigger : public Trigger
//{
//public:
//    PatchwerkNonTankTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk non-tank") {}
//    bool IsActive() override;
//};
//
//class PatchwerkRangedTrigger : public Trigger
//{
//public:
//    PatchwerkRangedTrigger(PlayerbotAI* ai) : Trigger(ai, "patchwerk ranged") {}
//    bool IsActive() override;
//};

class ThaddiusPhasePetTrigger : public Trigger
{
public:
    ThaddiusPhasePetTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase pet"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhasePetLoseAggroTrigger : public ThaddiusPhasePetTrigger
{
public:
    ThaddiusPhasePetLoseAggroTrigger(PlayerbotAI* ai) : ThaddiusPhasePetTrigger(ai) {}
    virtual bool IsActive()
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        return ThaddiusPhasePetTrigger::IsActive() && botAI->IsTank(bot) && target && target->GetVictim() != bot;
    }
};

class ThaddiusPhaseTransitionTrigger : public Trigger
{
public:
    ThaddiusPhaseTransitionTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase transition"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class ThaddiusPhaseThaddiusTrigger : public Trigger
{
public:
    ThaddiusPhaseThaddiusTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius phase thaddius"), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};

class FourHorsemenAttractorsTrigger : public Trigger
{
public:
    FourHorsemenAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen attractors"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

class FourHorsemenExceptAttractorsTrigger : public Trigger
{
public:
    FourHorsemenExceptAttractorsTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen except attractors"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Lady Blaumeux drops Void Zones (NPC 16697) on her current target every ~15s
// during the attract dance, and Sir Zeliek's Holy Wrath chains in roughly the
// same arc. The attractors stay at one preset spot for ~67s, so without this
// trigger they sit in a stack of voids until rotation. Applies to every bot in
// the encounter — tanks and DPS at the corners can also catch a stray drop.
class FourHorsemenVoidZoneTrigger : public Trigger
{
public:
    FourHorsemenVoidZoneTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen void zone"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Non-attractor healers only — attractors must stay within 45y of Lady/Sir or
// the boss casts its punishment AoE on the raid, so they eat the marks. Fires
// at 4 stacks and stays active until the aura fully decays (hysteresis is in
// FourHorsemenBossHelper::ShouldHealerBleedOffMark).
class FourHorsemenHealerHighMarkTrigger : public Trigger
{
public:
    FourHorsemenHealerHighMarkTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen healer high mark"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Fires for every bot during the opening burst (FourHorsemenBossHelper::
// IsOpeningWindow) so each pops its personal damage-reduction cooldown while
// the pull is at its roughest. Each bot only has its own class's action; the
// rest no-op. Wired below void-zone/bleed-off priority so survival movement
// still wins the tick.
class FourHorsemenOpeningDefensiveTrigger : public Trigger
{
public:
    FourHorsemenOpeningDefensiveTrigger(PlayerbotAI* ai)
        : Trigger(ai, "four horsemen opening defensive"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

// Back phase: both front melee bosses (Thane + Baron) are dead. Routes the
// surviving DPS and (now jobless) tanks onto the back casters via the
// back-phase action. Attractors are excluded (they keep soaking so Lady/Sir
// never punish the raid for an out-of-range victim); healers are excluded (they
// keep their existing park/bleed-off positioning).
class FourHorsemenBackPhaseTrigger : public Trigger
{
public:
    FourHorsemenBackPhaseTrigger(PlayerbotAI* ai) : Trigger(ai, "four horsemen back phase"), helper(ai) {}
    bool IsActive() override;

private:
    FourHorsemenBossHelper helper;
};

class SapphironGroundTrigger : public Trigger
{
public:
    SapphironGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron ground"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class SapphironFlightTrigger : public Trigger
{
public:
    SapphironFlightTrigger(PlayerbotAI* ai) : Trigger(ai, "sapphiron flight"), helper(ai) {}
    bool IsActive() override;

private:
    SapphironBossHelper helper;
};

class GluthTrigger : public Trigger
{
public:
    GluthTrigger(PlayerbotAI* ai) : Trigger(ai, "gluth trigger"), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class GluthMainTankMortalWoundTrigger : public Trigger
{
public:
    GluthMainTankMortalWoundTrigger(PlayerbotAI* ai) : Trigger(ai, "gluth main tank mortal wound trigger"), helper(ai) {}
    bool IsActive() override;

private:
    GluthBossHelper helper;
};

class LoathebTrigger : public Trigger
{
public:
    LoathebTrigger(PlayerbotAI* ai) : Trigger(ai, "loatheb"), helper(ai) {}
    bool IsActive() override;

private:
    LoathebBossHelper helper;
};

// Fires for the off-tank (assist tank index 0) while Noth is engaged. Detects
// the encounter via Noth himself (ground phase) or, when he's off the threat
// list, via his summoned adds (balcony phase).
class NothAddTankTrigger : public Trigger
{
public:
    NothAddTankTrigger(PlayerbotAI* ai) : Trigger(ai, "noth add tank") {}
    bool IsActive() override;
};

#endif
