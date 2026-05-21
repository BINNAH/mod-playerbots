/*
 * Cross-module control seam for the "Autopilot" solo-raid run system that lives
 * in mod-solo-raid-progression. That module sets a per-bot target waypoint here;
 * AutopilotMoveAction (a non-combat movement behavior) reads it and walks the
 * bot toward it. Kept deliberately tiny (POD + ObjectGuid) so the other module
 * can include it cheaply.
 */

#ifndef _PLAYERBOT_AUTOPILOTCONTROL_H
#define _PLAYERBOT_AUTOPILOTCONTROL_H

#include "Define.h"
#include "ObjectGuid.h"

namespace AutopilotControl
{
    void SetTarget(ObjectGuid bot, uint32 mapId, float x, float y, float z);
    void ClearTarget(ObjectGuid bot);
    bool GetTarget(ObjectGuid bot, uint32& mapId, float& x, float& y, float& z);
}

#endif // _PLAYERBOT_AUTOPILOTCONTROL_H
