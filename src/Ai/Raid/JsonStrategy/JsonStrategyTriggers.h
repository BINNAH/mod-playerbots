/*
 * JSON-driven raid strategy — generic Level-2 triggers.
 *
 * Parameters arrive via the factory "::qualifier" channel (see Qualified).
 */
#ifndef _PLAYERBOT_JSONSTRATEGYTRIGGERS_H
#define _PLAYERBOT_JSONSTRATEGYTRIGGERS_H

#include "NamedObjectContext.h"  // Qualified
#include "Trigger.h"

#include <vector>

// Shape T7 "encounter_active": the universal raid condition. Active while a
// named boss is engaged/found, optionally narrowed by:
//   - role : only fire for bots of a role (comma-list, OR semantics):
//            maintank|offtank|tank|notmaintank|nontank|ranged|melee|healer|dps|all
//   - aura : phase gate on a boss aura by name (e.g. "locust swarm")
//   - has  : 1 = require the aura present, 0 = require it absent (default 1)
//   - detect : how the boss is located. "threat" (default) = the find-target /
//              threat list. "nearest" = a proximity scan ("nearest npcs"), which
//              is THREAT-INDEPENDENT -- it sees the boss even for a bot that never
//              builds threat on him (kiters / off-tanks who only ever touch the
//              adds). Use it to gate their rules; everyone who actually fights the
//              boss can stay on the cheaper default.
//
// Evaluated per-bot, so the same rule string fans out to the right bots. This
// one trigger shape expresses "who + when" for the whole JSON catalog.
//
// Qualifier formats (both accepted):
//   "anub'rekhan"                                  (bare boss name; no filters)
//   "boss=anub'rekhan|role=maintank|aura=locust swarm|has=1|detect=threat"  (kv form)
class JsonEncounterActiveTrigger : public Trigger, public Qualified
{
public:
    JsonEncounterActiveTrigger(PlayerbotAI* ai) : Trigger(ai, "json encounter") {}

    bool IsActive() override;
    std::string const getName() override { return "json encounter::" + qualifier; }
};

// Shape "adds_near": fires while at least `count` living creatures matching `add`
// (name or entry id) are within `range` yards of the bot (of=self, the default)
// or the boss (of=boss; needs a `boss` name). Detection is a proximity scan
// ("nearest npcs"), so it is THREAT-INDEPENDENT -- it sees adds/boss this bot has
// no aggro on, the gate the threat-based encounter_active can't give kiters /
// off-tanks. Optionally narrowed by `role` (same comma-list tokens as
// encounter_active) so e.g. only off-tanks react to a chow pile-up. Drives
// add-control rules (snare / AoE-threat) and count-based switches ("kite once
// >= 6 chow pile on me"). Qualifier:
// "add=<name/entry>|range=<y>|count=<n>|of=<self|boss>|role=<csv>|boss=<name>".
class JsonAddsNearTrigger : public Trigger, public Qualified
{
public:
    JsonAddsNearTrigger(PlayerbotAI* ai) : Trigger(ai, "json addsnear") {}

    bool IsActive() override;
    std::string const getName() override { return "json addsnear::" + qualifier; }
};

// Shape "target_hp_ahead": fires while the bot's CURRENT TARGET is at/below
// `below` HP% AND at least one OTHER named creature is >= `margin` HP% higher.
// The cross-target HP compare a single-target trigger can't do: the death-sync
// throttle for twin adds that must die together (Thaddius's Feugen + Stalagg).
// Pair it with a `suppress` rule listing the "@damage" category token to zero
// the bot's damage casts while its add is too far ahead, so the other catches
// up — the data form of ThaddiusGenericMultiplier's <=40%/>=3% DPS clamp. The
// `others` are located by a proximity scan ("nearest npcs"), so it is
// THREAT-INDEPENDENT: a bot reads both adds' HP even though it only ever
// threatens the one it tanks/DPSes. The bot's own current target is excluded
// from `others`, so naming both adds is fine.
// Qualifier: "others=<name/entry csv>|margin=<pct>|below=<pct>".
class JsonTargetHpAheadTrigger : public Trigger, public Qualified
{
public:
    JsonTargetHpAheadTrigger(PlayerbotAI* ai) : Trigger(ai, "json hpahead") {}

