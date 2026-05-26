#include "JsonStrategyActions.h"

#include "JsonStrategyShapeUtil.h"
#include "Playerbots.h"
#include "Timer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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
// attack  (merged former attack_target + attack_priority + attack_nearest)
// ---------------------------------------------------------------------------

// Match `unit` against `token`: an all-digit token compares the creature entry
// id, anything else the (case-insensitive) creature name.
static bool MatchesNameOrEntry(PlayerbotAI* botAI, Unit* unit, std::string const& token)
{
    if (token.empty())
        return false;
    if (token.find_first_not_of("0123456789") == std::string::npos)
        return unit->GetEntry() == (uint32)std::strtoul(token.c_str(), nullptr, 10);
    return botAI->EqualLowercaseName(unit->GetName(), token);
}

void JsonAttackAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _targetsCsv = JsonKv(qual, "targets");
    _boss = JsonKv(qual, "boss");
    _nearestDetect = JsonKv(qual, "detect") == "nearest";
    _nearestSelect = JsonKv(qual, "select") == "nearest";
}

bool JsonAttackAction::Execute(Event /*event*/)
{
    Unit* target = nullptr;

    if (!_targetsCsv.empty())
    {
        std::vector<std::string> tokens = JsonSplit(_targetsCsv, ',');
        Unit* current = context->GetValue<Unit*>("current target")->Get();

        // Detection source: the threat list, or a nearby-NPC scan that also sees
        // OFF-THREAT objects (cocoons, freshly-spawned adds no one has aggro on).
        GuidVector candidates = _nearestDetect ? AI_VALUE(GuidVector, "nearest npcs")
                                               : AI_VALUE(GuidVector, "attackers");

        Unit* best = nullptr;
        float bestMetric = 0.0f;
        bool currentMatches = false;
        for (ObjectGuid const& guid : candidates)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;

            bool match = false;
            for (std::string const& token : tokens)
                if (MatchesNameOrEntry(botAI, unit, token))
                {
                    match = true;
                    break;
                }
            if (!match)
                continue;

            if (unit == current)
                currentMatches = true;

            // Lower wins for both metrics: nearest distance, or lowest health %.
            float metric = _nearestSelect ? bot->GetExactDist2d(unit) : unit->GetHealthPct();
            if (!best || metric < bestMetric)
            {
                best = unit;
                bestMetric = metric;
            }
        }

        // Stick to the match we're already on instead of chasing whichever is
        // momentarily best. With two near-equal candidates (e.g. Anub's pair of
        // Crypt Guards) the global "best" flips tick-to-tick and every flip
        // cancels a caster's in-flight nuke — so casters thrash without finishing.
        // Only fall back to the freshly-picked best when our current target is no
        // longer a live match (dead/gone), i.e. when we need the next target.
        target = currentMatches ? current : best;
    }

    // None of the priority targets up — fall back to the boss so DPS don't idle.
    if (!target && !_boss.empty())
        target = AI_VALUE2(Unit*, "find target", _boss);

    if (!target)
        return false;

    // Already on it — yield so lower-priority actions (rotation) run.
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

// ---------------------------------------------------------------------------
// timed_safe_zone  (generic form of HeiganDanceAction)
// ---------------------------------------------------------------------------
void JsonTimedSafeZoneAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _zones.clear();
    _pattern.clear();
    _valid = false;
    // A re-qualify (reload with edited params) is a fresh action — drop the clock.
    _phaseStartMs = 0;
    _lastSeenMs = 0;

    for (std::string const& tok : JsonSplit(JsonKv(qual, "zones"), ','))
        _zones.push_back((float)std::atof(tok.c_str()));
    for (std::string const& tok : JsonSplit(JsonKv(qual, "pattern"), ','))
        _pattern.push_back((uint8)std::atoi(tok.c_str()));

    _z = (float)std::atof(JsonKv(qual, "z", "0").c_str());
    _firstAt = (uint32)std::strtoul(JsonKv(qual, "first", "0").c_str(), nullptr, 10);
    _interval = (uint32)std::strtoul(JsonKv(qual, "interval", "0").c_str(), nullptr, 10);
    _hold = JsonKv(qual, "hold") == "1";
    _castWhileMoving = JsonKv(qual, "cw") == "1";
    std::string tol = JsonKv(qual, "tol", "5.0");
    _tol = (float)std::atof(tol.c_str());

    // Need an even, non-empty zone list, a pattern, a positive interval, and every
    // pattern index must resolve to a zone we actually have.
    if (_zones.size() >= 2 && (_zones.size() % 2 == 0) && !_pattern.empty() && _interval > 0)
    {
        _valid = true;
        for (uint8 idx : _pattern)
            if ((size_t)idx * 2 + 1 >= _zones.size())
            {
                _valid = false;
                break;
            }
    }
}

