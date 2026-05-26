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

// Resolve a trigger JSON object ({"name":..} or {"shape":..,..}) into the
// engine-ready factory name. Shared by rule triggers and suppress-rule triggers.
// On error, appends to `errors` and returns false (caller skips the entry).
static bool ResolveTrigger(json const& t, std::string const& fileBoss,
                           std::string const& fname, std::vector<std::string>& errors,
                           std::string& out)
{
    if (t.contains("name"))
    {
        out = t["name"].get<std::string>();
        return true;
    }
    if (!t.contains("shape"))
    {
        errors.push_back(fname + ": trigger needs 'name' or 'shape'");
        return false;
    }

    std::string shape = t["shape"].get<std::string>();
    if (shape == "encounter_active")
    {
        std::string boss = t.value("boss", fileBoss);
        if (boss.empty())
        {
            errors.push_back(fname + ": encounter_active needs a 'boss' (rule or file level)");
            return false;
        }
        std::string q = "boss=" + boss;
        std::string role = t.value("role", std::string());
        if (!role.empty())
            q += "|role=" + role;
        std::string aura = t.value("boss_aura", std::string());
        if (!aura.empty())
        {
            bool present = t.value("aura_present", true);
            bool includeCast = t.value("include_cast", true);
            q += "|aura=" + aura + "|has=" + (present ? "1" : "0") + "|cast=" + (includeCast ? "1" : "0");
        }
        out = "json encounter::" + q;
        return true;
    }
    if (shape == "pre_cast_window")
    {
        std::string boss = t.value("boss", fileBoss);
        if (boss.empty())
        {
            errors.push_back(fname + ": pre_cast_window needs a 'boss' (rule or file level)");
            return false;
        }
        uint32 interval = t.value("interval", 0u);
        if (interval == 0)
        {
            errors.push_back(fname + ": pre_cast_window needs a non-zero 'interval' (ms)");
            return false;
        }
        // Anchor spell is optional and may be a name or a numeric id.
        std::string spell;
        if (t.contains("spell"))
            spell = t["spell"].is_number_integer()
                        ? std::to_string(t["spell"].get<int>())
                        : t["spell"].get<std::string>();
        uint32 firstAt = t.value("first_at", interval);
        uint32 lead = t.value("lead", 4000u);
        uint32 tail = t.value("tail", 6000u);
        std::string q = "boss=" + boss;
        if (!spell.empty())
            q += "|spell=" + spell;
        q += "|interval=" + std::to_string(interval);
        q += "|first=" + std::to_string(firstAt);
        q += "|lead=" + std::to_string(lead);
        q += "|tail=" + std::to_string(tail);
        std::string aura = t.value("require_aura", std::string());
        if (!aura.empty())
            q += "|aura=" + aura;
        std::string role = t.value("role", std::string());
        if (!role.empty())
            q += "|role=" + role;
        out = "json precast::" + q;
        return true;
    }

    errors.push_back(fname + ": unknown trigger shape '" + shape + "'");
    return false;
}

