#include "RaidNaxxActions.h"

#include "Playerbots.h"

bool FourHorsemenAttractAlternativelyAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    helper.CalculatePosToGo(bot);
    auto [rawX, rawY] = helper.CurrentAttractPos();
    // Detour around void zones at the preset spot — biased toward the
    // opposite attractor so the bot stays in heal range. Without this, the
    // avoid action steps the bot out and this MoveTo yanks it straight back
    // into the puddle next tick.
    auto [posX, posY] = helper.ResolveSafeAttractPos(rawX, rawY);
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
    // (~60y apart, outside both 45y Mark auras); melee + non-attractor ranged
    // DPS converge on the main tank to focus Thane; healers park via
    // HealerParkPos (back healers rotate beside one back caster so they eat just
    // one Mark, front healer holds mid, and an extra healer joins the back once
    // the first horseman dies). Once Thane dies, target rolls to Baron and the
    // front group migrates to his corner — otherwise DPS get yanked back to
    // Thane's corpse every tick (distance > 4y from tankPosThane triggers the
    // MoveTo before Attack can pull them toward Baron).
    if (target == thane || target == fourth)
    {
        float destX, destY;
        if (PlayerbotAI::IsHeal(bot))
        {
            // With 2 ranged attractors no healer attracts, so every healer
            // reaches here. Back healers get their current rotating side spot;
            // the front healer holds healerMidPos.
            auto park = helper.HealerParkPos(bot);
            destX = park.first;
            destY = park.second;
            // Detour the back healer's side spot around a void zone sitting on
            // it — otherwise the avoid action steps out and this MoveTo yanks it
            // straight back into the puddle next tick (the 5-6y back-and-forth).
            // The void-avoid action below resolves to the same point so they
            // agree.
            if (helper.IsBackHealer(bot))
            {
                auto safe = helper.ResolveSafeHealerPos(destX, destY);
                destX = safe.first;
                destY = safe.second;
            }
        }
        else
        {
            const std::pair<float, float>* pos = &helper.tankPosThane;
            if (target == thane)
            {
                if (botAI->IsAssistTank(bot))
                    pos = &helper.tankPosBaron;
            }
            else  // target == fourth (Baron / Mograine) — Thane is dead
                pos = &helper.tankPosBaron;
            destX = pos->first;
            destY = pos->second;
        }

        if (bot->GetDistance2d(destX, destY) > 4.0f &&
            MoveTo(bot->GetMapId(), destX, destY, helper.posZ,
                   false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            return true;
    }

    // Parked healers don't chase the boss — yield so heal/instant actions run
    // from the spot. Otherwise the Attack(target) below pulls them toward the
    // boss and next tick's MoveTo drags them back: the forward-back dance.
    if (PlayerbotAI::IsHeal(bot))
        return false;

    if (context->GetValue<Unit*>("current target")->Get() == target && botAI->GetState() == BOT_STATE_COMBAT)
        return false;

    if (!bot->IsWithinLOSInMap(target))
        return MoveNear(target, 22.0f, MovementPriority::MOVEMENT_COMBAT);

    return Attack(target);
}

bool FourHorsemenAvoidVoidZoneAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    Unit* vz = helper.GetVoidZoneStandingIn(bot);
    if (!vz)
        return false;

    // Attractors flee toward the opposite attract spot — same bias the
    // attract action uses, so the immediate side-step and the follow-up
    // MoveTo agree on a destination. Non-attractors (front DPS who walked
    // through a stray void) just radiate away via the generic FleePosition.
    if (helper.IsAttracter(bot))
    {
        helper.CalculatePosToGo(bot);
        auto [destX, destY] = helper.ResolveSafeAttractPos(bot->GetPositionX(), bot->GetPositionY());
        return MoveTo(bot->GetMapId(), destX, destY, helper.posZ,
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // Back healers step out toward the same point their park MoveTo resolves to,
    // so the two actions don't fight over the contaminated spot. Front healer /
    // stray DPS just radiate away via the generic FleePosition.
    if (helper.IsBackHealer(bot))
    {
        auto park = helper.HealerParkPos(bot);
        auto safe = helper.ResolveSafeHealerPos(park.first, park.second);
        return MoveTo(bot->GetMapId(), safe.first, safe.second, helper.posZ,
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    return FleePosition(vz->GetPosition(), helper.VOID_ZONE_RADIUS + 1.0f);
}

bool FourHorsemenHealerBleedOffMarkAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    auto [x, y] = helper.markBleedoffPos;
    if (bot->GetDistance2d(x, y) <= 4.0f)
        return false;  // Already parked — let combat actions (heals, etc.) run.

    return MoveTo(bot->GetMapId(), x, y, helper.posZ,
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}
