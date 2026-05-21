/*
 * JSON-driven raid strategy — generic Level-2 triggers.
 *
 * Parameters arrive via the factory "::qualifier" channel (see Qualified).
 */
#ifndef _PLAYERBOT_JSONSTRATEGYTRIGGERS_H
#define _PLAYERBOT_JSONSTRATEGYTRIGGERS_H

#include "NamedObjectContext.h"  // Qualified
#include "Trigger.h"

// Shape T7 "encounter_active": active while a creature with the given (lower-
// cased) name is on the threat list / found nearby. Mirrors the per-boss C++
// triggers (e.g. AnubrekhanTrigger uses AI_VALUE2 "find target","anub'rekhan").
// Qualifier = the boss name.
class JsonEncounterActiveTrigger : public Trigger, public Qualified
{
public:
    JsonEncounterActiveTrigger(PlayerbotAI* ai) : Trigger(ai, "json encounter") {}

    bool IsActive() override;
    std::string const getName() override { return "json encounter::" + qualifier; }
};

#endif
