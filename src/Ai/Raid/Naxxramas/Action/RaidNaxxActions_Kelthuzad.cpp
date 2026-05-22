#include "RaidNaxxActions.h"

#include "CharmInfo.h"
#include "CreatureAI.h"
#include "Pet.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

namespace
{
    void CollectBotPets(Player* bot, std::vector<Creature*>& out)
    {
        if (Pet* primary = bot->GetPet())
            out.push_back(primary);

        for (Unit* u : bot->m_Controlled)
        {
            Creature* c = u ? u->ToCreature() : nullptr;
            if (!c || c == bot->GetPet() || c->IsTotem())
                continue;
            out.push_back(c);
        }
    }
}

bool KelthuzadControlPetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    std::vector<Creature*> pets;
    CollectBotPets(bot, pets);
    if (pets.empty())
        return false;

    ReactStates desired = helper.IsPhaseOne() ? REACT_PASSIVE : REACT_AGGRESSIVE;

    for (Creature* pet : pets)
    {
        if (pet->GetReactState() != desired)
        {
            if (desired == REACT_PASSIVE && pet->GetVictim())
                pet->AttackStop();
            pet->SetReactState(desired);
            if (CharmInfo* ci = pet->GetCharmInfo())
                ci->SetPlayerReactState(desired);
        }
    }

    // P1: the bot picks a center-pack target via KelthuzadChooseTargetAction;
    // forward that to the pet so it stays useful while parked on passive.
    if (desired == REACT_PASSIVE)
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (target && target->IsAlive() && bot->IsValidAttackTarget(target))
        {
            for (Creature* pet : pets)
            {
                CharmInfo* ci = pet->GetCharmInfo();
                if (!ci)
                    continue;
                if (pet->GetVictim() == target && ci->IsCommandAttack())
                    continue;

                if (pet->GetVictim())
                    pet->AttackStop();

                ci->SetIsCommandAttack(true);
                ci->SetIsAtStay(false);
                ci->SetIsFollowing(false);
                ci->SetIsCommandFollow(false);
                ci->SetIsReturning(false);
                pet->ClearUnitState(UNIT_STATE_FOLLOW);

                if (pet->IsAIEnabled)
                    pet->AI()->AttackStart(target);
                else
                    pet->Attack(target, true);
            }
        }
    }

    // Return false so the engine chains to "kel'thuzad position" and
    // "kel'thuzad choose target" on the same tick — a true return would
    // consume the tick (Engine.cpp:218) and the bot would never move or
    // pick a victim.
    return false;
}

