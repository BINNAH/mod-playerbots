#include "RaidNaxxMultipliers.h"

#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "DruidCatActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PriestActions.h"
#include "RaidNaxxActions.h"
#include "RaidNaxxSpellIds.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ScriptedCreature.h"
#include "ShamanActions.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UseMeetingStoneAction.h"
#include "WarlockActions.h"
#include "WarriorActions.h"

float GrobbulusMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "grobbulus");
    if (!boss)
        return 1.0f;

    if (dynamic_cast<AvoidAoeAction*>(action))
        return botAI->IsMainTank(bot) ? 0.0f : 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}

float HeiganDanceMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "heigan the unclean");
    if (!boss)
        return 1.0f;

    // Suppress only the MOVEMENT competitors that would drag the bot off its
    // dance wedge: combat-formation follow (re-targets the group toward the
    // master every tick), the gap-closer/retreat casts (Disengage, Blink), and
    // the generic avoid-AOE / flee behaviour (the eruption is AOE, so it keeps
    // picking its own escape spot). The dance/platform actions now hold the
    // tick while they're still en route (see RaidNaxxActions_Heigan.cpp), so
    // they win movement outright — but they return false once in the wedge,
    // and we deliberately DON'T suppress dps-assist / heal target-selection
    // here so bots resume attacking and healing from the safe spot between
    // eruptions.
    if (dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<CastDisengageAction*>(action) ||
        dynamic_cast<CastBlinkBackAction*>(action) ||
        dynamic_cast<FleeAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action))
        return 0.0f;

    // Fast-dance only: keep healers instant. The +4s eruption cadence demands
    // the bot stay free to relocate every tick, but starting a cast parks the
    // whole AI for its full cast time (PlayerbotAI sets nextCheckDelay =
    // castTime + reactDelay), so a 2-3.5s heal leaves the healer rooted right
    // through an eruption — and even when it doesn't, the dance's InterruptSpell
    // cancels it mid-cast for the next hop, burning the GCD and mana for no
    // heal. Suppress any heal with a (haste-adjusted) cast time or a channel;
    // instants still fire from the safe wedge between eruptions: Renew, PW:S,
    // PoM, Circle of Healing, Holy Shock, Rejuv, Wild Growth, Swiftmend,
    // Riptide, Earth Shield. The slow phase is untouched (cast freely there).
    if (HeiganIsFastDancing(botAI, boss))
    {
        if (CastHealingSpellAction* heal = dynamic_cast<CastHealingSpellAction*>(action))
        {
            uint32 spellId = AI_VALUE2(uint32, "spell id", heal->getSpell());
            if (spellId)
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
                if (info && (info->CalcCastTime(bot) > 0 || info->IsChanneled()))
                    return 0.0f;
            }
        }
    }

    return 1.0f;
}

float LoathebGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loatheb");
    if (!boss)
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action)))
    {
        return 0.0f;
    }
    if (!dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::NecroticAura10});
    if (!aura)
    {
        // Fallback to name for custom spell data.
        aura = botAI->GetAura("necrotic aura", bot);
    }
    if (!aura || aura->GetDuration() <= 1500)
        return 1.0f;

    return 0.0f;
}

float ThaddiusGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;
    // pet phase
    if (helper.IsPhasePet() &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
         dynamic_cast<ReachPartyMemberToHealAction*>(action) || dynamic_cast<BuffOnMainTankAction*>(action)))
    {
        return 0.0f;
    }
    // die at the same time
    Unit* target = AI_VALUE(Unit*, "current target");
    Unit* feugen = AI_VALUE2(Unit*, "find target", "feugen");
    Unit* stalagg = AI_VALUE2(Unit*, "find target", "stalagg");
    if (helper.IsPhasePet() && target && feugen && stalagg && target->GetHealthPct() <= 40 &&
        (feugen->GetHealthPct() >= target->GetHealthPct() + 3 || stalagg->GetHealthPct() >= target->GetHealthPct() + 3))
    {
        if (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastHealingSpellAction*>(action))
            return 0.0f;
    }
    // magnetic pull
    // uint32 curr_timer = eventMap->GetTimer();
    // // if (curr_phase == 2 && bot->GetPositionZ() > 312.5f && dynamic_cast<MovementAction*>(action))
    // {
    // if (curr_phase == 2 && (curr_timer % 20000 >= 18000 || curr_timer % 20000 <= 2000) &&
    // dynamic_cast<MovementAction*>(action))
    // {
    //     // MotionMaster *mm = bot->GetMotionMaster();
    //     // mm->Clear();
    //     return 0.0f;
    // }
    // thaddius phase
    // if (curr_phase == 8 && dynamic_cast<FleeAction*>(action))
    // {
    //         return 0.0f;
    // }
    return 1.0f;
}

float SapphironGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if (dynamic_cast<CastDeathGripAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    // Flight phase: bots must sit still behind their iceblock until the
    // frost-breath explosion resolves. Any of these would yank them off
    // the block — assist actions chase the (untargetable) flying boss,
    // flee/debuff pick fresh destinations every tick — and the resulting
    // tug-of-war with the iceblock positioning action shows up as the
    // back-and-forth dance behind the block.
    if (helper.IsPhaseFlight() &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<FleeAction*>(action) || dynamic_cast<CastDebuffSpellOnAttackerAction*>(action)))
    {
        return 0.0f;
    }

    return 1.0f;
}

float InstructorRazuviousGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    if (botAI->GetState() == BOT_STATE_COMBAT &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }
    return 1.0f;
}

float KelthuzadGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action)))
    {
        return 0.0f;
    }
    if (helper.IsPhaseOne())
    {
        if (dynamic_cast<CastTotemAction*>(action) || dynamic_cast<CastShadowfiendAction*>(action) ||
            dynamic_cast<CastRaiseDeadAction*>(action) || dynamic_cast<CastFeignDeathAction*>(action) ||
            dynamic_cast<CastInvisibilityAction*>(action) || dynamic_cast<CastVanishAction*>(action) ||
            dynamic_cast<PetAttackAction*>(action))
        {
            return 0.0f;
        }
    }
    if (helper.IsPhaseTwo())
    {
        if (dynamic_cast<CastBlizzardAction*>(action) || dynamic_cast<CastFrostNovaAction*>(action))
            return 0.0f;

    }
    return 1.0f;
}

float AnubrekhanGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return 1.0f;

    if (NaxxSpellIds::HasAnyAura(
            botAI, boss, {NaxxSpellIds::LocustSwarm10, NaxxSpellIds::LocustSwarm10Alt, NaxxSpellIds::LocustSwarm25}) ||
        botAI->HasAura("locust swarm", boss))
    {
        if (dynamic_cast<FleeAction*>(action))
            return 0.0f;
    }
    return 1.0f;
}

float FourHorsemenGenericMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sir zeliek");
    if (!boss)
        return 1.0f;

    context->GetValue<bool>("neglect threat")->Set(true);
    // Every position on this fight is scripted (tank corners, attract spots,
    // healerBackPos, healerMidPos, mark bleed-off). Let the generic movers
    // compete and you get the classic forward-back dance. Confirmed offenders
    // (caught via the per-tick action log):
    //   - Follow: pushed as a relevance-1 default, so on any tick a parked
    //     healer has nothing urgent to heal, it wins by default and walks the
    //     bot toward the master; the 4HM park MoveTo drags it back next tick.
    //     This is the actual 5-6y bounce — the parked-healer yield opened the
    //     idle gap that lets follow run.
    //   - CombatFormationMove / Flee: yank a parked bot toward the group.
    //   - ReachPartyMemberToHeal: a back healer's heal list includes the front
    //     tanks/melee/master ~60-70y away (outside its ~40y heal range), so it
    //     runs toward the front to reach them, then the 4HM MoveTo drags it back
    //     to healerBackPos next tick. Healing is split front/back by design, so
    //     a healer must never chase an out-of-range target across the room: it
    //     heals its in-range group from its spot and the other healer covers
    //     the rest.
    // Void-zone dodging is unaffected — it's a dedicated MovementAction, not a
    // FleeAction. (CombatFormationMove/Flee suppression mirrors Heigan/Kelthuzad/
    // Gluth; the reach-to-heal suppression mirrors Thaddius' pet phase.)
    if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FleeAction*>(action) ||
        dynamic_cast<ReachPartyMemberToHealAction*>(action) || dynamic_cast<FollowAction*>(action))
        return 0.0f;

    // NOTE: we deliberately do NOT force the back healers instant-only here.
    // They park at healerBackPos and stay put (the dance is gone now that the
    // generic movers above are suppressed), so they need their full cast-time
    // kit (Greater Heal, Flash Heal, Healing Wave, Prayer of Healing, Nourish)
    // for real throughput — an instant-only lock left them barely healing. When
    // they DO have to dodge a void zone or bleed off marks, the move action wins
    // the tick and CanCastSpell already rejects cast-time heals while moving, so
    // mobility is handled without crippling output. (Tradeoff: a healer that
    // commits to a long cast can get caught a beat late by a void zone spawning
    // under it, since the cast parks the AI; that's an acceptable, rare hit
    // versus near-zero healing.)

    return 1.0f;
}

