#include "AutopilotMoveAction.h"

#include "AutopilotControl.h"
#include "Playerbots.h"

bool AutopilotMoveAction::Execute(Event /*event*/)
{
    uint32 mapId = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (!AutopilotControl::GetTarget(bot->GetGUID(), mapId, x, y, z))
        return false;
    if (bot->GetMapId() != mapId)
        return false;

    return MoveTo(mapId, x, y, z);
}

bool AutopilotMoveAction::isUseful()
{
    uint32 mapId = 0;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (!AutopilotControl::GetTarget(bot->GetGUID(), mapId, x, y, z))
        return false;
    if (bot->GetMapId() != mapId)
        return false;

    return bot->GetExactDist(x, y, z) > 3.0f;
}
