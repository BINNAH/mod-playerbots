#include "JsonStrategyActions.h"

#include "JsonStrategyShapeUtil.h"
#include "Playerbots.h"

#include <cmath>
#include <cstdio>
#include <limits>

// ---------------------------------------------------------------------------
// orbit_point
// ---------------------------------------------------------------------------
void JsonOrbitPointAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _valid = false;

    float x = 0.0f, y = 0.0f, r = 40.0f;
    unsigned int segments = 16;
    int cw = 1;
    int parsed = std::sscanf(qual.c_str(), "%f,%f,%f,%u,%d", &x, &y, &r, &segments, &cw);
    if (parsed < 4 || segments == 0)
        return;

    center_x = x;
    center_y = y;
    radius = r;
    intervals = segments;
    clockwise = (cw != 0);

    waypoints.clear();
    for (uint32 i = 0; i < intervals; ++i)
    {
        float angle = 2.0f * (float)M_PI * i / intervals;
        waypoints.push_back(std::make_pair(center_x + std::cos(angle) * radius,
                                           center_y + std::sin(angle) * radius));
    }
    call_counters = 0;
    _valid = true;
}

uint32 JsonOrbitPointAction::GetCurrWaypoint()
{
    uint32 nearest = FindNearestWaypoint();
    uint32 step = clockwise ? 1u : (intervals - 1u);
    return (nearest + step) % intervals;
}

// ---------------------------------------------------------------------------
// stack_point
// ---------------------------------------------------------------------------
void JsonStackPointAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _valid = false;

    float px = 0.0f, py = 0.0f, r = 5.0f;
    int parsed = std::sscanf(qual.c_str(), "%f,%f,%f", &px, &py, &r);
    if (parsed < 2)
        return;

    x = px;
    y = py;
    distance = (parsed >= 3 && r > 0.0f) ? r : 5.0f;
    _valid = true;
}

// ---------------------------------------------------------------------------
// spread
// ---------------------------------------------------------------------------
void JsonSpreadAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _radius = 0.0f;
    _minInterval = 3000;
    _valid = false;

    float r = 0.0f;
    unsigned int interval = 3000;
    int parsed = std::sscanf(qual.c_str(), "%f,%u", &r, &interval);
    if (parsed >= 1 && r > 0.0f)
    {
        _radius = r;
        if (parsed >= 2)
            _minInterval = interval;
        _valid = true;
    }
}

bool JsonSpreadAction::Execute(Event /*event*/)
{
    if (!_valid)
        return false;

    GuidVector members = AI_VALUE(GuidVector, "group members");
    Unit* closest = nullptr;
    float best = _radius;
    for (ObjectGuid const& guid : members)
    {
        Unit* member = botAI->GetUnit(guid);
        if (!member || !member->IsAlive() || member == bot)
            continue;
        // Only spread from other ranged/healers — the melee/tank stack stays put.
        Player* memberPlayer = member->ToPlayer();
        if (!memberPlayer || (!botAI->IsRanged(memberPlayer) && !botAI->IsHeal(memberPlayer)))
            continue;
        float dist = bot->GetExactDist2d(member);
        if (dist < best)
        {
            best = dist;
            closest = member;
        }
    }

    if (closest)
        return FleePosition(closest->GetPosition(), _radius, _minInterval);
    return false;
}

// ---------------------------------------------------------------------------
// attack_target
// ---------------------------------------------------------------------------
bool JsonAttackTargetAction::Execute(Event /*event*/)
{
    if (_target.empty())
        return false;

    Unit* target = AI_VALUE2(Unit*, "find target", _target);
    if (!target)
        return false;

    // Already on it — let the normal rotation / lower-priority actions run.
    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    return Attack(target);
}

// ---------------------------------------------------------------------------
// attack_priority
// ---------------------------------------------------------------------------
void JsonAttackPriorityAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _addsCsv = JsonKv(qual, "adds");
    _boss = JsonKv(qual, "boss");
}

bool JsonAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* target = nullptr;

    if (!_addsCsv.empty())
    {
        std::vector<std::string> addNames = JsonSplit(_addsCsv, ',');
        GuidVector attackers = AI_VALUE(GuidVector, "attackers");
        Unit* lowest = nullptr;
        for (ObjectGuid const& guid : attackers)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;
            bool isAdd = false;
            for (std::string const& name : addNames)
            {
                if (botAI->EqualLowercaseName(unit->GetName(), name))
                {
                    isAdd = true;
                    break;
                }
            }
            if (!isAdd)
                continue;
            if (!lowest || unit->GetHealthPct() < lowest->GetHealthPct())
                lowest = unit;
        }
        target = lowest;
    }

    // No prioritized adds up — fall back to the boss.
    if (!target && !_boss.empty())
        target = AI_VALUE2(Unit*, "find target", _boss);

    if (!target)
        return false;

    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;

    return Attack(target);
}

// ---------------------------------------------------------------------------
// tank_adds
// ---------------------------------------------------------------------------
void JsonTankAddsAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _addName = JsonKv(qual, "add");
    _bossName = JsonKv(qual, "boss");
}

bool JsonTankAddsAction::Execute(Event /*event*/)
{
    if (_addName.empty())
        return false;

    // Only the first off-tank shepherds adds; others stay on their jobs.
    if (!botAI->IsAssistTankOfIndex(bot, 0))
        return false;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    std::vector<Unit*> adds;
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && botAI->EqualLowercaseName(unit->GetName(), _addName))
            adds.push_back(unit);
    }
    if (adds.empty())
        return false;

    // Rally point: where the main tank holds the boss (so adds get cleaved on
    // the boss). Fall back to the boss's own position.
    bool haveRally = false;
    float rallyX = 0.0f, rallyY = 0.0f, rallyZ = 0.0f;
    Unit* mt = AI_VALUE(Unit*, "main tank");
    if (mt && mt->IsAlive() && mt != bot)
    {
        rallyX = mt->GetPositionX();
        rallyY = mt->GetPositionY();
        rallyZ = mt->GetPositionZ();
        haveRally = true;
    }
    else if (!_bossName.empty())
    {
        if (Unit* boss = AI_VALUE2(Unit*, "find target", _bossName))
        {
            rallyX = boss->GetPositionX();
            rallyY = boss->GetPositionY();
            rallyZ = boss->GetPositionZ();
            haveRally = true;
        }
    }

    // Prefer the nearest add not already held by a tank; else keep beating the
    // nearest so we hold threat while moving.
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

    if (bot->GetVictim() != target)
        return Attack(target);

    if (target->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", Event(), true);

    if (haveRally && bot->GetExactDist2d(rallyX, rallyY) > 5.0f)
        return MoveTo(bot->GetMapId(), rallyX, rallyY, rallyZ, false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);

    return false;
}
