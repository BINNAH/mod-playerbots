#ifndef _PLAYERBOT_RAIDNAXXACTIONS_H
#define _PLAYERBOT_RAIDNAXXACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidNaxxBossHelper.h"

class GrobbulusGoBehindAction : public MovementAction
{
public:
    GrobbulusGoBehindAction(PlayerbotAI* ai, float distance = 24.0f, float delta_angle = M_PI / 8)
        : MovementAction(ai, "grobbulus go behind")
    {
        this->distance = distance;
        this->delta_angle = delta_angle;
    }
    virtual bool Execute(Event event);

protected:
    float distance, delta_angle;
};

class GrobbulusRotateAction : public RotateAroundTheCenterPointAction
{
public:
    GrobbulusRotateAction(PlayerbotAI* botAI)
        : RotateAroundTheCenterPointAction(botAI, "rotate grobbulus", 3281.23f, -3310.38f, 35.0f, 8, true, M_PI) {}
    virtual bool isUseful() override
    {
        return RotateAroundTheCenterPointAction::isUseful() && botAI->IsMainTank(bot) &&
               AI_VALUE2(bool, "has aggro", "boss target");
    }
    uint32 GetCurrWaypoint() override;
};

class GrobbulusMoveCenterAction : public MoveInsideAction
{
public:
    GrobbulusMoveCenterAction(PlayerbotAI* ai) : MoveInsideAction(ai, 3281.23f, -3310.38f, 5.0f) {}
};

class GrobbulusMoveAwayAction : public MovementAction
{
public:
    GrobbulusMoveAwayAction(PlayerbotAI* ai, float distance = 18.0f)
        : MovementAction(ai, "grobbulus move away"), distance(distance)
    {
    }
    bool Execute(Event event) override;

private:
    float distance;
};

// Heigan the Unclean.
//
// Slow-dance phase: tank + melee stand on the SE platform — eruption
// GameObjects are only placed on the dance floor proper, so the platform
// dodges the AOE entirely. Ranged dance to the safe section for the next
// eruption.
//
// Fast-dance phase: everyone dances. Same algorithm as slow with tighter
// constants (first eruption at +7s, every 4s).
//
// The schedule from boss_heigan.cpp is fully deterministic; each bot
// predicts it from the start of the current phase. Section index follows
// the boss script's `_currentSection` value (3 = nearest the spawn corner,
// 0 = far SW strip). HeiganFollowMasterAction is kept as a manual-piloting
// fallback but isn't wired into the strategy by default.
class HeiganFollowMasterAction : public MovementAction
{
public:
    HeiganFollowMasterAction(PlayerbotAI* ai) : MovementAction(ai, "heigan follow master") {}
    bool Execute(Event event) override;
};

class HeiganPlatformAction : public MovementAction
{
public:
    HeiganPlatformAction(PlayerbotAI* ai)
        : MovementAction(ai, "heigan platform"), slow_phase_start_ms(0), last_seen_ms(0) {}
    bool Execute(Event event) override;

private:
    // Slow-phase clock, mirroring HeiganDanceAction. Lets melee DPS predict the
    // +90s teleport and leave the platform a few seconds early to join the dance
    // (see RaidNaxxActions_Heigan.cpp). Re-anchored on the fast->slow resume.
    uint32 slow_phase_start_ms;
    uint32 last_seen_ms;
};

class HeiganDanceAction : public MovementAction
{
public:
    HeiganDanceAction(PlayerbotAI* ai)
        : MovementAction(ai, "heigan dance"), phase_start_ms(0), last_seen_ms(0),
          fast_phase(false) {}
    bool Execute(Event event) override;

private:
    uint8 ComputeSafeSection(uint32 now) const;

    // Phase clock. phase_start_ms is snapped to "now" on each phase
    // transition (fresh pull, observed slow↔fast flip, or long gap since
    // last invocation), giving each phase its own +15s/+7s lead-in. The
    // long-gap branch is load-bearing for tank/melee, whose slow-phase
    // trigger doesn't route here — without it they resume fast phase 2
    // with stale state from fast phase 1 and run to the wrong corner.
    uint32 phase_start_ms;
    uint32 last_seen_ms;
    bool fast_phase;
};