bool JsonTimedSafeZoneAction::Execute(Event /*event*/)
{
    if (!_valid)
        return false;

    uint32 now = getMSTime();

    // Per-bot phase clock. The rule's trigger only lets this run during its phase,
    // so the action sits dormant for the whole *other* phase. Anchor on first run
    // and re-anchor after a long idle gap (the dormancy between same-phase windows)
    // — that gap IS the phase boundary. The threshold must stay well above a single
    // cast/channel: a casting bot parks its AI (nextCheckDelay = castTime) so this
    // isn't re-entered for the cast's duration; at a low threshold every nuke would
    // look like a phase change and reset the clock every cast. 30s clears the
    // longest channel yet sits under any same-phase dormancy. (Mirrors the proven
    // HeiganDanceAction threshold reasoning.)
    constexpr uint32 kStaleGapMs = 30000;
    bool fresh = (_phaseStartMs == 0);
    bool longGap = (_lastSeenMs != 0) && (now - _lastSeenMs > kStaleGapMs);
    if (fresh || longGap)
        _phaseStartMs = now;
    _lastSeenMs = now;

    // Which eruption are we on, and therefore which zone is safe. Before the first
    // eruption the safe zone is pattern[0]; after it the index walks the pattern.
    uint32 elapsed = now - _phaseStartMs;
    uint32 k = (elapsed < _firstAt) ? 0u : (elapsed - _firstAt) / _interval + 1u;
    uint8 idx = _pattern[k % _pattern.size()];
    float x = _zones[idx * 2];
    float y = _zones[idx * 2 + 1];

    // Already on the safe zone.
    if (bot->IsWithinDist2d(x, y, _tol))
    {
        // Tight cadence (hold=1): own the tick rather than hand back — a started
        // cast parks the AI past the next eruption, so the bot would eat a wave
        // mid-cast. Interrupt anything carried in and hold. Costs nothing when the
        // boss is unattackable for the phase anyway.
        if (_hold)
        {
            botAI->InterruptSpell();
            return true;
        }
        // Loose cadence (hold=0): yield so DPS / heal rotations run between
        // eruptions — there's ample time to cast and still relocate.
        return false;
    }

    // En route to the safe zone.
    if (_castWhileMoving)
    {
        // Yield the tick so the bot's own rotation fires INSTANTS while we
        // relocate. PlayerbotAI::CanCastSpell refuses cast-time spells while the
        // bot is moving, so only instants come out -- no risk of rooting mid-cast
        // into an eruption. A companion json-raid `suppress` rule must zero the
        // movement-hijackers (avoid aoe / reach spell / combat formation move /
        // flee) for this phase, else they'd grab the yielded tick and pull the bot
        // off-route. The MoveTo movement generator persists across the yielded
        // ticks; we re-issue it each tick (deduped) so the dance keeps steering at
        // top relevance and just hands the *rest* of the tick to the rotation.
        // Don't InterruptSpell here -- starting the move already cancels any
        // carried-in cast-time spell, and we want this tick's instant to land.
        MoveTo(bot->GetMapId(), x, y, _z, false, false, false, false,
               MovementPriority::MOVEMENT_COMBAT);
        return false;
    }

    // Default: hold the tick. MoveTo returns false once the move is a duplicate,
    // and yielding there would let a lower-priority cast halt the bot mid-floor.
    // Holding keeps the dance owning movement until the bot reaches the safe zone.
    botAI->InterruptSpell();
    MoveTo(bot->GetMapId(), x, y, _z, false, false, false, false,
           MovementPriority::MOVEMENT_COMBAT);
    return true;
}
