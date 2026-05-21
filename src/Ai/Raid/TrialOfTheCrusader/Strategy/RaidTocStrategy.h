#ifndef _PLAYERBOT_RAIDTOCSTRATEGY_H
#define _PLAYERBOT_RAIDTOCSTRATEGY_H

#include "Strategy.h"

class RaidTocStrategy : public Strategy
{
public:
    RaidTocStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "toc"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