// float GothikGenericMultiplier::GetValue(Action* action)
// {
//     Unit* boss = AI_VALUE2(Unit*, "find target", "gothik the harvester");
//     if (!boss)
//     {
//         return 1.0f;
//     }
//     BossAI* boss_ai = dynamic_cast<BossAI*>(boss->GetAI());
//     EventMap* eventMap = boss_botAI->GetEvents();
//     uint32 curr_phase = eventMap->GetPhaseMask();
//     if (curr_phase == 1 && (dynamic_cast<FollowAction*>(action)))
//     {
//         return 0.0f;
//     }
//     if (curr_phase == 1 && (dynamic_cast<AttackAction*>(action)))
//     {
//         Unit* target = action->GetTarget();
//         if (target == boss)
//         {
//             return 0.0f;
//         }
//     }
//     return 1.0f;
// }

float GluthGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    if ((dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<FleeAction*>(action) || dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) ||
         dynamic_cast<CastStarfallAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action)))
    {
        return 0.0f;
    }

    if (botAI->IsMainTank(bot))
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::MortalWound10, NaxxSpellIds::MortalWound25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("mortal wound", bot, false, true);
        }
        if (aura && aura->GetStackAmount() >= 5)
        {
            if (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action))
            {
                return 0.0f;
            }
        }
    }
    if (dynamic_cast<PetAttackAction*>(action))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (helper.IsZombieChow(target))
            return 0.0f;
    }
    return 1.0f;
}

// Major offensive cooldowns we bank for the sub-30% Frenzy. Starfall is left
// out on purpose — it's the boomkin's AoE for the spiderling waves, so holding
// it would hurt add control. Hysteria is a raid utility buff on someone else,
// not self-burst, so it's left to fire normally.
static bool IsDpsBurstCooldown(Action* action)
{
    return dynamic_cast<CastBloodlustAction*>(action) ||
           dynamic_cast<CastHeroismAction*>(action) ||
           dynamic_cast<CastDeathWishAction*>(action) ||
           dynamic_cast<CastRecklessnessAction*>(action) ||
           dynamic_cast<CastIcyVeinsAction*>(action) ||
           dynamic_cast<CastCombustionAction*>(action) ||
           dynamic_cast<CastArcanePowerAction*>(action) ||
           dynamic_cast<CastAdrenalineRushAction*>(action) ||
           dynamic_cast<CastBladeFlurryAction*>(action) ||
           dynamic_cast<CastKillingSpreeAction*>(action) ||
           dynamic_cast<CastColdBloodAction*>(action) ||
           dynamic_cast<CastBerserkAction*>(action) ||  // druid cat
           dynamic_cast<CastBestialWrathAction*>(action) ||
           dynamic_cast<CastRapidFireAction*>(action) ||
           dynamic_cast<CastMetamorphosisAction*>(action) ||
           dynamic_cast<CastSummonGargoyleAction*>(action) ||
           dynamic_cast<CastDancingRuneWeaponAction*>(action);
}

// Big tank mitigation cooldowns. Divine Shield is excluded (it sheds threat),
// as are the rotational/reactive ones (Shield Block, Frenzied Regeneration).
static bool IsTankDefensiveCooldown(Action* action)
{
    return dynamic_cast<CastShieldWallAction*>(action) ||
           dynamic_cast<CastLastStandAction*>(action) ||
           dynamic_cast<CastSurvivalInstinctsAction*>(action) ||
           dynamic_cast<CastBarkskinAction*>(action) ||
           dynamic_cast<CastIceboundFortitudeAction*>(action) ||
           dynamic_cast<CastVampiricBloodAction*>(action) ||
           dynamic_cast<CastDivineProtectionAction*>(action);
}

float MaexxnaGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    // (1) Web Wrap target lock. The web-wrap action (ACTION_RAID + 2) does the
    // initial swap onto the wrap; without this, the default DpsAssist re-picks
    // the boss every tick and the bot ping-pongs between the two, never finishing
    // the wrap. Pin DPS on the wrap until it dies, then DpsAssist resumes.
    if (dynamic_cast<DpsAssistAction*>(action) && botAI->IsDps(bot) && helper.GetClosestWebWrap())
        return 0.0f;

    // (2) Cooldown banking. Hold burst until she frenzies; then let it all fly.
    if (!helper.IsFrenzied())
    {
        if (IsDpsBurstCooldown(action))
            return 0.0f;

        // Tanks bank their defensives for the Frenzy too, but only while healthy
        // — if a tank dips below the floor before 30%, release them so it never
        // dies holding an unused cooldown.
        if (botAI->IsTank(bot) && bot->GetHealthPct() > TANK_DEFENSIVE_HP_FLOOR &&
            IsTankDefensiveCooldown(action))
            return 0.0f;
    }

    return 1.0f;
}
