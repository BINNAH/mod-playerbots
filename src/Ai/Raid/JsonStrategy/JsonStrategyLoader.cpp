#include "JsonStrategyLoader.h"

#include "Config.h"
#include "Log.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

RaidJsonRuleSet& RaidJsonRuleSet::instance()
{
    static RaidJsonRuleSet inst;
    return inst;
}

std::string RaidJsonRuleSet::ResolveDir() const
{
    // Resolved relative to the worldserver working directory (the `server/`
    // folder), matching the mod-ollama-chat RAG-path precedent. Override with
    // `RaidJson.Dir` in any loaded .conf.
    std::string dir = sConfigMgr->GetOption<std::string>("RaidJson.Dir", "raid_strategies/");
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir += '/';
    return dir;
}

// Encode the orbit shape's tunables into the "::qualifier" the factory hands to
// JsonOrbitPointAction::Qualify. Fixed-precision so identical params always
// produce the same cache key.
static std::string OrbitQualifier(float x, float y, float radius, uint32 segments, bool clockwise)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%u,%d", x, y, radius, segments, clockwise ? 1 : 0);
    return std::string(buf);
}

void RaidJsonRuleSet::Load()
{
    std::vector<JsonResolvedRule> rules;
    std::vector<std::string> errors;
    uint32 fileCount = 0;

    std::string dir = ResolveDir();
    if (!fs::exists(dir))
    {
        // Convenience fallback so a first run finds files dropped next to the
        // module's data dir layout.
        std::string fallback = "data/raid_strategies/";
        if (fs::exists(fallback))
            dir = fallback;
    }
    _sourceDir = fs::absolute(fs::path(dir)).string();

    if (!fs::exists(dir))
    {
        errors.push_back("directory not found: " + _sourceDir);
        LOG_ERROR("server.loading", "[RaidJson] directory not found: {}", _sourceDir);
        _rules = std::move(rules);
        _errors = std::move(errors);
        _fileCount = 0;
        _loadedOnce = true;
        return;
    }

    for (auto const& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;

        ++fileCount;
        std::string fname = entry.path().filename().string();

        std::ifstream in(entry.path());
        if (!in)
        {
            errors.push_back(fname + ": cannot open");
            continue;
        }

        json j;
        try
        {
            in >> j;
        }
        catch (std::exception const& e)
        {
            errors.push_back(fname + ": parse error: " + e.what());
            LOG_ERROR("server.loading", "[RaidJson] {}: parse error: {}", fname, e.what());
            continue;
        }

        std::string fileBoss = j.value("boss", std::string());

        if (!j.contains("rules") || !j["rules"].is_array())
        {
            errors.push_back(fname + ": missing 'rules' array");
            continue;
        }

        for (auto const& ruleJson : j["rules"])
        {
            JsonResolvedRule rule;

            // ---- trigger ----
            if (!ruleJson.contains("trigger"))
            {
                errors.push_back(fname + ": rule without 'trigger'");
                continue;
            }
            json const& t = ruleJson["trigger"];
            if (t.contains("name"))
            {
                rule.trigger = t["name"].get<std::string>();
            }
            else if (t.contains("shape"))
            {
                std::string shape = t["shape"].get<std::string>();
                if (shape == "encounter_active")
                {
                    std::string boss = t.value("boss", fileBoss);
                    if (boss.empty())
                    {
                        errors.push_back(fname + ": encounter_active needs a 'boss' (rule or file level)");
                        continue;
                    }
                    rule.trigger = "json encounter::" + boss;
                }
                else
                {
                    errors.push_back(fname + ": unknown trigger shape '" + shape + "'");
                    continue;
                }
            }
            else
            {
                errors.push_back(fname + ": trigger needs 'name' or 'shape'");
                continue;
            }

            // ---- actions ----
            if (!ruleJson.contains("actions") || !ruleJson["actions"].is_array())
            {
                errors.push_back(fname + ": rule without 'actions' array");
                continue;
            }

            for (auto const& a : ruleJson["actions"])
            {
                JsonResolvedAction act;
                act.priority = a.value("priority", 1.0f);

                if (a.contains("name"))
                {
                    act.name = a["name"].get<std::string>();
                }
                else if (a.contains("shape"))
                {
                    std::string shape = a["shape"].get<std::string>();
                    if (shape == "orbit_point")
                    {
                        json const& p = a.value("params", json::object());
                        float x = p.value("x", 0.0f);
                        float y = p.value("y", 0.0f);
                        float radius = p.value("radius", 40.0f);
                        uint32 segments = p.value("segments", 16u);
                        bool clockwise = p.value("clockwise", true);
                        if (segments == 0)
                            segments = 16u;
                        act.name = "json orbit::" + OrbitQualifier(x, y, radius, segments, clockwise);
                    }
                    else
                    {
                        errors.push_back(fname + ": unknown action shape '" + shape + "'");
                        continue;
                    }
                }
                else
                {
                    errors.push_back(fname + ": action needs 'name' or 'shape'");
                    continue;
                }

                rule.actions.push_back(std::move(act));
            }

            if (!rule.actions.empty())
                rules.push_back(std::move(rule));
        }
    }

    _rules = std::move(rules);
    _errors = std::move(errors);
    _fileCount = fileCount;
    _loadedOnce = true;

    LOG_INFO("server.loading", "[RaidJson] loaded {} rule(s) from {} file(s) in {} ({} error(s))",
             (uint32)_rules.size(), _fileCount, _sourceDir, (uint32)_errors.size());
}
