#include "RaidTocActions.h"

#include <cmath>
#include <limits>

#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RaidTocBossHelper.h"

namespace
{
// Mirror of the spread-slot math in RaidTocTriggers.cpp (kept file-local in both
// TUs so we don't disturb shared headers / the build glob).
bool ComputeBeastSpreadPos(PlayerbotAI* botAI, Player* bot, float radius, float& outX, float& outY)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid myGuid = bot->GetGUID();
    uint32 total = 0;
    uint32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || !m->IsAlive())
            continue;
        if (!GET_PLAYERBOT_AI(m))  // skip the human; bots own the spread ring
            continue;
        if (botAI->IsTank(m))
            continue;
        if (!botAI->IsRanged(m) && !botAI->IsHeal(m))
            continue;
        ++total;
        if (m->GetGUID() < myGuid)
            ++index;
    }
    if (total == 0)
        return false;

    float angle = index * (TOC_TWO_PI / total);
    outX = TOC_CENTER_X + std::cos(angle) * radius;
    outY = TOC_CENTER_Y + std::sin(angle) * radius;
    return true;
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

bool GormokSpreadAction::Execute(Event /*event*/)
{
    float x, y;
    if (!ComputeBeastSpreadPos(botAI, bot, TOC_SPREAD_RADIUS, x, y))
        return false;
    return MoveTo(bot->GetMapId(), x, y, TOC_CENTER_Z, false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true);
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

    return FleePosition(pool->GetPosition(), TOC_SLIME_POOL_FLEE_DISTANCE);
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
