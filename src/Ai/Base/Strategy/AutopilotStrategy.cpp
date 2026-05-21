#include "AutopilotStrategy.h"

#include "Playerbots.h"

std::vector<NextAction> AutopilotStrategy::getDefaultActions()
{
    return { NextAction("autopilot move", 1.0f) };
}
