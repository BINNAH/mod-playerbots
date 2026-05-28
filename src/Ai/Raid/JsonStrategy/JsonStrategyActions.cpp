#include "JsonStrategyActions.h"

#include "JsonStrategyLoader.h"  // RaidJsonMode (pull-epoch -- fresh-pull latch reset)
#include "JsonStrategyShapeUtil.h"
#include "Config.h"            // sConfigMgr -- RaidJson.Debug gate for the climb log
#include "Playerbots.h"
#include "Timer.h"

#include <algorithm>
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
    unsigned int interval = 0;
    int parsed = std::sscanf(qual.c_str(), "%f,%f,%f,%u,%d,%u", &x, &y, &r, &segments, &cw, &interval);
    if (parsed < 4 || segments == 0)
        return;

    center_x = x;
    center_y = y;
    radius = r;
    intervals = segments;
    clockwise = (cw != 0);
    _stepIntervalMs = (parsed >= 6) ? interval : 0u;
    // A re-qualify (reload with edited params) is a fresh action — drop the clock.
    _lastStepMs = 0;
    _curWp = 0;
    _haveCurWp = false;

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
    uint32 step = clockwise ? 1u : (intervals - 1u);

    // Continuous (default): advance off the bot's current nearest waypoint every
    // call, so the bot perpetually walks the ring.
    if (_stepIntervalMs == 0)
        return (FindNearestWaypoint() + step) % intervals;

    // Stepped: hold the current target waypoint and only advance one slot every
    // `interval` ms — the tank parks and attacks between drags (cadence-paced
    // kite). Per-bot state on the instance. Re-anchor to the nearest waypoint
    // after a long dormant gap (a fresh pull after the rule sat idle) so we don't
    // drag the tank across the room to a slot left over from the last fight; the
    // gap floor stays above the step interval so it never false-fires mid-fight.
    uint32 now = getMSTime();
    uint32 reanchorGap = std::max<uint32>(30000u, _stepIntervalMs * 2u);
    if (!_haveCurWp || (now - _lastStepMs > reanchorGap))
    {
        _curWp = (FindNearestWaypoint() + step) % intervals;
        _lastStepMs = now;
        _haveCurWp = true;
        return _curWp;
    }
    if (now - _lastStepMs >= _stepIntervalMs)
    {
        _curWp = (_curWp + step) % intervals;
        _lastStepMs = now;
    }
    return _curWp;
}

// ---------------------------------------------------------------------------
// position_vs_boss
// ---------------------------------------------------------------------------
void JsonPositionVsBossAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _valid = false;

    _boss = JsonKv(qual, "boss");
    _distance = (float)std::atof(JsonKv(qual, "distance", "0").c_str());
    _angleOffset = (float)std::atof(JsonKv(qual, "angle", "0").c_str());
    _onlyIfCloser = JsonKv(qual, "closer", "0") == "1";

    std::string anchor = JsonKv(qual, "anchor", "radial_out");
    _anchorRadial = false;
    if (anchor == "radial_out")
        _anchorRadial = true;
    else if (anchor == "behind")
        _baseAngle = (float)M_PI;
    else if (anchor == "front")
        _baseAngle = 0.0f;
    else if (anchor == "left")
        _baseAngle = (float)M_PI / 2.0f;
    else if (anchor == "right")
        _baseAngle = -(float)M_PI / 2.0f;
    else
        return;  // unknown anchor — leave invalid

    if (_distance <= 0.0f)
        return;
    _valid = true;
}

bool JsonPositionVsBossAction::Execute(Event /*event*/)
{
    if (!_valid)
        return false;

    Unit* boss = _boss.empty() ? AI_VALUE(Unit*, "boss target")
                               : AI_VALUE2(Unit*, "find target", _boss);
    if (!boss)
        return false;

    // "Maintain range": once already far enough out there's nothing to do, so
    // yield the tick to a lower-priority action (e.g. attacking).
    if (_onlyIfCloser && bot->GetExactDist2d(boss) >= _distance)
        return false;

    // radial_out keys off the boss->bot bearing (back off wherever you stand);
    // the facing modes key off the boss's orientation + the chosen base bearing.
    float angle = _anchorRadial ? boss->GetAngle(bot)
                                : boss->GetOrientation() + _baseAngle;
    angle += _angleOffset;

    float x = boss->GetPositionX() + std::cos(angle) * _distance;
    float y = boss->GetPositionY() + std::sin(angle) * _distance;
    float z = boss->GetPositionZ();

    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

// ---------------------------------------------------------------------------
// stack_point
// ---------------------------------------------------------------------------
void JsonStackPointAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _valid = false;

    float px = 0.0f, py = 0.0f, r = 5.0f, pz = 0.0f;
    int hold = 0;
    int parsed = std::sscanf(qual.c_str(), "%f,%f,%f,%d,%f", &px, &py, &r, &hold, &pz);
    if (parsed < 2)
        return;

    x = px;
    y = py;
    distance = (parsed >= 3 && r > 0.0f) ? r : 5.0f;
    _hold = (parsed >= 4 && hold != 0);
    _hasZ = (parsed >= 5);
    _z = pz;
    _valid = true;
}

