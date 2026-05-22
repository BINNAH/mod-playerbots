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

// Shape "attack_priority": focus the lowest-health living add whose name is in
// `adds` (comma-list) if any are up, otherwise fall back to `boss`. Yields when
// already on the right target. The generic "kill adds first, then boss" pattern.
// Qualifier: "adds=<name[,name]>|boss=<bossname>".
class JsonAttackPriorityAction : public AttackAction, public Qualified
{
public:
    JsonAttackPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "json attackpriority") {}

    void Qualify(std::string const qual) override;
    bool Execute(Event event) override;
    bool isUseful() override { return !(_addsCsv.empty() && _boss.empty()); }
    std::string const getName() override { return "json attackpriority::" + qualifier; }

private:
    std::string _addsCsv;
    std::string _boss;
};

// Shape "attack_target": attack the named creature (e.g. focus the boss).
// No-ops (returns false) when already on that target. Qualifier: "<name>".
class JsonAttackTargetAction : public AttackAction, public Qualified
{
public:
    JsonAttackTargetAction(PlayerbotAI* ai) : AttackAction(ai, "json attack") {}

    void Qualify(std::string const qual) override { Qualified::Qualify(qual); _target = qual; }
    bool Execute(Event event) override;
    bool isUseful() override { return !_target.empty(); }
    std::string const getName() override { return "json attack::" + qualifier; }

private:
    std::string _target;
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

#endif
