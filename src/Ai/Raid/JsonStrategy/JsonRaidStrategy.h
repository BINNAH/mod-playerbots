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

#include "Strategy.h"

class JsonRaidStrategy : public Strategy
{
public:
    JsonRaidStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "json-raid"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
