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
        creators["gormok off tank backoff"] = &RaidTocTriggerContext::gormok_off_tank_backoff;
        creators["gormok tank off snobold"] = &RaidTocTriggerContext::gormok_tank_off_snobold;
        creators["gormok melee focus boss"] = &RaidTocTriggerContext::gormok_melee_focus_boss;
        creators["gormok spread"] = &RaidTocTriggerContext::gormok_spread;
        creators["gormok snobbled run in"] = &RaidTocTriggerContext::gormok_snobbled_run_in;
        creators["worms focus dreadscale"] = &RaidTocTriggerContext::worms_focus_dreadscale;
        creators["worms paralytic toxin"] = &RaidTocTriggerContext::worms_paralytic_toxin;
        creators["worms slime pool"] = &RaidTocTriggerContext::worms_slime_pool;
        creators["worms tank face away"] = &RaidTocTriggerContext::worms_tank_face_away;
        creators["worms avoid burning bile"] = &RaidTocTriggerContext::worms_avoid_burning_bile;
        creators["icehowl charge"] = &RaidTocTriggerContext::icehowl_charge;
        creators["jaraxxus nether power"] = &RaidTocTriggerContext::jaraxxus_nether_power;
        creators["jaraxxus legion flame"] = &RaidTocTriggerContext::jaraxxus_legion_flame;
        creators["gormok impale self bop"] = &RaidTocTriggerContext::gormok_impale_self_bop;
        creators["gormok remove self bop"] = &RaidTocTriggerContext::gormok_remove_self_bop;
    }

private:
    static Trigger* gormok_near_fire_bomb(PlayerbotAI* ai) { return new GormokNearFireBombTrigger(ai); }
    static Trigger* gormok_snobold_up(PlayerbotAI* ai) { return new GormokSnoboldUpTrigger(ai); }
    static Trigger* gormok_impale_tank_swap(PlayerbotAI* ai) { return new GormokImpaleTankSwapTrigger(ai); }
    static Trigger* gormok_off_tank_backoff(PlayerbotAI* ai) { return new GormokOffTankBackoffTrigger(ai); }
    static Trigger* gormok_tank_off_snobold(PlayerbotAI* ai) { return new GormokTankOffSnoboldTrigger(ai); }
    static Trigger* gormok_melee_focus_boss(PlayerbotAI* ai) { return new GormokMeleeFocusBossTrigger(ai); }
    static Trigger* gormok_spread(PlayerbotAI* ai) { return new GormokSpreadTrigger(ai); }
    static Trigger* gormok_snobbled_run_in(PlayerbotAI* ai) { return new GormokSnobbledRunInTrigger(ai); }
    static Trigger* worms_focus_dreadscale(PlayerbotAI* ai) { return new WormsFocusDreadscaleTrigger(ai); }
    static Trigger* worms_paralytic_toxin(PlayerbotAI* ai) { return new WormsParalyticToxinTrigger(ai); }
    static Trigger* worms_slime_pool(PlayerbotAI* ai) { return new WormsSlimePoolTrigger(ai); }
    static Trigger* worms_tank_face_away(PlayerbotAI* ai) { return new WormsTankFaceAwayTrigger(ai); }
    static Trigger* worms_avoid_burning_bile(PlayerbotAI* ai) { return new WormsAvoidBurningBileTrigger(ai); }
    static Trigger* icehowl_charge(PlayerbotAI* ai) { return new IcehowlChargeTrigger(ai); }
    static Trigger* jaraxxus_nether_power(PlayerbotAI* ai) { return new JaraxxusNetherPowerTrigger(ai); }
    static Trigger* jaraxxus_legion_flame(PlayerbotAI* ai) { return new JaraxxusLegionFlameTrigger(ai); }
    static Trigger* gormok_impale_self_bop(PlayerbotAI* ai) { return new GormokImpaleSelfBopTrigger(ai); }
    static Trigger* gormok_remove_self_bop(PlayerbotAI* ai) { return new GormokRemoveSelfBopTrigger(ai); }
};

#endif
