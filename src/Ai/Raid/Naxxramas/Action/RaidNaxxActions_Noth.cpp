#include <limits>

#include "ObjectGuid.h"
#include "Playerbots.h"
#include "RaidNaxxActions.h"

bool NothAddTankAction::Execute(Event /*event*/)
{
    // Only the off-tank shepherds the adds; the main tank stays glued to Noth.
    if (!botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    // Collect every living Plagued add in combat with the group.
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    std::vector<Unit*> adds;
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && NothAddEntries::IsNothAdd(unit->GetEntry()))
            adds.push_back(unit);
    }
    if (adds.empty())
        return false;

    // Rally point: where the main tank is holding Noth, so the adds get
    // dragged onto the boss for the raid to cleave. Fall back to Noth himself,
    // then his fixed ground spot (balcony phase, when no MT is on the boss).
    float rallyX = 2684.94f, rallyY = -3502.53f, rallyZ = 261.31f;
    Unit* mt = AI_VALUE(Unit*, "main tank");
    if (mt && mt->IsAlive() && mt != bot)
    {
        rallyX = mt->GetPositionX();
        rallyY = mt->GetPositionY();
        rallyZ = mt->GetPositionZ();
    }
    else if (Unit* noth = AI_VALUE2(Unit*, "find target", "noth the plaguebringer"))
    {
        rallyX = noth->GetPositionX();
        rallyY = noth->GetPositionY();
        rallyZ = noth->GetPositionZ();
    }

    // Prefer grabbing the nearest add that isn't already on a tank (loose on a
    // healer/dps, or on nothing). Otherwise keep beating the nearest add so we
    // hold threat while moving.
    Unit* loose = nullptr;
    Unit* nearest = nullptr;
    float looseDist = std::numeric_limits<float>::max();
    float nearestDist = std::numeric_limits<float>::max();
    for (Unit* add : adds)
    {
        float dist = bot->GetExactDist2d(add);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = add;
        }

        Unit* victim = add->GetVictim();
        bool heldByTank = victim && victim->ToPlayer() && botAI->IsTank(victim->ToPlayer());
        if (!heldByTank && dist < looseDist)
        {
            looseDist = dist;
            loose = add;
        }
    }

    Unit* target = loose ? loose : nearest;
    if (!target)
        return false;

    // Face/attack the add, then taunt it off whoever it's chewing on. Mirrors
    // the established off-tank idiom (attack -> taunt -> reposition).
    if (bot->GetVictim() != target)
        return Attack(target);

    if (target->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", Event(), true);

    // Held — now drag the pack onto the rally point so they stack on Noth.
    if (bot->GetExactDist2d(rallyX, rallyY) > 5.0f)
        return MoveTo(NAXX_MAP_ID, rallyX, rallyY, rallyZ, false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);

    return false;
}
