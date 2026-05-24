#include "RaidTocActions.h"

#include <cmath>
#include <limits>

#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RaidTocBossHelper.h"

namespace
{
// Mirror of NearestSpreaderWithin in RaidTocTriggers.cpp (kept file-local in
// both TUs so we don't disturb shared headers / the build glob).
Player* NearestSpreaderWithin(PlayerbotAI* botAI, Player* bot, float dist)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* best = nullptr;
    float bestDist = dist;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || m == bot || !m->IsAlive())
            continue;
        if (!GET_PLAYERBOT_AI(m))  // ignore the human; they position themselves
            continue;
        if (botAI->IsTank(m))
            continue;
        if (!botAI->IsRanged(m) && !botAI->IsHeal(m))
            continue;
        float d = bot->GetExactDist2d(m);
        if (d < bestDist)
        {
            bestDist = d;
            best = m;
        }
    }
    return best;
}

// The worm (Acidmaw/Dreadscale) `bot` is currently tanking, else null. Mirror in
// RaidTocTriggers.cpp (kept file-local so we don't disturb shared headers).
Unit* WormTankedBy(PlayerbotAI* botAI, Player* bot)
{
    for (uint32 entry : { NPC_DREADSCALE, NPC_ACIDMAW })
    {
        Unit* worm = GetFirstAliveUnitByEntry(botAI, entry);
        if (worm && worm->GetVictim() == bot)
            return worm;
    }
    return nullptr;
}

// Centroid (2D) of the backline to protect: living group members that aren't this
// tank and aren't stacked on the worm (those are committed to its front anyway).
// Falls back to the arena centre when nobody qualifies.
void RaidBacklineCentroid(PlayerbotAI* botAI, Player* bot, Unit* worm, float& cx, float& cy)
{
    float sx = 0.0f, sy = 0.0f;
    int n = 0;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || m == bot || !m->IsAlive())
                continue;
            if (m->GetExactDist2d(worm) < TOC_WORM_MELEE_RANGE)
                continue;  // melee on the worm don't define the backline
            sx += m->GetPositionX();
            sy += m->GetPositionY();
            ++n;
        }
    }
    if (n == 0)
    {
        cx = TOC_CENTER_X;
        cy = TOC_CENTER_Y;
        return;
    }
    cx = sx / n;
    cy = sy / n;
}
}  // namespace

bool GormokMoveAwayFireBombAction::Execute(Event /*event*/)
{
    Creature* bomb = bot->FindNearestCreature(NPC_FIRE_BOMB, TOC_FIRE_BOMB_AVOID_RADIUS);
    if (!bomb)
        return false;

    return FleePosition(bomb->GetPosition(), TOC_FIRE_BOMB_FLEE_DISTANCE);
}

bool GormokAttackSnoboldAction::Execute(Event /*event*/)
{
    Creature* snobold = bot->FindNearestCreature(NPC_SNOBOLD_VASSAL, TOC_SNOBOLD_SEARCH_RADIUS);
    if (!snobold || !snobold->IsAlive())
        return false;

    return Attack(snobold);
}

bool GormokTankSwapTauntAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    // Already holding him — nothing to do.
    if (boss->GetVictim() == bot)
        return false;

    // Make sure Gormok is our target before firing the taunt spell.
    if (bot->GetVictim() != boss)
        return Attack(boss);

    return botAI->DoSpecificAction("taunt spell", Event(), true);
}

bool GormokOffTankBackoffAction::Execute(Event /*event*/)
{
    // Halt auto-attack and consume the tick (suppressing the threat rotation) so
    // the active tank keeps a threat lead and the boss doesn't snap back to us.
    bot->AttackStop();
    return true;
}

bool GormokTankOffSnoboldAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER);
    if (!boss)
        return false;
    // Re-target Gormok so we stop chasing/attacking the snobold.
    return Attack(boss);
}

bool GormokMeleeFocusBossAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER);
    if (!boss)
        return false;

    // Already on Gormok — let the normal melee rotation keep swinging.
    if (bot->GetTarget() == boss->GetGUID())
        return false;

    return Attack(boss);
}

bool GormokSpreadAction::Execute(Event /*event*/)
{
    Player* clumpedWith = NearestSpreaderWithin(botAI, bot, TOC_SPREAD_MIN_DIST);
    if (!clumpedWith)
        return false;
    // Step away from whoever we're crowding; once >MIN_DIST the trigger stops.
    return FleePosition(clumpedWith->GetPosition(), TOC_SPREAD_PUSH_DIST);
}

bool GormokSnobbledRunInAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER);
    if (!boss)
        return false;

    // Already in the melee stack — don't shove further in.
    if (bot->GetExactDist2d(boss) <= TOC_SNOBBLED_REACH)
        return false;

    return MoveTo(bot->GetMapId(), boss->GetPositionX(), boss->GetPositionY(), boss->GetPositionZ(),
                  false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool WormsFocusDreadscaleAction::Execute(Event /*event*/)
{
    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, NPC_DREADSCALE);
    if (!dreadscale)
        return false;

    // Already on Dreadscale — let the normal rotation keep nuking it.
    if (bot->GetTarget() == dreadscale->GetGUID())
        return false;

    return Attack(dreadscale);
}

