/*
 * JSON-driven raid strategy — generic Level-2 actions.
 *
 * One C++ class per catalog "shape". Parameters arrive through the factory's
 * "::qualifier" channel (see NamedObjectContext): the factory builds the base
 * object then calls Qualify(<params>), which we parse. Each distinct qualifier
 * is cached as its own instance, so editing JSON params and reloading yields a
 * fresh action with the new numbers (no rebuild).
 *
 * These are GENERIC shapes meant to be reused across bosses; WHO runs them and
 * WHEN is decided by the rule's trigger (role + phase), not baked in here.
 */
#ifndef _PLAYERBOT_JSONSTRATEGYACTIONS_H
#define _PLAYERBOT_JSONSTRATEGYACTIONS_H

#include "AttackAction.h"           // AttackAction (-> MovementAction)
#include "MovementActions.h"        // RotateAroundTheCenterPointAction, MoveInsideAction
#include "NamedObjectContext.h"     // Qualified

#include <cstdint>
#include <vector>

// Shape A2 "orbit_point": continuously walk the ring around (x, y).
// Qualifier: "x,y,radius,segments,clockwise".
class JsonOrbitPointAction : public RotateAroundTheCenterPointAction, public Qualified
{
public:
    JsonOrbitPointAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "json orbit", 0.0f, 0.0f, 40.0f, 16) {}

    void Qualify(std::string const qual) override;
    bool isUseful() override { return _valid; }
    uint32 GetCurrWaypoint() override;
    std::string const getName() override { return "json orbit::" + qualifier; }

private:
    bool _valid = false;
};

// Shape "stack_point": move to (x, y) and stay within `radius` of it (tight
// raid stack). Qualifier: "x,y,radius".
class JsonStackPointAction : public MoveInsideAction, public Qualified
{
public:
    JsonStackPointAction(PlayerbotAI* ai) : MoveInsideAction(ai, 0.0f, 0.0f, 5.0f) {}

    void Qualify(std::string const qual) override;
    bool isUseful() override { return _valid; }
    std::string const getName() override { return "json stack::" + qualifier; }

private:
    bool _valid = false;
};

// Shape "spread": move away from the nearest OTHER ranged/healer within `radius`
// (point-blank AoE / chain-target avoidance, e.g. Impale). Only spreads from
// other ranged/healers — never from the melee/tank stack — and repositions at
// most once per `min_interval` ms so casters aren't perpetually interrupted.
// Returns false when already clear / on cooldown, so a lower-priority action
// (e.g. attack_target) runs the same tick. Qualifier: "radius,min_interval".
class JsonSpreadAction : public MovementAction, public Qualified
{
public:
    JsonSpreadAction(PlayerbotAI* ai) : MovementAction(ai, "json spread") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return _valid; }
    std::string const getName() override { return "json spread::" + qualifier; }

private:
    float _radius = 0.0f;
    uint32 _minInterval = 3000;
    bool _valid = false;
};

// Shape "attack": pick one creature from a candidate set and attack it, falling
// back to `boss` when none are up. Generalizes the former attack_target,
// attack_priority and attack_nearest shapes through two knobs:
//   detect = "threat"  -> scan the threat / "attackers" list (default).
//          = "nearest" -> scan nearby NPCs, so OFF-THREAT objects are visible
//                         (e.g. Maexxna's Web Wrap cocoons, freshly-spawned adds
//                         no one has aggro on yet).
//   select = "lowest_hp" -> focus the lowest-health match (default for detect
//                           "threat"): the "kill adds first" pattern.
//          = "nearest"   -> focus the closest match (default for detect
//                           "nearest"): grab the cocoon nearest to free fastest.
// Candidates match `targets` (comma-list) by case-insensitive NAME or numeric
// ENTRY id. Sticks to the current pick while it's still a live match (no thrash
// that would cancel in-flight casts), then falls back to `boss`. Yields when
// already on the right target. With `targets` empty it is just "attack the boss".
// Qualifier: "targets=<name-or-entry[,...]>|boss=<name>|detect=<..>|select=<..>".
class JsonAttackAction : public AttackAction, public Qualified
{
public:
    JsonAttackAction(PlayerbotAI* ai) : AttackAction(ai, "json attackpick") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return !(_targetsCsv.empty() && _boss.empty()); }
    std::string const getName() override { return "json attackpick::" + qualifier; }

