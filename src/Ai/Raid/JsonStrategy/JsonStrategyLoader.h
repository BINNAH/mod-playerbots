/*
 * JSON-driven raid strategy — experimental, opt-in.
 *
 * In-memory rule set parsed from JSON files on disk. Holds rules already
 * resolved to engine-ready names: each trigger/action name is the exact string
 * the NamedObject factories expect, with any Level-2 parameters encoded in the
 * "::qualifier" suffix (see JsonStrategyActions / JsonStrategyTriggers).
 *
 * Single source of truth read by JsonRaidStrategy::InitTriggers. Touched only
 * on the world thread (bot AI updates + chat commands both run there), so no
 * locking is needed. Reload re-reads disk into this set, then the .rjson
 * command forces an engine re-init on active bots.
 */
#ifndef _PLAYERBOT_JSONSTRATEGYLOADER_H
#define _PLAYERBOT_JSONSTRATEGYLOADER_H

#include "Define.h"  // uint32
#include "ObjectGuid.h"

#include <set>
#include <string>
#include <vector>

// Session-scoped record of which bots are toggled into json-raid mode (`.rjson
// on`). Source of truth for json-raid EXCLUSIVITY: while a bot is in this set,
// `PlayerbotAI::ApplyInstanceStrategies` must NOT (re-)attach the C++ instance
// strategy on top of json-raid. The core re-runs ApplyInstanceStrategies on its
// own (worldport ACK, ResetStrategies), which would otherwise silently re-add
// e.g. "naxx" alongside json-raid — making it ambiguous which strategy actually
// runs the fight. Toggled by the `.rjson on`/`off` commands. In-memory only
// (cleared on worldserver restart — re-run `.rjson on` next session).
class RaidJsonMode
{
public:
    static RaidJsonMode& instance();

    void Set(ObjectGuid bot, bool on);
    bool IsActive(ObjectGuid bot) const;

private:
    RaidJsonMode() = default;
    std::set<ObjectGuid> _bots;
};

// One action under a trigger. `name` is the fully-resolved factory name
// (possibly "base::qualifier"); `priority` is the offset added to ACTION_RAID.
struct JsonResolvedAction
{
    std::string name;
    float priority = 1.0f;
};

// One trigger -> ordered list of actions, mirroring a C++ TriggerNode.
struct JsonResolvedRule
{
    std::string trigger;
    std::vector<JsonResolvedAction> actions;
};

// A data-driven multiplier: while `trigger` is active for a bot, the listed
// action names have their relevance zeroed. This is the JSON analog of a C++
// Strategy::InitMultipliers entry (e.g. HeiganDanceMultiplier suppressing
// "avoid aoe"); json-raid carries none otherwise. `trigger` is the resolved
// factory name; `names` are matched against each action's getName().
struct JsonResolvedSuppress
{
    std::string trigger;
    std::set<std::string> names;
};

class RaidJsonRuleSet
{
public:
    static RaidJsonRuleSet& instance();

    // Re-read every *.json under the resolved directory and rebuild the rule
    // set. Always replaces the previous set wholesale (even on partial errors,
    // valid files still load). Safe to call repeatedly (live reload).
    void Load();

    std::vector<JsonResolvedRule> const& Rules() const { return _rules; }
    std::vector<JsonResolvedSuppress> const& SuppressRules() const { return _suppress; }

    // --- status / diagnostics (populated by Load) ---
    std::string const& SourceDir() const { return _sourceDir; }
    uint32 FileCount() const { return _fileCount; }
    uint32 RuleCount() const { return (uint32)_rules.size(); }
    std::vector<std::string> const& Errors() const { return _errors; }
    bool Loaded() const { return _loadedOnce; }

private:
    RaidJsonRuleSet() = default;

    std::string ResolveDir() const;

    std::vector<JsonResolvedRule> _rules;
    std::vector<JsonResolvedSuppress> _suppress;
    std::string _sourceDir;
    uint32 _fileCount = 0;
    std::vector<std::string> _errors;
    bool _loadedOnce = false;
};

#endif
