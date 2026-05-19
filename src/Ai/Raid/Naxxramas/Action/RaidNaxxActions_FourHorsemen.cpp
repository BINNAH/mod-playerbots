#include "RaidNaxxActions.h"

#include "Playerbots.h"

bool FourHorsemenAttractAlternativelyAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    helper.CalculatePosToGo(bot);
    auto [posX, posY] = helper.CurrentAttractPos();
    if (MoveTo(bot->GetMapId(), posX, posY, helper.posZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        return true;

    Unit* attackTarget = helper.CurrentAttackTarget();
    if (attackTarget && context->GetValue<Unit*>("current target")->Get() != attackTarget)
        return Attack(attackTarget);

    return false;
}

bool FourHorsemenAttackInOrderAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    Unit* target = nullptr;
    Unit* thane = AI_VALUE2(Unit*, "find target", "thane korth'azz");
    Unit* lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
    Unit* sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
    Unit* fourth = AI_VALUE2(Unit*, "find target", "baron rivendare");
    if (!fourth)
        fourth = AI_VALUE2(Unit*, "find target", "highlord mograine");

    std::vector<Unit*> attack_order;
    if (botAI->IsAssistTank(bot))
        attack_order = {fourth, thane, lady, sir};
    else
        attack_order = {thane, fourth, lady, sir};
    for (Unit* t : attack_order)
    {
        if (t && t->IsAlive())
        {
            target = t;
            break;
        }
    }
    if (!target)
        return false;

    // Split-corner strategy: tanks anchor each melee boss in his own corner
    // (~60y apart, outside both 45y Mark auras); melee + ranged DPS converge
    // on the main tank to focus Thane; non-attractor healers split between
    // mid and the assist tank's corner.
    if (target == thane || target == fourth)
    {
        const std::pair<float, float>* pos = &helper.tankPosThane;
        if (botAI->IsAssistTank(bot))
            pos = &helper.tankPosBaron;
        else if (PlayerbotAI::IsHeal(bot))
            // Healer index 0 attracts and never reaches here. Healer index 2
            // (only present in 3-healer setups) dedicates to the assist tank;
            // any other non-attractor healer (typically index 1) covers both
            // tanks from the middle.
            pos = botAI->IsAssistHealOfIndex(bot, 2) ? &helper.tankPosBaron
                                                    : &helper.healerMidPos;

        if (bot->GetDistance2d(pos->first, pos->second) > 4.0f &&
            MoveTo(bot->GetMapId(), pos->first, pos->second, helper.posZ,
                   false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            return true;
    }

    if (context->GetValue<Unit*>("current target")->Get() == target && botAI->GetState() == BOT_STATE_COMBAT)
        return false;

    if (!bot->IsWithinLOSInMap(target))
        return MoveNear(target, 22.0f, MovementPriority::MOVEMENT_COMBAT);

    return Attack(target);
}
