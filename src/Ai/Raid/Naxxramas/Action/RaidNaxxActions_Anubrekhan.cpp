#include "ObjectGuid.h"
#include "Playerbots.h"
#include "RaidNaxxActions.h"

bool AnubrekhanChooseTargetAction::Execute(Event /*event*/)
{
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    Unit* target = nullptr;
    Unit* target_boss = nullptr;
    std::vector<Unit*> target_guards;
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;
        if (botAI->EqualLowercaseName(unit->GetName(), "crypt guard"))
            target_guards.push_back(unit);

        if (botAI->EqualLowercaseName(unit->GetName(), "anub'rekhan"))
            target_boss = unit;
    }
    if (botAI->IsMainTank(bot))
        target = target_boss;
    else
    {
        if (target_guards.size() == 0)
            target = target_boss;
        else
        {
            if (botAI->IsAssistTank(bot))
            {
                for (Unit* t : target_guards)
                {
                    if (target == nullptr || (target->GetVictim() && target->GetVictim()->ToPlayer() &&
                                              botAI->IsTank(target->GetVictim()->ToPlayer())))
                        target = t;
                }
            }
            else
            {
                for (Unit* t : target_guards)
                {
                    if (target == nullptr || target->GetHealthPct() > t->GetHealthPct())
                        target = t;
                }
            }
        }
    }
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    return Attack(target);
}

bool AnubrekhanPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'rekhan");
    if (!boss)
        return false;

    bool inPhase = botAI->HasAura("locust swarm", boss) || boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (inPhase)
    {
        if (botAI->IsMainTank(bot))
        {
            uint32 nearest = FindNearestWaypoint();
            uint32 next_point;
            next_point = (nearest + 1) % intervals;

            return MoveTo(bot->GetMapId(), waypoints[next_point].first, waypoints[next_point].second,
                          bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        else
            return MoveInside(533, 3272.49f, -3476.27f, bot->GetPositionZ(), 3.0f, MovementPriority::MOVEMENT_COMBAT);
    }

    // Outside Locust Swarm, Impale targets a random non-tank and erupts spikes
    // at their position — stacked ranged/healers chain-eat the same volley.
    if (botAI->IsRanged(bot) || botAI->IsHeal(bot))
    {
        constexpr float spreadRadius = 10.0f;
        GuidVector members = AI_VALUE(GuidVector, "group members");
        for (ObjectGuid const& guid : members)
        {
            Unit* member = botAI->GetUnit(guid);
            if (!member || !member->IsAlive() || member == bot)
                continue;
            Player* memberPlayer = member->ToPlayer();
            if (!memberPlayer)
                continue;
            if (!botAI->IsRanged(memberPlayer) && !botAI->IsHeal(memberPlayer))
                continue;
            if (bot->GetExactDist2d(member) < spreadRadius)
                return FleePosition(member->GetPosition(), spreadRadius);
        }
    }
    return false;
}
