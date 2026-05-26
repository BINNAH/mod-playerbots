/*
 * JSON-driven raid strategy — factory contexts.
 *
 * Registers the data-driven strategy ("json-raid") plus the generic Level-2
 * shape names ("json orbit", "json encounter") so the engine can resolve them
 * by name. Added to the shared context lists in BuildShared*Contexts.cpp.
 */
#ifndef _PLAYERBOT_JSONSTRATEGYCONTEXTS_H
#define _PLAYERBOT_JSONSTRATEGYCONTEXTS_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "Strategy.h"
#include "Trigger.h"

#include "JsonRaidStrategy.h"
#include "JsonStrategyActions.h"
#include "JsonStrategyTriggers.h"

class JsonStrategyContext : public NamedObjectContext<Strategy>
{
public:
    JsonStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["json-raid"] = &JsonStrategyContext::json_raid;
    }

private:
    static Strategy* json_raid(PlayerbotAI* ai) { return new JsonRaidStrategy(ai); }
};

class JsonStrategyActionContext : public NamedObjectContext<Action>
{
public:
    JsonStrategyActionContext()
    {
        creators["json orbit"] = &JsonStrategyActionContext::json_orbit;
        creators["json stack"] = &JsonStrategyActionContext::json_stack;
        creators["json spread"] = &JsonStrategyActionContext::json_spread;
        creators["json attackpick"] = &JsonStrategyActionContext::json_attackpick;
        creators["json tankadds"] = &JsonStrategyActionContext::json_tankadds;
        creators["json safezone"] = &JsonStrategyActionContext::json_safezone;
    }

private:
    static Action* json_orbit(PlayerbotAI* ai) { return new JsonOrbitPointAction(ai); }
    static Action* json_stack(PlayerbotAI* ai) { return new JsonStackPointAction(ai); }
    static Action* json_spread(PlayerbotAI* ai) { return new JsonSpreadAction(ai); }
    static Action* json_attackpick(PlayerbotAI* ai) { return new JsonAttackAction(ai); }
    static Action* json_tankadds(PlayerbotAI* ai) { return new JsonTankAddsAction(ai); }
    static Action* json_safezone(PlayerbotAI* ai) { return new JsonTimedSafeZoneAction(ai); }
};

class JsonStrategyTriggerContext : public NamedObjectContext<Trigger>
{
public:
    JsonStrategyTriggerContext()
    {
        creators["json encounter"] = &JsonStrategyTriggerContext::json_encounter;
        creators["json precast"] = &JsonStrategyTriggerContext::json_precast;
    }

private:
    static Trigger* json_encounter(PlayerbotAI* ai) { return new JsonEncounterActiveTrigger(ai); }
    static Trigger* json_precast(PlayerbotAI* ai) { return new JsonPreCastWindowTrigger(ai); }
};

#endif
