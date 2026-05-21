#ifndef _PLAYERBOT_RAIDTOCACTIONCONTEXT_H
#define _PLAYERBOT_RAIDTOCACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "RaidTocActions.h"

class RaidTocActionContext : public NamedObjectContext<Action>
{
public:
    RaidTocActionContext()
    {
        creators["gormok move away fire bomb"] = &RaidTocActionContext::gormok_move_away_fire_bomb;
        creators["gormok attack snobold"] = &RaidTocActionContext::gormok_attack_snobold;
        creators["gormok tank swap taunt"] = &RaidTocActionContext::gormok_tank_swap_taunt;
        creators["gormok spread"] = &RaidTocActionContext::gormok_spread;
        creators["worms run to burning bile"] = &RaidTocActionContext::worms_run_to_burning_bile;
        creators["worms avoid slime pool"] = &RaidTocActionContext::worms_avoid_slime_pool;
        creators["icehowl dodge charge"] = &RaidTocActionContext::icehowl_dodge_charge;
    }

private:
    static Action* gormok_move_away_fire_bomb(PlayerbotAI* ai) { return new GormokMoveAwayFireBombAction(ai); }
    static Action* gormok_attack_snobold(PlayerbotAI* ai) { return new GormokAttackSnoboldAction(ai); }
    static Action* gormok_tank_swap_taunt(PlayerbotAI* ai) { return new GormokTankSwapTauntAction(ai); }
    static Action* gormok_spread(PlayerbotAI* ai) { return new GormokSpreadAction(ai); }
    static Action* worms_run_to_burning_bile(PlayerbotAI* ai) { return new WormsRunToBurningBileAction(ai); }
    static Action* worms_avoid_slime_pool(PlayerbotAI* ai) { return new WormsAvoidSlimePoolAction(ai); }
    static Action* icehowl_dodge_charge(PlayerbotAI* ai) { return new IcehowlDodgeChargeAction(ai); }
};

#endif
