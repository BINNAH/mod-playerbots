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

#endif
