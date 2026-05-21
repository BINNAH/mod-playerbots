#include "AutopilotControl.h"

#include <mutex>
#include <unordered_map>

namespace
{
    struct AutopilotTarget
    {
        uint32 mapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    std::unordered_map<ObjectGuid, AutopilotTarget> g_targets;
    std::mutex g_mutex; // bot AI and the run controller may tick on different map threads
}

namespace AutopilotControl
{
    void SetTarget(ObjectGuid bot, uint32 mapId, float x, float y, float z)
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        g_targets[bot] = AutopilotTarget{ mapId, x, y, z };
    }

    void ClearTarget(ObjectGuid bot)
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        g_targets.erase(bot);
    }

    bool GetTarget(ObjectGuid bot, uint32& mapId, float& x, float& y, float& z)
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        auto it = g_targets.find(bot);
        if (it == g_targets.end())
            return false;

        mapId = it->second.mapId;
        x = it->second.x;
        y = it->second.y;
        z = it->second.z;
        return true;
    }
}
