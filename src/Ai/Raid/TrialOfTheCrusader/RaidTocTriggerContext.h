#ifndef _PLAYERBOT_RAIDTOCTRIGGERCONTEXT_H
#define _PLAYERBOT_RAIDTOCTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "RaidTocTriggers.h"

class RaidTocTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTocTriggerContext()
    {
        creators["gormok near fire bomb"] = &RaidTocTriggerContext::gormok_near_fire_bomb;
        creators["gormok snobold up"] = &RaidTocTriggerContext::gormok_snobold_up;
        creators["gormok impale tank swap"] = &RaidTocTriggerContext::gormok_impale_tank_swap;
        creators["gormok spread"] = &RaidTocTriggerContext::gormok_spread;
        creators["worms paralytic toxin"] = &RaidTocTriggerContext::worms_paralytic_toxin;
        creators["worms slime pool"] = &RaidTocTriggerContext::worms_slime_pool;
        creators["icehowl charge"] = &RaidTocTriggerContext::icehowl_charge;
    }

private:
    static Trigger* gormok_near_fire_bomb(PlayerbotAI* ai) { return new GormokNearFireBombTrigger(ai); }
    static Trigger* gormok_snobold_up(PlayerbotAI* ai) { return new GormokSnoboldUpTrigger(ai); }
    static Trigger* gormok_impale_tank_swap(PlayerbotAI* ai) { return new GormokImpaleTankSwapTrigger(ai); }
    static Trigger* gormok_spread(PlayerbotAI* ai) { return new GormokSpreadTrigger(ai); }
    static Trigger* worms_paralytic_toxin(PlayerbotAI* ai) { return new WormsParalyticToxinTrigger(ai); }
    static Trigger* worms_slime_pool(PlayerbotAI* ai) { return new WormsSlimePoolTrigger(ai); }
    static Trigger* icehowl_charge(PlayerbotAI* ai) { return new IcehowlChargeTrigger(ai); }
};

#endif
