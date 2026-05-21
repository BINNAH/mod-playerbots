/*
 * Movement strategy (sibling of "follow"/"stay") that drives the bot along an
 * externally-set route via the "autopilot move" action. Enabled per-bot by
 * mod-solo-raid-progression's Autopilot run controller.
 */

#ifndef _PLAYERBOT_AUTOPILOTSTRATEGY_H
#define _PLAYERBOT_AUTOPILOTSTRATEGY_H

#include "NonCombatStrategy.h"

class PlayerbotAI;

class AutopilotStrategy : public Strategy
{
public:
    AutopilotStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "autopilot"; }
    std::vector<NextAction> getDefaultActions() override;
};

#endif // _PLAYERBOT_AUTOPILOTSTRATEGY_H
