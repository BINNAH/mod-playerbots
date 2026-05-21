#include "JsonStrategyActions.h"

#include <cmath>
#include <cstdio>

void JsonOrbitPointAction::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _valid = false;

    float x = 0.0f, y = 0.0f, r = 40.0f;
    unsigned int segments = 16;
    int cw = 1;
    int parsed = std::sscanf(qual.c_str(), "%f,%f,%f,%u,%d", &x, &y, &r, &segments, &cw);
    if (parsed < 4 || segments == 0)
        return;

    center_x = x;
    center_y = y;
    radius = r;
    intervals = segments;
    clockwise = (cw != 0);

    // Rebuild the ring exactly as the base constructor would (start_angle 0).
    waypoints.clear();
    for (uint32 i = 0; i < intervals; ++i)
    {
        float angle = 2.0f * (float)M_PI * i / intervals;
        waypoints.push_back(std::make_pair(center_x + std::cos(angle) * radius,
                                           center_y + std::sin(angle) * radius));
    }
    call_counters = 0;
    _valid = true;
}

uint32 JsonOrbitPointAction::GetCurrWaypoint()
{
    // Head to the next point along the ring so the bot actually orbits; honor
    // the clockwise flag by stepping the other way.
    uint32 nearest = FindNearestWaypoint();
    uint32 step = clockwise ? 1u : (intervals - 1u);
    return (nearest + step) % intervals;
}