void RaidJsonRuleSet::Load()
{
    std::vector<JsonResolvedRule> rules;
    std::vector<JsonResolvedSuppress> suppress;
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
            if (!ResolveTrigger(ruleJson["trigger"], fileBoss, fname, errors, rule.trigger))
                continue;

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
                    json const& p = a.value("params", json::object());
                    if (shape == "orbit_point")
                    {
                        float x = p.value("x", 0.0f);
                        float y = p.value("y", 0.0f);
                        float radius = p.value("radius", 40.0f);
                        uint32 segments = p.value("segments", 16u);
                        bool clockwise = p.value("clockwise", true);
                        if (segments == 0)
                            segments = 16u;
                        act.name = "json orbit::" + OrbitQualifier(x, y, radius, segments, clockwise);
                    }
                    else if (shape == "stack_point")
                    {
                        float x = p.value("x", 0.0f);
                        float y = p.value("y", 0.0f);
                        float radius = p.value("radius", 5.0f);
                        char buf[96];
                        std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f", x, y, radius);
                        act.name = std::string("json stack::") + buf;
                    }
                    else if (shape == "spread")
                    {
                        float radius = p.value("radius", 8.0f);
                        uint32 minInterval = p.value("min_interval", 3000u);
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "%.4f,%u", radius, minInterval);
                        act.name = std::string("json spread::") + buf;
                    }
                    else if (shape == "attack")
                    {
                        // params.targets: a name ("crypt guard"), an entry id
                        // (16486), or an array mixing both. Omit for a pure
                        // "attack the boss" rule.
                        std::string targets;
                        auto appendToken = [&targets](json const& el)
                        {
                            if (!targets.empty())
                                targets += ",";
                            targets += el.is_number_integer() ? std::to_string(el.get<int>())
                                                              : el.get<std::string>();
                        };
                        if (p.contains("targets"))
                        {
                            if (p["targets"].is_array())
                                for (auto const& el : p["targets"])
                                    appendToken(el);
                            else
                                appendToken(p["targets"]);
                        }
                        std::string boss = p.value("boss", fileBoss);
                        if (targets.empty() && boss.empty())
                        {
                            errors.push_back(fname + ": attack needs params.targets and/or a boss");
                            continue;
                        }
                        // detect: where to look. "threat" = the attacker/threat
                        // list (default); "nearest" = a nearby-NPC scan that sees
                        // off-threat objects.
                        std::string detect = p.value("detect", std::string("threat"));
                        if (detect != "threat" && detect != "nearest")
                        {
                            errors.push_back(fname + ": attack params.detect must be 'threat' or 'nearest'");
                            continue;
                        }
                        // select: which match to pick. Defaults to match the
                        // detection mode (nearest-scan -> nearest, threat -> lowest_hp).
                        std::string select = p.value("select",
                            std::string(detect == "nearest" ? "nearest" : "lowest_hp"));
                        if (select != "lowest_hp" && select != "nearest")
                        {
                            errors.push_back(fname + ": attack params.select must be 'lowest_hp' or 'nearest'");
                            continue;
                        }
                        std::string q = "targets=" + targets + "|boss=" + boss +
                                        "|detect=" + detect + "|select=" + select;
                        act.name = "json attackpick::" + q;
                    }
                    else if (shape == "tank_adds")
                    {
                        std::string add = p.value("add", std::string());
                        std::string boss = p.value("boss", fileBoss);
                        if (add.empty())
                        {
                            errors.push_back(fname + ": tank_adds needs params.add");
                            continue;
                        }
                        std::string q = "add=" + add;
                        if (!boss.empty())
                            q += "|boss=" + boss;
                        act.name = "json tankadds::" + q;
                    }
                    else if (shape == "timed_safe_zone")
                    {
                        // zones: array of [x,y] pairs (or a flat [x,y,x,y,...]
                        // array) -> "x1,y1,x2,y2,..."
                        std::string zones;
                        auto appendCoord = [&zones](json const& c)
                        {
                            char b[32];
                            std::snprintf(b, sizeof(b), "%.4f", c.get<float>());
                            if (!zones.empty())
                                zones += ",";
                            zones += b;
                        };
                        if (p.contains("zones") && p["zones"].is_array())
                            for (auto const& zone : p["zones"])
                            {
                                if (zone.is_array())
                                    for (auto const& c : zone)
                                        appendCoord(c);
                                else
                                    appendCoord(zone);
                            }
                        // pattern: array of ints -> "3,2,1,0,1,2"
                        std::string pattern;
                        if (p.contains("pattern") && p["pattern"].is_array())
                            for (auto const& el : p["pattern"])
                            {
                                if (!pattern.empty())
                                    pattern += ",";
                                pattern += std::to_string(el.get<int>());
                            }
                        uint32 interval = p.value("interval", 0u);
                        if (zones.empty() || pattern.empty() || interval == 0)
                        {
                            errors.push_back(fname + ": timed_safe_zone needs 'zones', 'pattern' and a non-zero 'interval'");
                            continue;
                        }
                        float z = p.value("z", 0.0f);
                        uint32 firstAt = p.value("first_at", interval);
                        bool hold = p.value("hold", false);
                        bool castWhileMoving = p.value("cast_while_moving", false);
                        float tol = p.value("tolerance", 5.0f);
                        char tail[96];
                        std::snprintf(tail, sizeof(tail), "|z=%.4f|first=%u|interval=%u|hold=%d|tol=%.4f|cw=%d",
                                      z, firstAt, interval, hold ? 1 : 0, tol, castWhileMoving ? 1 : 0);
                        act.name = "json safezone::zones=" + zones + "|pattern=" + pattern + tail;
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

        // ---- suppress (data-driven multipliers) ----
        // Optional top-level array: each entry zeroes the relevance of the named
        // actions while its trigger is active (the JSON analog of a C++
        // Strategy::InitMultipliers entry). Lets a movement shape yield the tick
        // for instant casts without the eruption-dodge / reach / formation actions
        // grabbing it and pulling the bot off-route.
        if (j.contains("suppress") && j["suppress"].is_array())
        {
            for (auto const& sJson : j["suppress"])
            {
                if (!sJson.contains("trigger"))
                {
                    errors.push_back(fname + ": suppress entry without 'trigger'");
                    continue;
                }
                JsonResolvedSuppress sup;
                if (!ResolveTrigger(sJson["trigger"], fileBoss, fname, errors, sup.trigger))
                    continue;
                if (!sJson.contains("actions") || !sJson["actions"].is_array())
                {
                    errors.push_back(fname + ": suppress entry without 'actions' array");
                    continue;
                }
                for (auto const& a : sJson["actions"])
                    if (a.is_string())
                        sup.names.insert(a.get<std::string>());
                if (!sup.names.empty())
                    suppress.push_back(std::move(sup));
            }
        }
    }

    _rules = std::move(rules);
    _suppress = std::move(suppress);
    _errors = std::move(errors);
    _fileCount = fileCount;
    _loadedOnce = true;

    LOG_INFO("server.loading", "[RaidJson] loaded {} rule(s), {} suppress rule(s) from {} file(s) in {} ({} error(s))",
             (uint32)_rules.size(), (uint32)_suppress.size(), _fileCount, _sourceDir, (uint32)_errors.size());
}
