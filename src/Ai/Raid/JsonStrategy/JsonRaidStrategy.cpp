#include "JsonRaidStrategy.h"

#include "JsonStrategyLoader.h"

void JsonRaidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    RaidJsonRuleSet& ruleSet = RaidJsonRuleSet::instance();

    // Lazy first load so the very first `.rjson on` has rules even before an
    // explicit `.rjson reload`.
    if (!ruleSet.Loaded())
        ruleSet.Load();

    for (JsonResolvedRule const& rule : ruleSet.Rules())
    {
        std::vector<NextAction> handlers;
        handlers.reserve(rule.actions.size());
        for (JsonResolvedAction const& act : rule.actions)
            handlers.push_back(NextAction(act.name, ACTION_RAID + act.priority));

        triggers.push_back(new TriggerNode(rule.trigger, std::move(handlers)));
    }
}