bool WormsRunToBurningBileAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Player* carrier = nullptr;
    float best = std::numeric_limits<float>::max();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;
        if (!botAI->HasAura("burning bile", member))
            continue;

        float d = bot->GetExactDist2d(member);
        if (d < best)
        {
            best = d;
            carrier = member;
        }
    }

    if (!carrier)
        return false;

    // Close enough — the bile will burn the toxin off; stop crowding.
    if (bot->GetExactDist2d(carrier) <= TOC_BILE_CURE_REACH)
        return false;

    return MoveTo(bot->GetMapId(), carrier->GetPositionX(), carrier->GetPositionY(),
                  carrier->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool WormsAvoidSlimePoolAction::Execute(Event /*event*/)
{
    Creature* pool = bot->FindNearestCreature(NPC_SLIME_POOL, TOC_SLIME_POOL_AVOID_RADIUS);
    if (!pool)
        return false;

    // Tanks kite the mobile worm in a smooth arc around the arena centre instead
    // of fleeing each pool's nearest edge (which zig-zags = "pacing"). Stepping
    // along a consistent tangent drags the worm so its pools trail behind us.
    if (botAI->IsTank(bot))
    {
        float rx = bot->GetPositionX() - TOC_CENTER_X;
        float ry = bot->GetPositionY() - TOC_CENTER_Y;
        float len = std::sqrt(rx * rx + ry * ry);
        if (len < 1.0f) { rx = 1.0f; ry = 0.0f; len = 1.0f; }
        rx /= len;
        ry /= len;
        // Counter-clockwise tangent, one short step ahead.
        float tx = bot->GetPositionX() + (-ry) * TOC_SLIME_POOL_FLEE_DISTANCE;
        float ty = bot->GetPositionY() + (rx) * TOC_SLIME_POOL_FLEE_DISTANCE;
        return MoveTo(bot->GetMapId(), tx, ty, bot->GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_COMBAT, true);
    }

    return FleePosition(pool->GetPosition(), TOC_SLIME_POOL_FLEE_DISTANCE);
}

bool WormsTankFaceAwayAction::Execute(Event /*event*/)
{
    Unit* worm = WormTankedBy(botAI, bot);
    if (!worm)
        return false;

    float cx = TOC_CENTER_X, cy = TOC_CENTER_Y;
    RaidBacklineCentroid(botAI, bot, worm, cx, cy);

    // Step to the far side of the worm from the raid. The worm faces its tank
    // (us), so standing opposite the raid points the spew cone away from them.
    float dx = worm->GetPositionX() - cx;
    float dy = worm->GetPositionY() - cy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    dx /= len;
    dy /= len;

    float dist = worm->GetCombatReach() + TOC_WORM_FACE_MELEE_GAP;
    float tx = worm->GetPositionX() + dx * dist;
    float ty = worm->GetPositionY() + dy * dist;

    return MoveTo(bot->GetMapId(), tx, ty, worm->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool IcehowlDodgeChargeAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_ICEHOWL);
    if (!boss)
        return false;

    // The charge always runs straight through the arena centre, so the lane is
    // the line through Icehowl and the centre. Step perpendicular off it.
    float bx = boss->GetPositionX();
    float by = boss->GetPositionY();
    float dx = TOC_CENTER_X - bx;
    float dy = TOC_CENTER_Y - by;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f)
    {
        dx = std::cos(boss->GetOrientation());
        dy = std::sin(boss->GetOrientation());
        len = 1.0f;
    }
    dx /= len;
    dy /= len;

    // Perpendicular to the lane.
    float px = -dy;
    float py = dx;

    // Signed distance of the bot from the lane (measured from Icehowl).
    float rx = bot->GetPositionX() - bx;
    float ry = bot->GetPositionY() - by;
    float side = rx * px + ry * py;

    if (std::fabs(side) >= TOC_ICEHOWL_CHARGE_CLEAR)
        return false; // already clear of the lane

    float sign = (side >= 0.0f) ? 1.0f : -1.0f;
    float need = (TOC_ICEHOWL_CHARGE_CLEAR - std::fabs(side)) + 2.0f;
    float tx = bot->GetPositionX() + px * sign * need;
    float ty = bot->GetPositionY() + py * sign * need;

    return MoveTo(bot->GetMapId(), tx, ty, bot->GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool JaraxxusStealNetherPowerAction::Execute(Event /*event*/)
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_JARAXXUS);
    if (!boss)
        return false;

    // Only worth a GCD if a stack is actually up (the trigger already checks, but
    // a tick can pass between the two — re-check so we never waste the cast).
    if (!botAI->HasAura("nether power", boss))
        return false;

    return botAI->CastSpell("spellsteal", boss);
}

bool JaraxxusAvoidLegionFlameAction::Execute(Event /*event*/)
{
    Creature* flame = bot->FindNearestCreature(NPC_LEGION_FLAME, TOC_LEGION_FLAME_AVOID_RADIUS);
    if (!flame)
        return false;

    return FleePosition(flame->GetPosition(), TOC_LEGION_FLAME_FLEE_DISTANCE);
}

bool GormokSelfBopClearImpaleAction::Execute(Event /*event*/)
{
    // Hand of Protection grants physical immunity, which strips the Impale bleed.
    return botAI->CastSpell("hand of protection", bot);
}

bool GormokRemoveSelfBopAction::Execute(Event /*event*/)
{
    Aura* bop = botAI->GetAura("hand of protection", bot);
    if (!bop)
        return false;
    // Drop the immunity immediately so we keep tanking; the stacks are already gone.
    bot->RemoveAura(bop->GetId());
    return true;
}

bool WormsAvoidBurningBileAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Player* carrier = nullptr;
    float best = TOC_BILE_AVOID_RADIUS;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || m == bot || !m->IsAlive())
            continue;
        if (!botAI->HasAura("burning bile", m))
            continue;

        float d = bot->GetExactDist2d(m);
        if (d < best)
        {
            best = d;
            carrier = m;
        }
    }

    if (!carrier)
        return false;

    return FleePosition(carrier->GetPosition(), TOC_BILE_AVOID_FLEE);
}