class ThaddiusAttackNearestPetAction : public AttackAction
{
public:
    ThaddiusAttackNearestPetAction(PlayerbotAI* ai) : AttackAction(ai, "thaddius attack nearest pet"), helper(ai) {}
    virtual bool Execute(Event event);
    virtual bool isUseful();

private:
    ThaddiusBossHelper helper;
};

// class ThaddiusMeleeToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusMeleeToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius melee to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

// class ThaddiusRangedToPlaceAction : public MovementAction
// {
// public:
//     ThaddiusRangedToPlaceAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius ranged to place") {}
//     virtual bool Execute(Event event);
//     virtual bool isUseful();
// };

class ThaddiusMoveToPlatformAction : public MovementAction
{
public:
    ThaddiusMoveToPlatformAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move to platform") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class ThaddiusMovePolarityAction : public MovementAction
{
public:
    ThaddiusMovePolarityAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius move polarity") {}
    virtual bool Execute(Event event);
    virtual bool isUseful();
};

class RazuviousUseObedienceCrystalAction : public MovementAction
{
public:
    RazuviousUseObedienceCrystalAction(PlayerbotAI* ai)
        : MovementAction(ai, "razuvious use obedience crystal"), helper(ai)
    {
    }
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class RazuviousTargetAction : public AttackAction
{
public:
    RazuviousTargetAction(PlayerbotAI* ai) : AttackAction(ai, "razuvious target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    RazuviousBossHelper helper;
};

class FourHorsemenAttractAlternativelyAction : public AttackAction
{
public:
    FourHorsemenAttractAlternativelyAction(PlayerbotAI* ai) : AttackAction(ai, "four horsemen attract alternatively"), helper(ai)
    {
    }
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

class FourHorsemenAttackInOrderAction : public AttackAction
{
public:
    FourHorsemenAttackInOrderAction(PlayerbotAI* ai) : AttackAction(ai, "four horsemen attack in order"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

class FourHorsemenAvoidVoidZoneAction : public MovementAction
{
public:
    FourHorsemenAvoidVoidZoneAction(PlayerbotAI* ai)
        : MovementAction(ai, "four horsemen avoid void zone"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

class FourHorsemenHealerBleedOffMarkAction : public MovementAction
{
public:
    FourHorsemenHealerBleedOffMarkAction(PlayerbotAI* ai)
        : MovementAction(ai, "four horsemen healer bleed off mark"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    FourHorsemenBossHelper helper;
};

// class SapphironGroundMainTankPositionAction : public MovementAction
// {
// public:
//     SapphironGroundMainTankPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron ground main tank
//     position") {} virtual bool Execute(Event event);
// };

class SapphironGroundPositionAction : public MovementAction
{
public:
    SapphironGroundPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron ground position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    SapphironBossHelper helper;
};

class SapphironFlightPositionAction : public MovementAction
{
public:
    SapphironFlightPositionAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron flight position"), helper(ai) {}
    bool Execute(Event event) override;

protected:
    SapphironBossHelper helper;
};

// class SapphironAvoidChillAction : public MovementAction
// {
// public:
//     SapphironAvoidChillAction(PlayerbotAI* ai) : MovementAction(ai, "sapphiron avoid chill") {}
//     virtual bool Execute(Event event);
// };

class KelthuzadChooseTargetAction : public AttackAction
{
public:
    KelthuzadChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "kel'thuzad choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

class KelthuzadPositionAction : public MovementAction
{
public:
    KelthuzadPositionAction(PlayerbotAI* ai) : MovementAction(ai, "kel'thuzad position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

// P1 has the raid AoEing waves of adds in the central pit while KT himself is
// flagged non-attackable in his alcove. Pets on REACT_AGGRESSIVE leash out to
// soldiers still in the side rooms and pull entire alcoves into the raid.
// Park pets on passive during P1 and sic them on the bot's current target so
// they still contribute DPS. P2 (KT attackable) restores aggressive.
class KelthuzadControlPetAction : public Action
{
public:
    KelthuzadControlPetAction(PlayerbotAI* ai) : Action(ai, "kel'thuzad control pet"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    KelthuzadBossHelper helper;
};

// DPS focus-target switch onto the Web Wrap NPC that spawns when a non-tank
// gets webbed (entry 16486). Sits above the default attacker selection so the
// bot leaves the boss until the wrapped player is freed.
class MaexxnaAttackWebWrapAction : public AttackAction
{
public:
    MaexxnaAttackWebWrapAction(PlayerbotAI* ai)
        : AttackAction(ai, "maexxna attack web wrap"), helper(ai) {}
    bool Execute(Event event) override;

private:
    MaexxnaBossHelper helper;
};

// Holy paladin external for the sub-30% Web Spray: Hand of Sacrifice (6940)
// redirects 30% of the tank's damage to the paladin for 12s. Mirrors
// BuffOnMainTankAction so the cast lands on the "main tank" value; isUseful /
// isPossible already gate on knowing the spell, it being off cooldown, and the
// tank not already carrying the buff. The pre-Web-Spray trigger handles timing.
class MaexxnaHandOfSacrificeOnMainTankAction : public BuffOnMainTankAction
{
public:
    MaexxnaHandOfSacrificeOnMainTankAction(PlayerbotAI* ai)
        : BuffOnMainTankAction(ai, "hand of sacrifice") {}
};

// Holy priest external for the sub-30% Web Spray: Guardian Spirit (47788)
// prevents the next lethal hit and boosts healing taken by 40% for 10s — the
// best anti-stun tool since it carries through the window when no heal can
// land. The generic "guardian spirit on party" action targets the lowest-HP
// party member; this variant forces the main tank instead (same retarget trick
// as BuffOnMainTankAction).
class MaexxnaGuardianSpiritOnMainTankAction : public HealPartyMemberAction
{
public:
    MaexxnaGuardianSpiritOnMainTankAction(PlayerbotAI* ai)
        : HealPartyMemberAction(ai, "guardian spirit", 40.0f, HealingManaEfficiency::MEDIUM) {}

    Value<Unit*>* GetTargetValue() override
    {
        return context->GetValue<Unit*>("main tank", "guardian spirit");
    }
    std::string const getName() override { return "guardian spirit on main tank"; }
};

class AnubrekhanChooseTargetAction : public AttackAction
{
public:
    AnubrekhanChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "anub'rekhan choose target") {}
    bool Execute(Event event) override;
};

class AnubrekhanPositionAction : public RotateAroundTheCenterPointAction
{
public:
    AnubrekhanPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "anub'rekhan position", 3272.49f, -3476.27f, 45.0f, 16) {}
    bool Execute(Event event) override;
};

class GluthChooseTargetAction : public AttackAction
{
public:
    GluthChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "gluth choose target"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthPositionAction : public RotateAroundTheCenterPointAction
{
public:
    GluthPositionAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "gluth position", 3293.61f, -3149.01f, 12.0f, 12), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class GluthSlowdownAction : public Action
{
public:
    GluthSlowdownAction(PlayerbotAI* ai) : Action(ai, "gluth slowdown"), helper(ai) {}
    bool Execute(Event event) override;

private:
    GluthBossHelper helper;
};

class LoathebPositionAction : public MovementAction
{
public:
    LoathebPositionAction(PlayerbotAI* ai) : MovementAction(ai, "loatheb position"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

class LoathebChooseTargetAction : public AttackAction
{
public:
    LoathebChooseTargetAction(PlayerbotAI* ai) : AttackAction(ai, "loatheb choose target"), helper(ai) {}
    virtual bool Execute(Event event);

private:
    LoathebBossHelper helper;
};

//class PatchwerkRangedPositionAction : public MovementAction
//{
//public:
//    PatchwerkRangedPositionAction(PlayerbotAI* ai) : MovementAction(ai, "patchwerk ranged position") {}
//    bool Execute(Event event) override;
//};

// Noth the Plaguebringer.
//
// Off-tank (assist tank index 0) duty: the boss summons waves of Plagued
// Warriors/Champions/Guardians scattered around the room. This action sweeps
// up any add that isn't already on a tank (taunting it off squishies) and
// drags the whole pack onto the main tank's position — i.e. onto Noth — so
// melee cleave and the boss-target AoE chew them down together. Falls back to
// Noth's own position (then his fixed ground spot) when no main tank is found,
// which covers the balcony phase where Noth is off the threat list.
class NothAddTankAction : public AttackAction
{
public:
    NothAddTankAction(PlayerbotAI* ai) : AttackAction(ai, "noth tank adds") {}
    bool Execute(Event event) override;
};

#endif
