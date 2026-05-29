#include "JsonStrategyLoader.h"

#include "Config.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "json.hpp"

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

namespace fs = std::filesystem;
using json = nlohmann::json;

RaidJsonRuleSet& RaidJsonRuleSet::instance()
{
    static RaidJsonRuleSet inst;
    return inst;
}

RaidJsonMode& RaidJsonMode::instance()
{
    static RaidJsonMode inst;
    return inst;
}

void RaidJsonMode::Set(ObjectGuid bot, bool on)
{
    if (on)
        _bots.insert(bot);
    else
        _bots.erase(bot);
}

bool RaidJsonMode::IsActive(ObjectGuid bot) const
{
    return _bots.find(bot) != _bots.end();
}

void RaidJsonMode::SetEngaged(ObjectGuid bot, bool on, std::string const& boss)
{
    if (on)
        _engaged[bot] = boss;
    else
        _engaged.erase(bot);
}

bool RaidJsonMode::IsEngaged(ObjectGuid bot) const
{
    return _engaged.find(bot) != _engaged.end();
}

std::string RaidJsonMode::EngagedBoss(ObjectGuid bot) const
{
    auto it = _engaged.find(bot);
    return it != _engaged.end() ? it->second : std::string();
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
static std::string OrbitQualifier(float x, float y, float radius, uint32 segments, bool clockwise,
                                  uint32 intervalMs)
{
    char buf[192];
    std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%u,%d,%u", x, y, radius, segments, clockwise ? 1 : 0,
                  intervalMs);
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
        std::string klass = t.value("class", std::string());
        if (!klass.empty())
            q += "|class=" + klass;
        // detect: how to locate the boss. "threat" (default) = find-target /
        // threat list; "nearest" = a proximity scan that works threat-independently
        // (for kiters / off-tanks who never threaten the boss).
        std::string detect = t.value("detect", std::string());
        if (!detect.empty())
        {
            if (detect != "threat" && detect != "nearest")
            {
                errors.push_back(fname + ": encounter_active 'detect' must be 'threat' or 'nearest'");
                return false;
            }
            q += "|detect=" + detect;
        }
        std::string aura = t.value("boss_aura", std::string());
        if (!aura.empty())
        {
            bool present = t.value("aura_present", true);
            bool includeCast = t.value("include_cast", true);
            q += "|aura=" + aura + "|has=" + (present ? "1" : "0") + "|cast=" + (includeCast ? "1" : "0");
        }
        // Self-aura phase gate: an aura on the BOT (e.g. "mutating injection").
        std::string selfAura = t.value("self_aura", std::string());
        if (!selfAura.empty())
        {
            bool selfPresent = t.value("self_aura_present", true);
            q += "|self=" + selfAura + "|selfhas=" + (selfPresent ? "1" : "0");
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
    if (shape == "adds_near")
    {
        // add: a creature name, an entry id, or an array mixing both. An array is
        // joined with commas here; the trigger counts a unit matching ANY token
        // (the union -- e.g. ["stalagg","feugen"] = "either Thaddius add alive").
        std::string add;
        auto appendAdd = [&add](json const& el)
        {
            if (!add.empty())
                add += ",";
            add += el.is_number_integer() ? std::to_string(el.get<int>())
                                          : el.get<std::string>();
        };
        if (t.contains("add"))
        {
            if (t["add"].is_array())
                for (auto const& el : t["add"])
                    appendAdd(el);
            else
                appendAdd(t["add"]);
        }
        if (add.empty())
        {
            errors.push_back(fname + ": adds_near needs an 'add' (name or entry id)");
            return false;
        }
        float range = t.value("range", 0.0f);
        uint32 count = t.value("count", 1u);
        std::string of = t.value("of", std::string("self"));
        if (of != "self" && of != "boss")
        {
            errors.push_back(fname + ": adds_near 'of' must be 'self' or 'boss'");
            return false;
        }
        char rbuf[32];
        std::snprintf(rbuf, sizeof(rbuf), "%.4f", range);
        std::string q = "add=" + add + "|range=" + rbuf + "|count=" + std::to_string(count) + "|of=" + of;
        std::string role = t.value("role", std::string());
        if (!role.empty())
            q += "|role=" + role;
        std::string klass = t.value("class", std::string());
        if (!klass.empty())
            q += "|class=" + klass;
        if (of == "boss")
        {
            std::string boss = t.value("boss", fileBoss);
            if (boss.empty())
            {
                errors.push_back(fname + ": adds_near of=boss needs a 'boss' (rule or file level)");
                return false;
            }
            q += "|boss=" + boss;
        }
        out = "json addsnear::" + q;
        return true;
    }
    if (shape == "target_hp_ahead")
    {
        // others: a creature name ("feugen"), an entry id, or an array mixing
        // both — the adds to compare the bot's current target against. Required.
        std::string others;
        auto appendToken = [&others](json const& el)
        {
            if (!others.empty())
                others += ",";
            others += el.is_number_integer() ? std::to_string(el.get<int>())
                                              : el.get<std::string>();
        };
        if (t.contains("others"))
        {
            if (t["others"].is_array())
                for (auto const& el : t["others"])
                    appendToken(el);
            else
                appendToken(t["others"]);
        }
        if (others.empty())
        {
            errors.push_back(fname + ": target_hp_ahead needs 'others' (name/entry or array)");
            return false;
        }
        char buf[32];
        std::string q = "others=" + others;
        std::snprintf(buf, sizeof(buf), "%.4f", t.value("margin", 0.0f));
        q += std::string("|margin=") + buf;
        std::snprintf(buf, sizeof(buf), "%.4f", t.value("below", 100.0f));
        q += std::string("|below=") + buf;
        out = "json hpahead::" + q;
        return true;
    }
    if (shape == "target_victim")
    {
        // role: the victim role to match (default "tank"). Fires while the bot's
        // current target is being hit by a BOT of that role -- used in suppress to
        // stop tanks taunting an add another tank already holds.
        std::string role = t.value("role", std::string("tank"));
        out = "json targetvictim::role=" + role;
        return true;
    }
    if (shape == "manual_engage")
    {
        // Both optional: `add` (name/entry) gates on that add being alive nearby
        // so the rule stops once it dies; `role` assigns MT vs OT to their adds.
        std::string q;
        std::string add;
        if (t.contains("add"))
            add = t["add"].is_number_integer() ? std::to_string(t["add"].get<int>())
                                               : t["add"].get<std::string>();
        if (!add.empty())
            q += "add=" + add;
        std::string role = t.value("role", std::string());
        if (!role.empty())
            q += (q.empty() ? "" : "|") + std::string("role=") + role;
        float range = t.value("range", 0.0f);
        if (range > 0.0f)
        {
            char rbuf[48];
            std::snprintf(rbuf, sizeof(rbuf), "%srange=%.4f", q.empty() ? "" : "|", range);
            q += rbuf;
        }
        // Optional "i/n": split the bots matching `role` into n groups and fire only
        // for the i-th -- sends one role to two adds (e.g. healers half-left/half-right).
        std::string split = t.value("split", std::string());
        if (!split.empty())
            q += (q.empty() ? "" : "|") + std::string("split=") + split;
        out = "json engage::" + q;
        return true;
    }
    if (shape == "is_alive")
    {
        // targets: creature name(s), matched by NAME against the bot's threat list
        // ("find target", threat-based -- no LOS, no range, never flickers). An
        // array is joined with commas. This is the flicker-free phase gate that
        // adds_near (sight+LOS proximity) is NOT. Entry ids aren't supported (find
        // target matches names) -- use a name.
        std::string targets;
        auto appendTarget = [&targets](json const& el)
        {
            if (!targets.empty())
                targets += ",";
            targets += el.is_number_integer() ? std::to_string(el.get<int>())
                                              : el.get<std::string>();
        };
        if (t.contains("targets"))
        {
            if (t["targets"].is_array())
                for (auto const& el : t["targets"])
                    appendTarget(el);
            else
                appendTarget(t["targets"]);
        }
        if (targets.empty())
        {
            errors.push_back(fname + ": is_alive needs 'targets' (name or array of names)");
            return false;
        }
        bool present = t.value("present", true);
        std::string match = t.value("match", std::string("any"));
        if (match != "any" && match != "all")
        {
            errors.push_back(fname + ": is_alive 'match' must be 'any' or 'all'");
            return false;
        }
        std::string q = "targets=" + targets + "|present=" + (present ? "1" : "0") + "|match=" + match;
        std::string role = t.value("role", std::string());
        if (!role.empty())
            q += "|role=" + role;
        out = "json isalive::" + q;
        return true;
    }

    errors.push_back(fname + ": unknown trigger shape '" + shape + "'");
    return false;
}

void RaidJsonRuleSet::Load()
{
    std::vector<JsonResolvedRule> rules;
    std::vector<JsonResolvedSuppress> suppress;
    std::set<std::string> bosses;
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
        _bosses.clear();
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
        if (!fileBoss.empty())
            bosses.insert(ToLower(fileBoss));  // a `.rjson pull <boss>` target

        // Part A — per-file boss scope. Wrap a resolved trigger name so it only
        // fires while this file's boss is the ACTIVE encounter for the bot (see
        // JsonScopedTrigger). This is what stops one boss's rules -- especially
        // boss-agnostic ones like manual_engage (`.rjson pull`) and Level-1 named
        // gates ("has attackers") -- from firing in another boss's room. The
        // \x1f (unit separator) splits <boss> from the inner name, which itself
        // contains '|' '=' '::' and so can't share their delimiters. Files with
        // no `boss` stay unwrapped (can't be scoped).
        auto wrapScope = [&fileBoss](std::string const& inner) -> std::string
        {
            if (fileBoss.empty())
                return inner;
            return "json scoped::" + fileBoss + '\x1f' + inner;
        };

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
            rule.trigger = wrapScope(rule.trigger);

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
                        // Optional: 0/absent = continuous orbit; >0 = step one
                        // waypoint per `interval` ms (cadence-paced kite).
                        uint32 interval = p.value("interval", 0u);
                        if (segments == 0)
                            segments = 16u;
                        act.name = "json orbit::" + OrbitQualifier(x, y, radius, segments, clockwise, interval);
                    }
                    else if (shape == "stack_point")
                    {
                        float x = p.value("x", 0.0f);
                        float y = p.value("y", 0.0f);
                        float radius = p.value("radius", 5.0f);
                        bool hold = p.value("hold", false);
                        char buf[128];
                        // Optional z: an elevated anchor height (e.g. Thaddius's add
                        // platforms) -> a 3D move so the bot climbs instead of
                        // yielding on the floor below. Omitted = ground stack (the
                        // 4-field qualifier stays byte-stable with existing files).
                        if (p.contains("z"))
                            std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%d,%.4f", x, y, radius,
                                          hold ? 1 : 0, p.value("z", 0.0f));
                        else
                            std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%d", x, y, radius, hold ? 1 : 0);
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
                        // Optional candidate filters (omitted when 0 so existing
                        // qualifiers stay byte-stable): only adds at/below a HP%
                        // and/or within a range. "Burst the 5% Decimate chow in
                        // range, else fall back to the boss" becomes pure data.
                        float maxHp = p.value("max_hp_pct", 0.0f);
                        float maxRange = p.value("max_range", 0.0f);
                        char fbuf[48];
                        if (maxHp > 0.0f)
                        {
                            std::snprintf(fbuf, sizeof(fbuf), "|maxhp=%.4f", maxHp);
                            q += fbuf;
                        }
                        if (maxRange > 0.0f)
                        {
                            std::snprintf(fbuf, sizeof(fbuf), "|maxrange=%.4f", maxRange);
                            q += fbuf;
                        }
                        // sticky: default true keeps the bot on its current match to
                        // avoid caster nuke-cancel thrash; sticky:false re-picks the
                        // best every tick (a tank swapping to the now-nearest add
                        // after Magnetic Pull). Appended only when false (byte-stable).
                        if (!p.value("sticky", true))
                            q += "|sticky=0";
                        act.name = "json attackpick::" + q;
                    }
                    else if (shape == "move_to_target")
                    {
                        // params.target: a name ("stalagg"), an entry id, or an
                        // array. The nearest live match is approached; falls back
                        // to `boss` when none are alive.
                        auto buildSet = [](json const& node) -> std::string
                        {
                            std::string out;
                            auto appendToken = [&out](json const& el)
                            {
                                if (!out.empty())
                                    out += ",";
                                out += el.is_number_integer() ? std::to_string(el.get<int>())
                                                              : el.get<std::string>();
                            };
                            if (node.is_array())
                                for (auto const& el : node)
                                    appendToken(el);
                            else
                                appendToken(node);
                            return out;
                        };
                        std::string targets = p.contains("target") ? buildSet(p["target"]) : "";
                        // Optional swap-recovery set: after the bot first reaches its add,
                        // it re-paths to the NEAREST of `then` (Thaddius tank Magnetic-Pull
                        // swap). Empty = latch stays permanent in combat (old behavior).
                        std::string thenSet = p.contains("then") ? buildSet(p["then"]) : "";
                        std::string boss = p.value("boss", fileBoss);
                        if (targets.empty() && boss.empty())
                        {
                            errors.push_back(fname + ": move_to_target needs params.target and/or a boss");
                            continue;
                        }
                        // detect DEFAULTS to nearest here (a proximity scan), unlike
                        // attack -- the point is to path to something the bot does
                        // not threaten yet (the `.rjson pull`).
                        std::string detect = p.value("detect", std::string("nearest"));
                        if (detect != "threat" && detect != "nearest")
                        {
                            errors.push_back(fname + ": move_to_target params.detect must be 'threat' or 'nearest'");
                            continue;
                        }
                        float distance = p.value("distance", 0.0f);
                        char buf[48];
                        std::snprintf(buf, sizeof(buf), "|distance=%.4f", distance);
                        std::string q = "target=" + targets + "|detect=" + detect + buf;
                        if (!boss.empty())
                            q += "|boss=" + boss;
                        if (!thenSet.empty())
                            q += "|then=" + thenSet;
                        act.name = "json movetotarget::" + q;
                    }
                    else if (shape == "snare_area")
                    {
                        // spells: ordered array of spell names; each bot fires the
                        // first it knows + has off cooldown (self-gating, so one
                        // rule serves every class).
                        std::string spells;
                        if (p.contains("spells") && p["spells"].is_array())
                            for (auto const& el : p["spells"])
                            {
                                if (!spells.empty())
                                    spells += ",";
                                spells += el.get<std::string>();
                            }
                        if (spells.empty())
                        {
                            errors.push_back(fname + ": snare_area needs a non-empty params.spells array");
                            continue;
                        }
                        std::string add;
                        if (p.contains("add"))
                            add = p["add"].is_number_integer() ? std::to_string(p["add"].get<int>())
                                                               : p["add"].get<std::string>();
                        if (add.empty())
                        {
                            errors.push_back(fname + ": snare_area needs params.add (the targeting / in-range gate)");
                            continue;
                        }
                        std::string target = p.value("target", std::string("self"));
                        if (target != "self" && target != "nearest" && target != "leak")
                        {
                            errors.push_back(fname + ": snare_area params.target must be 'self', 'nearest' or 'leak'");
                            continue;
                        }
                        float range = p.value("range", 0.0f);
                        char rbuf[32];
                        std::snprintf(rbuf, sizeof(rbuf), "%.4f", range);
                        std::string q = "spells=" + spells + "|add=" + add +
                                        "|target=" + target + "|range=" + rbuf;
                        std::string boss = p.value("boss", fileBoss);
                        if (!boss.empty())
                            q += "|boss=" + boss;
                        act.name = "json snarearea::" + q;
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
                    else if (shape == "tank_swap")
                    {
                        std::string aura = p.value("aura", std::string());
                        if (aura.empty())
                        {
                            errors.push_back(fname + ": tank_swap needs params.aura (the stacking debuff name)");
                            continue;
                        }
                        uint32 stacks = p.value("stacks", 1u);
                        std::string boss = p.value("boss", fileBoss);
                        std::string detect = p.value("detect", std::string("threat"));
                        if (detect != "threat" && detect != "nearest")
                        {
                            errors.push_back(fname + ": tank_swap params.detect must be 'threat' or 'nearest'");
                            continue;
                        }
                        std::string watch = p.value("watch", std::string("victim"));
                        if (watch != "victim" && watch != "maintank")
                        {
                            errors.push_back(fname + ": tank_swap params.watch must be 'victim' or 'maintank'");
                            continue;
                        }
                        std::string q = "aura=" + aura + "|stacks=" + std::to_string(stacks) +
                                        "|boss=" + boss + "|detect=" + detect + "|watch=" + watch;
                        act.name = "json tankswap::" + q;
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
                    else if (shape == "position_vs_boss")
                    {
                        std::string anchor = p.value("anchor", std::string("radial_out"));
                        if (anchor != "radial_out" && anchor != "behind" && anchor != "front" &&
                            anchor != "left" && anchor != "right")
                        {
                            errors.push_back(fname + ": position_vs_boss params.anchor must be "
                                                     "radial_out|behind|front|left|right");
                            continue;
                        }
                        float distance = p.value("distance", 0.0f);
                        if (distance <= 0.0f)
                        {
                            errors.push_back(fname + ": position_vs_boss needs a positive params.distance");
                            continue;
                        }
                        float angleOffset = p.value("angle_offset", 0.0f);
                        bool onlyIfCloser = p.value("only_if_closer", false);
                        std::string boss = p.value("boss", fileBoss);
                        char buf[80];
                        std::snprintf(buf, sizeof(buf), "|distance=%.4f|angle=%.4f|closer=%d",
                                      distance, angleOffset, onlyIfCloser ? 1 : 0);
                        std::string q = "anchor=" + anchor + buf;
                        if (!boss.empty())
                            q += "|boss=" + boss;
                        act.name = "json posboss::" + q;
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
                sup.trigger = wrapScope(sup.trigger);
                if (!sJson.contains("actions") || !sJson["actions"].is_array())
                {
                    errors.push_back(fname + ": suppress entry without 'actions' array");
                    continue;
                }
                for (auto const& a : sJson["actions"])
                    if (a.is_string())
                        sup.names.insert(a.get<std::string>());
                // Optional grace window: skip suppression until the trigger has been
                // continuously active for this long. Accepts `after_ms` (preferred,
                // exact) or `after_seconds` (convenience).
                sup.minAgeMs = sJson.value("after_ms", 0u);
                if (sup.minAgeMs == 0 && sJson.contains("after_seconds"))
                    sup.minAgeMs = sJson.value("after_seconds", 0u) * 1000u;
                if (!sup.names.empty())
                    suppress.push_back(std::move(sup));
            }
        }
    }

    _rules = std::move(rules);
    _suppress = std::move(suppress);
    _bosses = std::move(bosses);
    _errors = std::move(errors);
    _fileCount = fileCount;
    _loadedOnce = true;

    LOG_INFO("server.loading", "[RaidJson] loaded {} rule(s), {} suppress rule(s) from {} file(s) in {} ({} error(s))",
             (uint32)_rules.size(), (uint32)_suppress.size(), _fileCount, _sourceDir, (uint32)_errors.size());
}
