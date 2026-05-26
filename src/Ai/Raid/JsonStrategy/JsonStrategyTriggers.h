/*
 * JSON-driven raid strategy — generic Level-2 triggers.
 *
 * Parameters arrive via the factory "::qualifier" channel (see Qualified).
 */
#ifndef _PLAYERBOT_JSONSTRATEGYTRIGGERS_H
#define _PLAYERBOT_JSONSTRATEGYTRIGGERS_H

#include "NamedObjectContext.h"  // Qualified
#include "Trigger.h"

// Shape T7 "encounter_active": the universal raid condition. Active while a
// named boss is engaged/found, optionally narrowed by:
//   - role : only fire for bots of a role (comma-list, OR semantics):
//            maintank|offtank|tank|notmaintank|nontank|ranged|melee|healer|dps|all
//   - aura : phase gate on a boss aura by name (e.g. "locust swarm")
//   - has  : 1 = require the aura present, 0 = require it absent (default 1)
//
// Evaluated per-bot, so the same rule string fans out to the right bots. This
// one trigger shape expresses "who + when" for the whole JSON catalog.
//
// Qualifier formats (both accepted):
//   "anub'rekhan"                                  (bare boss name; no filters)
//   "boss=anub'rekhan|role=maintank|aura=locust swarm|has=1"   (kv form)
class JsonEncounterActiveTrigger : public Trigger, public Qualified
{
public:
    JsonEncounterActiveTrigger(PlayerbotAI* ai) : Trigger(ai, "json encounter") {}

    bool IsActive() override;
    std::string const getName() override { return "json encounter::" + qualifier; }
};

// Shape "pre_cast_window": fires in the short window just BEFORE a boss's
// periodic cast, so externals/defensives are pre-applied and carry through the
// hit (e.g. Maexxna's 40s Web Spray raid stun — reactive healing can't help
// because the healers are stunned too). The boss exposes no readable timer, so
// this PREDICTS from a fixed cadence (`first` then every `interval` ms) and
// re-anchors whenever the cast is actually observed, keeping the prediction
// locked to the real rhythm instead of drifting. The window opens `lead` ms
// before the prediction and holds `tail` ms past it. Optionally narrowed by a
// required boss `aura` (phase gate, e.g. sub-30% "frenzy") and a `role` list.
//
// Per-bot predicted clock lives on the instance (triggers are per-bot and
// persistent), mirroring the C++ boss-helper state machines.
//
// Qualifier: "boss=<name>|spell=<name-or-id>|interval=<ms>|first=<ms>|
//             lead=<ms>|tail=<ms>|aura=<name>|role=<csv>".
class JsonPreCastWindowTrigger : public Trigger, public Qualified
{
public:
    JsonPreCastWindowTrigger(PlayerbotAI* ai) : Trigger(ai, "json precast") {}

    void Qualify(std::string const qual) override;
    bool IsActive() override;
    std::string const getName() override { return "json precast::" + qualifier; }

private:
    bool AnchorObserved(Unit* boss);

    std::string _boss;
    std::string _spell;        // anchor spell, name or numeric id ("" = pure clock)
    bool        _spellIsId = false;
    uint32      _spellId = 0;
    std::string _aura;         // optional required boss aura (phase gate)
    std::string _role;         // optional role filter (comma-list, OR semantics)
    uint32      _interval = 0;
    uint32      _firstAt = 0;
    uint32      _lead = 4000;
    uint32      _tail = 6000;

    // Per-bot predicted clock, re-anchored on each observed cast.
    uint32 _combatStartMs = 0;
    uint32 _nextCastMs = 0;
    bool   _castSeen = false;
};

#endif
