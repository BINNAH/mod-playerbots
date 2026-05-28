#include "JsonRaidStrategy.h"

#include "Action.h"
#include "AiObjectContext.h"
#include "GenericSpellActions.h"  // CastSpellAction / CastHealingSpellAction (@damage token)
#include "JsonStrategyLoader.h"
#include "Timer.h"                // getMSTime (grace-window timer for suppress)
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
        multipliers.push_back(new JsonSuppressMultiplier(botAI, s.trigger, s.names, s.minAgeMs));
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
    {
        // Trigger inactive -> reset the grace timer so the next activation gets
        // a fresh window (e.g. a multi-wave phase that re-enters).
        _activeSince = 0;
        return 1.0f;
    }

    // Grace window: don't suppress until the trigger has been continuously active
    // for `_minAgeMs`. Lets a phase's INITIAL pickup happen normally (e.g. tanks
    // taunting their first add at pull) before in-phase suppression kicks in.
    if (_minAgeMs > 0)
    {
        uint32 now = getMSTime();
        if (_activeSince == 0)
            _activeSince = now;
        if (now - _activeSince < _minAgeMs)
            return 1.0f;
    }

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