bool KelthuzadChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit *target_soldier = nullptr, *target_weaver = nullptr, *target_abomination = nullptr, *target_kelthuzad = nullptr,
         *target_guardian = nullptr;
    for (auto i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "guardian of icecrown"))
        {
            if (!target_guardian)
                target_guardian = unit;
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()))
            {
                target_guardian = unit;
            }
            else if (unit->GetVictim() && target_guardian->GetVictim() && unit->GetVictim()->ToPlayer() &&
                     target_guardian->GetVictim()->ToPlayer() && !botAI->IsAssistTank(unit->GetVictim()->ToPlayer()) &&
                     !botAI->IsAssistTank(target_guardian->GetVictim()->ToPlayer()) &&
                     target_guardian->GetDistance2d(helper.center.first, helper.center.second) >
                         bot->GetDistance2d(unit))
            {
                target_guardian = unit;
            }
        }

        if (unit->GetDistance2d(helper.center.first, helper.center.second) > 30.0f)
            continue;

        if (bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance)
            continue;

        if (botAI->EqualLowercaseName(unit->GetName(), "unstoppable abomination"))
        {
            if (target_abomination == nullptr ||
                target_abomination->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_abomination = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soldier of the frozen wastes"))
        {
            if (target_soldier == nullptr ||
                target_soldier->GetDistance2d(helper.center.first, helper.center.second) >
                    unit->GetDistance2d(helper.center.first, helper.center.second))
            {
                target_soldier = unit;
            }
        }
        if (botAI->EqualLowercaseName(unit->GetName(), "soul weaver"))
        {
            if (target_weaver == nullptr || target_weaver->GetDistance2d(helper.center.first, helper.center.second) >
                                                unit->GetDistance2d(helper.center.first, helper.center.second))
                target_weaver = unit;
        }

        if (botAI->EqualLowercaseName(unit->GetName(), "kel'thuzad"))
            target_kelthuzad = unit;
    }
    std::vector<Unit*> targets;
    if (botAI->IsRanged(bot))
    {
        if (botAI->GetRangedDpsIndex(bot) <= 1)
            targets = {target_soldier, target_weaver, target_abomination, target_kelthuzad};
        else
            targets = {target_weaver, target_soldier, target_abomination, target_kelthuzad};
    }
    else if (botAI->IsAssistTank(bot))
        targets = {target_abomination, target_guardian, target_kelthuzad};
    else
        targets = {target_abomination, target_kelthuzad};

    for (Unit* t : targets)
    {
        if (t)
        {
            target = t;
            break;
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    if (target_kelthuzad && target == target_kelthuzad)
        return Attack(target, true);

    return Attack(target, false);
}

bool KelthuzadPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.IsPhaseOne())
    {
        if (AI_VALUE(Unit*, "current target") == nullptr)
            return MoveInside(NAXX_MAP_ID, helper.center.first, helper.center.second, bot->GetPositionZ(), 3.0f,
                              MovementPriority::MOVEMENT_COMBAT);
    }
    else if (helper.IsPhaseTwo())
    {
        Unit* shadow_fissure = helper.GetAnyShadowFissure();
        if (!shadow_fissure || !bot->IsWithinDistInMap(shadow_fissure, 10.0f))
        {
            float distance, angle;
            if (botAI->IsMainTank(bot))
            {
                if (AI_VALUE2(bool, "has aggro", "current target"))
                    return MoveTo(NAXX_MAP_ID, helper.tank_pos.first, helper.tank_pos.second, bot->GetPositionZ(), false, false, false,
                                  false, MovementPriority::MOVEMENT_COMBAT);
                else
                    return false;
            }
            else if (botAI->IsRanged(bot))
            {
                // Two concentric rings, pushed out from the old 20/32y. Frost
                // Blast is a 10y flood-fill (see the melee block below), so a
                // frozen melee cluster (~11.5y from KT) would chain straight into
                // a ranged sitting at 20y — only 8.5y away. The inner ring now
                // clears the three melee clusters by ~13y (>10y hop), and the
                // outer ring is offset half a slot (22.5 deg) so an inner and
                // outer caster never line up radially within the hop. Both rings
                // stay inside heal/cast range.
                uint32 index = botAI->GetRangedIndex(bot);
                if (index < 8)
                {
                    distance = 26.0f;
                    angle = index * M_PI / 4;
                }
                else
                {
                    distance = 34.0f;
                    angle = (index - 8) * M_PI / 4 + M_PI / 8;
                }
                float dx, dy;
                dx = helper.center.first + cos(angle) * distance;
                dy = helper.center.second + sin(angle) * distance;
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            else if (botAI->IsTank(bot))
            {
                Unit* cur_tar = AI_VALUE(Unit*, "current target");
                if (cur_tar && cur_tar->GetVictim() && cur_tar->GetVictim()->ToPlayer() &&
                    botAI->EqualLowercaseName(cur_tar->GetName(), "guardian of icecrown") &&
                    botAI->IsAssistTank(cur_tar->GetVictim()->ToPlayer()))
                {
                    return MoveTo(NAXX_MAP_ID, helper.assist_tank_pos.first, helper.assist_tank_pos.second, bot->GetPositionZ(),
                                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
                }
                else
                    return false;
            }
            // P2 melee Frost Blast spread (4-point). Frost Blast (27808) freezes a
            // random player, then re-casts itself from each new victim every 1s — a
            // 10y flood-fill that chains through anyone within 10y of an already-
            // frozen player (see SpellAuraEffects.cpp case 27808). It freezes the
            // whole *connected blob*, so the defence is to keep melee in a FEW tight
            // clusters that are each >10y from every other group — other clusters,
            // the tank, AND the ranged ring (which is pushed out above for exactly
            // this reason) — so a frozen cluster has nobody to bridge the chain onto.
            // With the tank holding KT at the front, the three clusters sit at KT's
            // left, directly behind, and KT's right (the "4-point" layout).
            //
            // Clusters are anchored to the *fixed* tank spot, not KT's live facing,
            // so they don't spin every time he turns to Frostbolt someone. Radius =
            // boss+bot combat reach (a hair inside max melee range, ~11.5y given
            // KT's 10y reach), which puts the 90-degree-separated clusters ~16y
            // apart — well past the 10y Frost Blast. To fall back to the wider-
            // margin 3-point (tank + two rear clusters), swap to the 3-cluster
            // angle table noted below. Healers keep their own positioning; this
            // only herds melee DPS.
            else if (!botAI->IsHeal(bot))
            {
                Unit* boss = AI_VALUE2(Unit*, "find target", "kel'thuzad");
                if (!boss)
                    return false;

                // Offsets from the tank-facing "front". 4-point: KT's left,
                // behind, KT's right. 3-point alt (wider margin, two rear
                // clusters): {(float)(3*M_PI/4), (float)(-3*M_PI/4)}, count = 2.
                static const float kMeleeAngles4[] = {(float)(M_PI / 2.0), (float)M_PI, (float)(-M_PI / 2.0)};
                const float* angles = kMeleeAngles4;
                const int clusterCount = 3;

                int meleeIdx = botAI->GetMeleeIndex(bot);
                if (meleeIdx < 0)
                    meleeIdx = 0;
                int cluster = meleeIdx % clusterCount;
                int rank = meleeIdx / clusterCount;  // position within the cluster

                float frontAngle = boss->GetAngle(helper.tank_pos.first, helper.tank_pos.second);
                float clusterAngle = frontAngle + angles[cluster];
                float radius = boss->GetCombatReach() + bot->GetCombatReach();
                float cx = boss->GetPositionX() + cos(clusterAngle) * radius;
                float cy = boss->GetPositionY() + sin(clusterAngle) * radius;

                // Fan cluster-mates a couple yards along the tangent so they pack
                // tightly instead of all shoving onto one coordinate (which
                // jitters). The cluster still fits inside one Frost Blast footprint,
                // well clear of its neighbours.
                float tangent = clusterAngle + (float)(M_PI / 2.0);
                int sign = (rank % 2 == 0) ? 1 : -1;
                float lateral = sign * ((rank + 1) / 2) * 2.0f;
                float dx = cx + cos(tangent) * lateral;
                float dy = cy + sin(tangent) * lateral;
                return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false,
                              MovementPriority::MOVEMENT_COMBAT);
            }
        }
        else
        {
            float dx, dy;
            float angle;
            if (!botAI->IsRanged(bot))
                angle = shadow_fissure->GetAngle(helper.center.first, helper.center.second);
            else
                angle = bot->GetAngle(shadow_fissure) + M_PI;

            dx = shadow_fissure->GetPositionX() + cos(angle) * 10.0f;
            dy = shadow_fissure->GetPositionY() + sin(angle) * 10.0f;
            return MoveTo(NAXX_MAP_ID, dx, dy, bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}
