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

// Shape A2 "orbit_point": walk the ring around (x, y). By default (interval 0)
// the orbit is CONTINUOUS -- it advances off the bot's nearest waypoint every
// tick, a smooth perpetual kite. With a non-zero `interval` (ms) it is STEPPED:
// the bot parks on the current waypoint and only advances one slot every
// `interval` ms, reproducing a cadence-paced kite (e.g. Grobbulus's ~15s/cloud
// rotation) so the tank holds threat between drags. The step clock + current
// waypoint are per-bot state on the instance (instances are per-bot-context,
// same as timed_safe_zone's clock); it re-anchors to the nearest waypoint after
// a long dormant gap so a fresh pull doesn't drag the tank to a stale slot.
// Qualifier: "x,y,radius,segments,clockwise,interval" (interval optional).
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
    uint32 _stepIntervalMs = 0;  // 0 = continuous; >0 = advance one slot per interval
    uint32 _lastStepMs = 0;      // per-bot: when the current waypoint was last advanced
    uint32 _curWp = 0;           // per-bot: currently-targeted waypoint (stepped mode)
    bool   _haveCurWp = false;   // per-bot: has _curWp been anchored yet
};

// Shape A3/A4 "position_vs_boss": move to a single spot defined RELATIVE to the
// boss -- the generic form of the Grobbulus "go behind" / "move away" actions
// (and a fit for Onyxia move-to-side, Yogg face-away, any "stand <d> off the
// boss in direction X" mechanic). Two anchoring modes:
//   anchor = "radial_out" -> straight out along the boss->bot bearing (back off
//            to range wherever you currently are). Pair with only_if_closer to
//            get "maintain >= distance" (the GrobbulusMoveAway semantics).
//          = "behind"/"front"/"left"/"right" -> a fixed bearing off the boss's
//            FACING (orientation +pi / +0 / +pi/2 / -pi/2), plus `angle_offset`.
//            "behind" + offset pi/8 reproduces GrobbulusGoBehind.
// distance       : how far from the boss to stand.
// angle_offset   : extra radians added to the anchor bearing.
// only_if_closer : true -> do nothing (yield) while already >= distance away.
// Qualifier: "anchor=..|distance=..|angle=..|closer=0|1|boss=<name>".
class JsonPositionVsBossAction : public MovementAction, public Qualified
{
public:
    JsonPositionVsBossAction(PlayerbotAI* ai) : MovementAction(ai, "json posboss") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return _valid; }
    std::string const getName() override { return "json posboss::" + qualifier; }

private:
    std::string _boss;
    float _distance = 0.0f;
    float _angleOffset = 0.0f;
    float _baseAngle = 0.0f;     // boss-facing modes: the bearing off orientation
    bool  _anchorRadial = false; // true = radial_out (ignore boss facing)
    bool  _onlyIfCloser = false;
    bool  _valid = false;
};

// Shape "stack_point": move to (x, y) and stay within `radius` of it (tight
// raid stack). `hold` (default 0): once parked within `radius`, 0 = YIELD the
// tick so lower-priority actions run (the raid-stack default); 1 = OWN the tick
// (stand and do nothing) so the bot does NOT fall through to its generic combat
// and go attack the boss — the fix for a bot parked off the boss with no current
// job (e.g. a Gluth off-tank waiting for the first chow wave). Optional `z`: an
// explicit anchor height for an ELEVATED spot (Thaddius's add platforms) — moves
// in 3D so the bot climbs instead of yielding on the floor below (2D distance).
// Omit z for ground stacks (back-compat). Qualifier: "x,y,radius,hold[,z]".
class JsonStackPointAction : public MoveInsideAction, public Qualified
{
public:
    JsonStackPointAction(PlayerbotAI* ai) : MoveInsideAction(ai, 0.0f, 0.0f, 5.0f) {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return _valid; }
    std::string const getName() override { return "json stack::" + qualifier; }

private:
    bool _valid = false;
    bool _hold = false;
    bool _hasZ = false;  // explicit anchor z (elevated platforms) -> 3D move, not floor-z
    float _z = 0.0f;
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
// ENTRY id, optionally further filtered to those at/below `max_hp_pct` health
// and/or within `max_range` yards of the bot (0 = no filter). The filters make
// the "burst the decimated adds, otherwise tunnel the boss" pattern pure data:
// when nothing passes the filter the action falls back to `boss`, so a chow that
// is only a candidate while at <=10% HP self-gates to the Decimate burn window.
// Sticks to the current pick while it's still a live match (no thrash that would
// cancel in-flight casts), then falls back to `boss`. Yields when already on the
// right target. With `targets` empty it is just "attack the boss". Qualifier:
// "targets=<name-or-entry[,...]>|boss=<name>|detect=<..>|select=<..>|
//  maxhp=<pct>|maxrange=<y>".
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
    bool _sticky = true;          // 0 = re-pick best every tick (tank swap on Magnetic Pull)
    float _maxHpPct = 0.0f;       // 0 = no filter; else only targets at/below this HP%
    float _maxRange = 0.0f;       // 0 = no filter; else only targets within this distance
};

