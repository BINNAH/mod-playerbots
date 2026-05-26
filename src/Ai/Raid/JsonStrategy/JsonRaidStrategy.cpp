#include "JsonRaidStrategy.h"

#include "Action.h"
#include "AiObjectContext.h"
#include "GenericSpellActions.h"  // CastSpellAction / CastHealingSpellAction (@damage token)
#include "JsonStrategyLoader.h"
#include "Trigger.h"

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

void JsonRaidStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    RaidJsonRuleSet& ruleSet = RaidJsonRuleSet::instance();
    if (!ruleSet.Loaded())
        ruleSet.Load();

    for (JsonResolvedSuppress const& s : ruleSet.SuppressRules())
        multipliers.push_back(new JsonSuppressMultiplier(botAI, s.trigger, s.names));
}

float JsonSuppressMultiplier::GetValue(Action* action)
{
    if (!action || _names.empty())
        return 1.0f;

    // Resolve the gating trigger once from the shared context (handles the
    // "base::qualifier" form, e.g. "json encounter::boss=heigan the unclean").
    if (!_resolved)
    {
        _trigger = context->GetTrigger(_triggerName);
        _resolved = true;
    }

    if (!_trigger || !_trigger->IsActive())
        return 1.0f;

    // Exact action-name match (the original behavior, e.g. "avoid aoe").
    if (_names.count(action->getName()))
        return 0.0f;

    // Category token "@damage": zero every non-healing damage cast — the type
    // filter a name list can't express. Mirrors ThaddiusGenericMultiplier's
    // `dynamic_cast<CastSpellAction*> && !CastHealingSpellAction`, so a death-
    // sync suppress can throttle DPS generically without naming every spell.
    if (_names.count("@damage") && dynamic_cast<CastSpellAction*>(action) &&
        !dynamic_cast<CastHealingSpellAction*>(action))
        return 0.0f;

    return 1.0f;
}
