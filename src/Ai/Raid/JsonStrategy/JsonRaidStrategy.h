/*
 * JSON-driven raid strategy — experimental, opt-in.
 *
 * A normal Strategy whose trigger list is built from the in-memory rule set
 * (RaidJsonRuleSet) instead of hand-written C++. Reads the ruleset fresh on
 * every InitTriggers, so an engine re-init after a reload picks up edited JSON
 * with no rebuild. Registered as "json-raid"; never auto-applied on map entry,
 * so it only runs when toggled on via `.rjson on`.
 */
#ifndef _PLAYERBOT_JSONRAIDSTRATEGY_H
#define _PLAYERBOT_JSONRAIDSTRATEGY_H

#include "Multiplier.h"
#include "Strategy.h"

#include <set>
#include <string>

class Trigger;

// Data-driven multiplier: while its trigger is active for the bot, the listed
// action names have their relevance zeroed. Built from a json-raid `suppress`
// rule, it gives json-raid the InitMultipliers capability the hand-tuned C++
// strategies have (e.g. HeiganDanceMultiplier suppressing "avoid aoe"). The
// trigger is resolved lazily from the shared context on first use.
class JsonSuppressMultiplier : public Multiplier
{
public:
    JsonSuppressMultiplier(PlayerbotAI* ai, std::string triggerName, std::set<std::string> names)
        : Multiplier(ai, "json suppress"), _triggerName(std::move(triggerName)), _names(std::move(names))
    {
    }

    float GetValue(Action* action) override;

private:
    std::string _triggerName;
    std::set<std::string> _names;
    Trigger* _trigger = nullptr;
    bool _resolved = false;
};

class JsonRaidStrategy : public Strategy
{
public:
    JsonRaidStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "json-raid"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

#endif