private:
    std::string _targetsCsv;
    std::string _boss;
    bool _nearestDetect = false;  // scan nearby NPCs (off-threat) vs the threat list
    bool _nearestSelect = false;  // pick the closest match vs the lowest-health match
};

// Shape "tank_adds": off-tank (assist-tank index 0) picks up every living add
// matching `add` (by name) and drags them onto the main tank / boss so the raid
// can cleave. Mirrors the proven NothAddTankAction idiom (attack -> taunt ->
// reposition). Qualifier: "add=<addname>|boss=<bossname>".
class JsonTankAddsAction : public AttackAction, public Qualified
{
public:
    JsonTankAddsAction(PlayerbotAI* ai) : AttackAction(ai, "json tankadds") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return !_addName.empty(); }
    std::string const getName() override { return "json tankadds::" + qualifier; }

private:
    std::string _addName;
    std::string _bossName;
};

// Shape A6 "timed_safe_zone": the eruption-dance pattern. The encounter splits a
// room into fixed `zones`; on a deterministic clock one zone after another is the
// only safe place to stand. Given that geometry as data, predict which zone is
// safe right now and stand on it -- the generic form of HeiganDanceAction (and a
// fit for any fixed-pattern "safe zone at time T" mechanic).
//
//   zones    : flat "x1,y1,x2,y2,..." -- the safe-spot centers.
//   pattern  : "i0,i1,..." -- index into `zones` for the K-th eruption (a period
//              equal to the list length; e.g. Heigan's triangle wave 3,2,1,0,1,2).
//   z        : floor height shared by every zone.
//   first_at : ms from phase start to the first eruption.
//   interval : ms between eruptions thereafter.
//   hold     : 1 -> own the tick even when parked on the safe zone (tight cadence,
//              e.g. Heigan fast phase -- no casting, relocate instantly every tick).
//              0 -> yield once parked so DPS/heal rotations run between eruptions.
//   cw       : "cast while moving". 1 -> while EN ROUTE, yield the tick instead of
//              owning it, so the bot's rotation fires INSTANTS as it relocates (the
//              engine refuses cast-time spells while moving, so only instants come
//              out -- no risk of rooting into an eruption). Requires a companion
//              json-raid `suppress` rule zeroing the movement-hijackers (avoid aoe /
//              reach spell / combat formation move / flee) for the dance's phase, or
//              they grab the yielded tick. 0 (default) -> hold the tick while moving.
//   tol      : in-position tolerance (default 5.0).
//
// The phase clock is per-bot state: anchored on first run and re-anchored after a
// long idle gap (the rule going dormant across the other phase), so each instance
// tracks its own phase. WHICH phase / WHO dances is the rule's trigger's job; this
// action just needs the cadence for the phase it's wired under. Qualifier:
// "zones=..|pattern=..|z=..|first=..|interval=..|hold=..|tol=..".
class JsonTimedSafeZoneAction : public MovementAction, public Qualified
{
public:
    JsonTimedSafeZoneAction(PlayerbotAI* ai) : MovementAction(ai, "json safezone") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return _valid; }
    std::string const getName() override { return "json safezone::" + qualifier; }

private:
    std::vector<float> _zones;     // flat x,y pairs
    std::vector<uint8> _pattern;   // safe-zone index per eruption
    float _z = 0.0f;
    uint32 _firstAt = 0;
    uint32 _interval = 0;
    bool _hold = false;
    bool _castWhileMoving = false;
    float _tol = 5.0f;
    bool _valid = false;
    // Per-bot phase clock (see Execute).
    uint32 _phaseStartMs = 0;
    uint32 _lastSeenMs = 0;
};

#endif
