#include "JsonStrategyTriggers.h"

#include "Playerbots.h"

bool JsonEncounterActiveTrigger::IsActive()
{
    if (qualifier.empty())
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", qualifier);
    return boss != nullptr;
}