bool JsonStackPointAction::Execute(Event event)
{
    if (!_valid)
        return false;

    // Explicit anchor z (an ELEVATED spot, e.g. Thaddius's add platforms): move in
    // 3D. MoveInsideAction yields on 2D distance at the bot's CURRENT z, so a bot on
    // the floor below the platform thinks it has arrived and never climbs. Route to
    // (x,y,z) and only yield once within `distance` in 3D (the pathfinder climbs the
    // ramp, same as the C++ ThaddiusAttackNearestPetAction's MoveTo(tankPosZ)).
    if (_hasZ)
    {
        if (bot->GetExactDist(x, y, _z) > distance &&
            MoveTo(bot->GetMapId(), x, y, _z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            return true;
        return _hold;
    }

    // MoveInsideAction returns true while still pathing to the point, false once
    // within `distance`. When parked, hold=1 keeps owning the tick (stand still)
    // so the bot doesn't fall through to generic combat and run off to the boss;
    // hold=0 yields so a lower-priority action can run.
    if (MoveInsideAction::Execute(event))
        return true;
    return _hold;
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
    _sticky = JsonKv(qual, "sticky", "1") != "0";
    _maxHpPct = (float)std::atof(JsonKv(qual, "maxhp", "0").c_str());
    _maxRange = (float)std::atof(JsonKv(qual, "maxrange", "0").c_str());
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

            // Optional candidate filters: only adds at/below a HP% (e.g. the 5%
            // Decimate chow) and/or within a range. When these zero out every
            // candidate the action falls back to the boss below, so "burst the
            // decimated chow, else tunnel the boss" needs no separate phase gate.
            if (_maxHpPct > 0.0f && unit->GetHealthPct() > _maxHpPct)
                continue;
            if (_maxRange > 0.0f && bot->GetExactDist2d(unit) > _maxRange)
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
        // sticky=0 disables the stick and re-picks `best` every tick — for a tank
        // that must swap to whatever add is now nearest after Magnetic Pull yanks
        // it to the other platform (else it runs back to its original add).
        target = (currentMatches && _sticky) ? current : best;
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
// move_to_target
// ---------------------------------------------------------------------------
void JsonMoveToTargetAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _targetsCsv = JsonKv(qual, "target");
    _boss = JsonKv(qual, "boss");
    // Default detect = nearest (proximity scan): the point of this shape is to run
    // to something the bot does NOT threaten yet (the pull), where a threat scan
    // would find nothing. "threat" is opt-in for the in-combat "close on my add".
    _nearestDetect = (JsonKv(qual, "detect", "nearest") != "threat");
    _distance = (float)std::atof(JsonKv(qual, "distance", "0").c_str());
    _thenCsv = JsonKv(qual, "then");
    _lastLogMs = 0;
    _reached = false;
    _everReached = false;
    _valid = !(_targetsCsv.empty() && _boss.empty());
}

bool JsonMoveToTargetAction::Execute(Event /*event*/)
{
    if (!_valid)
        return false;

    // FRESH-PULL RESET: when `.rjson pull` is called the RaidJsonMode pull-epoch
    // bumps; on mismatch with this action's last-seen, clear BOTH latches so we
    // re-climb to the NAMED `target` (re-establishes the MT/OT split). Required
    // because re-pulling a wipe doesn't clear the bot's IsInCombat() flag, so the
    // existing `!IsInCombat() && far` reset failed and `_everReached` survived --
    // seen in the kill-attempt log (2026-05-28): Gheed had _everReached=true from
    // a prior swap and headed to STALAGG on re-pull (his `then` nearest add) while
    // Luucious headed to FEUGEN, leaving each tank on the WRONG add and the add
    // they were *supposed* to tank with no one on it (DPS/healers there died).
    uint32 epoch = RaidJsonMode::instance().PullEpoch();
    if (epoch != _lastSeenPullEpoch)
    {
        _lastSeenPullEpoch = epoch;
        _reached = false;
        _everReached = false;
    }

    // Pick the NEAREST live creature matching `target`. The candidate source is a
    // proximity scan by default so it sees off-threat adds (the pull); falls back
    // to the boss by name when no named target is alive.
    // After the bot has engaged once (_everReached), a rule with a `then` set re-paths
    // to the NEAREST of that set instead of the original named `target` -- so a tank
    // thrown to the other platform by Magnetic Pull climbs to the add it landed on, not
    // back to its original. Before the first reach, `target` drives the climb so the
    // initial MT/OT split holds.
    std::string const& activeCsv = (_everReached && !_thenCsv.empty()) ? _thenCsv : _targetsCsv;
    Unit* target = nullptr;
    if (!activeCsv.empty())
    {
        std::vector<std::string> tokens = JsonSplit(activeCsv, ',');
        GuidVector candidates = _nearestDetect ? AI_VALUE(GuidVector, "nearest npcs")
                                               : AI_VALUE(GuidVector, "attackers");
        float bestDist = 0.0f;
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
            float d = bot->GetExactDist(unit);
            if (!target || d < bestDist)
            {
                target = unit;
                bestDist = d;
            }
        }
    }
    if (!target && !_boss.empty())
        target = AI_VALUE2(Unit*, "find target", _boss);

    if (!target)
        return false;

    float dist3d = bot->GetExactDist(target);

    // Climb-then-hand-off LATCH (per-bot). This action's only job is to CLIMB the bot
    // up to its add; once it has REACHED the add, the proven in-combat C++ owns
    // positioning from there (parks ranged at the anchor, swaps tanks on Magnetic
    // Pull). Driving past that point dragged healers off the anchor chasing the
    // moving add, and hauled a tank back to its original add after a swap. We latch
    // on REACHING -- NOT on "in combat": a bot flagged in combat early (an off-tank
    // taunting, or anyone caught by Static Field AoE while still on the ramp) must
    // keep climbing, not hand off to the C++ -- whose MoveTo can't climb -- and get
    // stuck at the bottom (the Magicguyman case). Re-arm only when plainly reset:
    // out of combat AND far from the add (a wipe / fresh pull), so the latch persists
    // through the whole engagement, including a Magnetic Pull that legitimately
    // throws a tank far from its NAMED add (it then tanks the nearest add via C++).
    if (!bot->IsInCombat() && dist3d > _distance + 10.0f)
    {
        // Genuine reset (wipe / fresh pull): out of combat AND far. Clear BOTH latches
        // so the next pull re-climbs to the named `target` and re-establishes the split.
        _reached = false;
        _everReached = false;
    }
    else if (_reached && !_thenCsv.empty() && dist3d > _distance + 40.0f)
    {
        // SWAP RECOVERY (in combat): a Magnetic Pull flung this tank far off its add
        // (e.g. z~338, 80y away). The in-combat C++ MoveTo can't climb back, so re-arm
        // the per-tick latch to drive a fresh exact_waypoint climb to the (now nearest,
        // via `then`) add. _everReached stays set, so it heads to the add it was thrown
        // onto, not the original. 40 > any normal melee jitter, so steady tanking (dist
        // ~6) never trips it -- only a real fling does.
        _reached = false;
    }
    if (dist3d <= _distance + 0.5f)
    {
        _reached = true;
        _everReached = true;
    }

    // Debug instrumentation, gated behind `RaidJson.Debug` (default OFF) and throttled
    // to ~1/s per bot. Watch the climb: the bot's Z should rise toward the target's Z;
    // reached=1 means handed off to the C++. Off by default because a 25-man pull spams
    // ~25 lines/sec to the console -- set `RaidJson.Debug = 1` in playerbots.conf and
    // `.reload config` when you actually need to watch a climb.
    uint32 now = getMSTime();
    bool throttleOk = (now - _lastLogMs > 1000);
    if (throttleOk)
        _lastLogMs = now;
    bool logTick = throttleOk && sConfigMgr->GetOption<bool>("RaidJson.Debug", false);
    if (logTick)
    {
        LOG_INFO("playerbots",
                 "[RaidJson][move_to_target] {} -> {} (entry {}): bot=({:.1f},{:.1f},{:.1f}) "
                 "target=({:.1f},{:.1f},{:.1f}) dist={:.1f} stopAt={:.1f} reached={} combat={}",
                 bot->GetName(), target->GetName(), target->GetEntry(),
                 bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                 target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(),
                 dist3d, _distance, _reached ? 1 : 0, bot->IsInCombat() ? 1 : 0);
    }

    // Reached the add -> hand off: yield every tick so the in-combat C++ owns it.
    if (_reached)
        return false;

    // Path to the unit's ACTUAL position with exact_waypoint=TRUE. This is the
    // crux: the default flags route MoveTo through SearchForBestPath, which DROPS
    // the destination Z and re-snaps to the SHORTEST-path surface -- from the floor
    // that's the slime directly under an elevated platform, so the bot walks across
    // the hazard to the spot UNDER the add and never climbs (CONFIRMED in the log:
    // bots reached the add's x,y but stayed at z~292 while the add sat at z~312).
    // exact_waypoint skips that snap and keeps the add's literal Z, so recast routes
    // up the ramp to the platform poly. generatePath stays on, so it's still a real
    // navmesh path (no straight-line clip). We aim at the unit's exact spot (not the
    // Unit-overload's stop-short point, which would land mid-air short of the ledge)
    // and rely on the distance yield above to stop once on top.
    bool issued = MoveTo(bot->GetMapId(),
                         target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(),
                         false, false, false, /*exact_waypoint=*/true,
                         MovementPriority::MOVEMENT_COMBAT);
    if (!issued && logTick)
        LOG_INFO("playerbots",
                 "[RaidJson][move_to_target] {} MoveTo({}) issued no fresh spline (dist={:.1f}) "
                 "-- throttled/duplicate; HOLDING tick to stay on the climb",
                 bot->GetName(), target->GetName(), dist3d);
    // HOLD the tick while still en route (we're past `distance`), even when this
    // call issued no fresh spline. A throttled/duplicate MoveTo returns false but the
    // bot is already mid-path; if we yielded here, a lower-priority action (autopilot
    // move / idle drink-and-loot / formation) would grab the tick and drag the bot
    // off the ramp -- the up/down oscillation seen in the logs (movetotarget FAILED
    // 7427x vs OK 205x). Owning the tick until arrival keeps the climb monotonic.
    return true;
}

// ---------------------------------------------------------------------------
// snare_area
// ---------------------------------------------------------------------------
void JsonSnareAreaAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _spells.clear();
    for (std::string const& s : JsonSplit(JsonKv(qual, "spells"), ','))
        _spells.push_back(s);
    _add = JsonKv(qual, "add");
    _boss = JsonKv(qual, "boss");
    std::string t = JsonKv(qual, "target", "self");
    _targetMode = (t == "nearest") ? 1 : (t == "leak") ? 2 : 0;
    _range = (float)std::atof(JsonKv(qual, "range", "0").c_str());
}