    bool IsActive() override;
    std::string const getName() override { return "json hpahead::" + qualifier; }
};

// Shape "target_victim": fires while the bot's CURRENT TARGET is being hit by a
// BOT group member matching `role` -- "my target is already handled by a <role>".
// Built for SUPPRESS: zero "taunt spell" while a tank's add is already on ANOTHER
// tank, so the two tanks never taunt off each other during a Magnetic Pull swap (the
// boss's threat-swap re-assigns them on its own; the taunt is only meant to grab an
// add back from a DPS/healer -- where the victim is NOT a tank, so this stays
// inactive and the taunt fires). A human / non-bot victim reads as "not handled"
// (returns false) so the taunt still recovers the add. Qualifier: "role=<csv>" (same
// tokens as encounter_active, incl. `!`).
class JsonTargetVictimTrigger : public Trigger, public Qualified
{
public:
    JsonTargetVictimTrigger(PlayerbotAI* ai) : Trigger(ai, "json targetvictim") {}

    bool IsActive() override;
    std::string const getName() override { return "json targetvictim::" + qualifier; }
};

// Shape "manual_engage": the "call the pull" gate. Fires while the bot's owner
// has issued `.rjson pull` (RaidJsonMode::IsEngaged) AND `role` matches AND (if
// `add` is given) that add is alive within `range` yards. Lets the raid leader
// kick off the engage ON COMMAND instead of waiting for something to wander into
// combat — e.g. main tank on one add, off-tank on the other. The `range`
// proximity gate is the pathing trick: a bot FAR from its add stays out of the
// rule and just follows the master up the ramp (bots can't path the long
// low->high climb on their own), breaking off to engage only once led close; and
// a tank yanked off by Magnetic Pull falls out of range so the in-combat
// nearest-pet rule retargets it. Qualifier: "add=<name/entry>|role=<csv>|
// range=<yds>" (all optional).
class JsonManualEngageTrigger : public Trigger, public Qualified
{
public:
    JsonManualEngageTrigger(PlayerbotAI* ai) : Trigger(ai, "json engage") {}

    bool IsActive() override;
    std::string const getName() override { return "json engage::" + qualifier; }
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

// Internal scope wrapper (NOT authored directly — the loader wraps every rule's
// resolved trigger in this). Gates an inner trigger on its file's boss being the
// ACTIVE encounter for the bot, so one boss's rules never fire in another boss's
// room. This is the uniform fix for cross-boss contamination: it scopes BOTH the
// generic shapes and Level-1 named triggers ("has attackers", four_horsemen),
// since it wraps the resolved name regardless of what it is.
//
// Qualifier: "<boss>\x1f<inner trigger name>" (the loader builds it; \x1f because
// the inner name contains '|' '=' '::'). Delegates getHandlers/Reset/GetTarget to
// the inner so wrapping is transparent to the rest of the engine.
class JsonScopedTrigger : public Trigger, public Qualified
{
public:
    JsonScopedTrigger(PlayerbotAI* ai) : Trigger(ai, "json scoped") {}

    void Qualify(std::string const qual) override;
    bool IsActive() override;
    std::vector<NextAction> getHandlers() override;
    void Reset() override;
    Unit* GetTarget() override;
    Value<Unit*>* GetTargetValue() override;
    std::string const GetTargetName() override;
    std::string const getName() override { return "json scoped[" + _boss + "]::" + _innerName; }

private:
    Trigger* Inner();

    std::string _boss;
    std::string _innerName;
    Trigger*    _inner = nullptr;
    bool        _resolved = false;
};

#endif