// Shape "snare_area": cast a control / AoE-threat spell on adds while a separate
// lower-priority shape (orbit_point / stack_point) holds the movement. Tries each
// spell in the list the bot KNOWS and has off cooldown, in order, casting the
// first that fires -- the same self-gating idiom as Four Horsemen's "opening
// defensive", so one rule can list every class's option and each bot fires its
// own. Generalizes the C++ CastZombieThreat per-class switch. It YIELDS (returns
// false) when no add qualifies or no spell is castable, so it layers cleanly over
// a movement shape underneath.
//   spells : comma-list of spell names, tried in order (first castable wins).
//   add    : add name / entry id the targeting modes look for.
//   target : self    -> cast on the bot (self / ground-centered AoE: Frost Nova,
//                       Consecration, Arcane Explosion, Death and Decay). Requires
//                       >= 1 matching add within `range` (else yields) so a
//                       cooldown isn't burned on an empty floor.
//            nearest -> cast on the nearest matching add within `range` of the bot.
//            leak    -> cast on the matching add NEAREST THE BOSS (the one about to
//                       reach it), within `range` of the boss when `range` > 0.
//   range  : self/nearest measure from the BOT, leak from the boss (0 = unbounded).
//   boss   : boss name (needed for target=leak).
// Qualifier:
// "spells=a,b,c|add=<name/entry>|target=<self|nearest|leak>|range=<y>|boss=<name>".
class JsonSnareAreaAction : public Action, public Qualified
{
public:
    JsonSnareAreaAction(PlayerbotAI* ai) : Action(ai, "json snarearea") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return !_spells.empty(); }
    std::string const getName() override { return "json snarearea::" + qualifier; }

private:
    std::vector<std::string> _spells;
    std::string _add;
    std::string _boss;
    int _targetMode = 0;   // 0 = self, 1 = nearest, 2 = leak
    float _range = 0.0f;
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

// Shape A16 "tank_swap": the stacking-debuff tank rotation. While a tank carries
// `stacks`+ of the `aura` (a healing-reduction / armor-shred debuff the boss
// stacks on whoever it hits — Mortal Wound, Crunch Armor, Gormok's Impale, ...),
// another tank taunts the boss off them. Self-contained + self-gating: it only
// acts when a swap is actually due, so wire it under any encounter trigger and it
// stays quiet otherwise. Casts the taunt via DoSpecificAction("taunt spell"),
// which FORCE-runs the class taunt (warrior Taunt / pala Hand of Reckoning / DK
// Dark Command / druid Growl) regardless of relevance — so a `suppress` of
// "taunt spell" (to stop a relieved tank auto-taunting back) does not block the
// swap itself.
//   aura   : stacking debuff name to watch (matched by name, difficulty-proof).
//   stacks : taunt once the watched tank is at/above this many stacks (default 1).
//   boss   : boss name (default file `boss`).
//   detect : how to find the boss — threat (default) or nearest (proximity, for a
//            taunter that doesn't yet threaten the boss, e.g. a Gluth chow off-tank).
//   watch  : whose stacks gate the swap.
//            victim   (default) -> the boss's CURRENT victim (the active tank).
//                       Symmetric ping-pong: after the taunt the new tank has fresh
//                       (low) stacks, so no one re-taunts until they rebuild — the
//                       standard 2-tank rotation (Kologarn, Festergut, AQ40, ...).
//            maintank -> the designated MAIN tank. The primary relief tank
//                       (assist-tank #0) holds the boss (re-taunt + stay on it)
//                       while the main tank is loaded, releasing once its stacks
//                       decay — the "one off-tank covers while the MT detoxes"
//                       form (Gluth Mortal Wound). Other off-tanks keep their job.
// Qualifier: "aura=<name>|stacks=<n>|boss=<name>|detect=<threat|nearest>|watch=<victim|maintank>".
class JsonTankSwapAction : public AttackAction, public Qualified
{
public:
    JsonTankSwapAction(PlayerbotAI* ai) : AttackAction(ai, "json tankswap") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return !_aura.empty(); }
    std::string const getName() override { return "json tankswap::" + qualifier; }

private:
    std::string _aura;
    std::string _boss;
    uint32 _stacks = 1;
    bool _nearestDetect = false;  // find the boss by proximity vs the threat list
    bool _watchMainTank = false;  // gate on the main tank's stacks vs the boss's victim
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