bool JsonSnareAreaAction::Execute(Event /*event*/)
{
    if (_spells.empty() || _add.empty())
        return false;

    // Off-threat-visible scan: chow are not on a kiter's / off-tank's threat list.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");

    Unit* castTarget = nullptr;

    if (_targetMode == 0)  // self / ground-centered AoE: only fire if an add is near.
    {
        bool anyInRange = false;
        for (ObjectGuid const& guid : npcs)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || !MatchesNameOrEntry(botAI, unit, _add))
                continue;
            if (_range <= 0.0f || bot->GetExactDist2d(unit) <= _range)
            {
                anyInRange = true;
                break;
            }
        }
        if (!anyInRange)
            return false;
        castTarget = bot;
    }
    else  // nearest (1) / leak (2): pick a chow to cast on.
    {
        // leak measures from the boss (the chow nearest him is the one to stop);
        // nearest measures from the bot. The reference unit is found by proximity,
        // so it works even when no one threatens the boss.
        Unit* ref = bot;
        if (_targetMode == 2)
        {
            ref = nullptr;
            for (ObjectGuid const& guid : npcs)
            {
                Unit* unit = botAI->GetUnit(guid);
                if (unit && unit->IsAlive() && !_boss.empty() && MatchesNameOrEntry(botAI, unit, _boss))
                {
                    ref = unit;
                    break;
                }
            }
            if (!ref)
                return false;
        }

        Unit* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        for (ObjectGuid const& guid : npcs)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || !MatchesNameOrEntry(botAI, unit, _add))
                continue;
            float dist = ref->GetExactDist2d(unit);
            if (_range > 0.0f && dist > _range)
                continue;
            if (dist < bestDist)
            {
                bestDist = dist;
                best = unit;
            }
        }
        if (!best)
            return false;
        castTarget = best;
    }

    // Try each listed spell in order; the first the bot knows and has off cooldown
    // fires (CastSpell returns false otherwise), so one rule serves every class.
    for (std::string const& spell : _spells)
        if (botAI->CastSpell(spell, castTarget))
            return true;

    return false;
}

// ---------------------------------------------------------------------------
// tank_swap
// ---------------------------------------------------------------------------
void JsonTankSwapAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _aura = JsonKv(qual, "aura");
    _boss = JsonKv(qual, "boss");
    _stacks = (uint32)std::strtoul(JsonKv(qual, "stacks", "1").c_str(), nullptr, 10);
    if (_stacks == 0)
        _stacks = 1;
    _nearestDetect = JsonKv(qual, "detect") == "nearest";
    _watchMainTank = JsonKv(qual, "watch") == "maintank";
}

bool JsonTankSwapAction::Execute(Event /*event*/)
{
    if (_aura.empty() || !botAI->IsTank(bot))
        return false;

    Unit* boss = nullptr;
    if (_nearestDetect)
    {
        // Proximity find so a taunter that never threatens the boss (a Gluth chow
        // off-tank) can still locate him.
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
        for (ObjectGuid const& guid : npcs)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsAlive() && MatchesNameOrEntry(botAI, unit, _boss))
            {
                boss = unit;
                break;
            }
        }
    }
    else
    {
        boss = AI_VALUE2(Unit*, "find target", _boss);
    }
    if (!boss)
        return false;

    Unit* victim = boss->GetVictim();

    // I'm the ACTIVE tank (the boss is on me): hold it and go ham. Re-acquire the
    // boss if I drifted off it, otherwise YIELD so my own rotation pounds it
    // (nothing else is wired for the dedicated swap tank, so the tick falls through
    // to its normal combat = full threat/DPS on the boss).
    if (victim == bot)
    {
        if (bot->GetVictim() != boss)
            return Attack(boss);
        return false;
    }

    // I'm an OFF tank. Whose load triggers my taunt?
    //   watch=victim (default): the boss's current victim -> symmetric ping-pong,
    //     each tank takes over when the other hits the cap (standard 2-tank swap).
    //   watch=maintank: the designated main tank, and only the PRIMARY relief tank
    //     (assist #0) acts, so several off-tanks don't all pile on (one covers
    //     while the MT detoxes).
    bool eligible = _watchMainTank ? botAI->IsAssistTankOfIndex(bot, 0) : true;
    Unit* watched = _watchMainTank ? AI_VALUE(Unit*, "main tank") : victim;

    // checkStack (5th arg) returns the aura only at >= _stacks stacks, so the null
    // check is the "watched tank is loaded?" gate.
    if (eligible && watched && watched != bot &&
        botAI->GetAura(_aura, watched, false, false, (int)_stacks))
    {
        // Take over: get on the boss, then FORCE the class taunt. DoSpecificAction
        // bypasses relevance, so a suppressed auto-taunt elsewhere can't block it.
        if (bot->GetVictim() != boss)
            return Attack(boss);
        return botAI->DoSpecificAction("taunt spell", Event(), true);
    }

    // Not my turn. The MAIN tank YIELDS so its own attack/stack rules keep it
    // tanking the boss (and holding threat to take back) — it must engage on the
    // pull, not stand idle. A dedicated swap OFF tank STANDS READY (owns the tick)
    // so it doesn't fall through to generic combat and hit the boss before the swap
    // is due. A non-eligible off tank (maintank-mode, not the relief tank) yields so
    // its other rules (e.g. holding adds) run instead.
    if (botAI->IsMainTank(bot))
        return false;
    return eligible;
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
