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
        creators["gormok off tank backoff"] = &RaidTocActionContext::gormok_off_tank_backoff;
        creators["gormok tank off snobold"] = &RaidTocActionContext::gormok_tank_off_snobold;
        creators["gormok melee focus boss"] = &RaidTocActionContext::gormok_melee_focus_boss;
        creators["gormok spread"] = &RaidTocActionContext::gormok_spread;
        creators["gormok snobbled run in"] = &RaidTocActionContext::gormok_snobbled_run_in;
        creators["worms focus dreadscale"] = &RaidTocActionContext::worms_focus_dreadscale;
        creators["worms run to burning bile"] = &RaidTocActionContext::worms_run_to_burning_bile;
        creators["worms avoid slime pool"] = &RaidTocActionContext::worms_avoid_slime_pool;
        creators["worms tank face away"] = &RaidTocActionContext::worms_tank_face_away;
        creators["worms avoid burning bile"] = &RaidTocActionContext::worms_avoid_burning_bile;
        creators["icehowl dodge charge"] = &RaidTocActionContext::icehowl_dodge_charge;
        creators["gormok self bop clear impale"] = &RaidTocActionContext::gormok_self_bop_clear_impale;
        creators["gormok remove self bop"] = &RaidTocActionContext::gormok_remove_self_bop;
    }

private:
    static Action* gormok_move_away_fire_bomb(PlayerbotAI* ai) { return new GormokMoveAwayFireBombAction(ai); }
    static Action* gormok_attack_snobold(PlayerbotAI* ai) { return new GormokAttackSnoboldAction(ai); }
    static Action* gormok_tank_swap_taunt(PlayerbotAI* ai) { return new GormokTankSwapTauntAction(ai); }
    static Action* gormok_off_tank_backoff(PlayerbotAI* ai) { return new GormokOffTankBackoffAction(ai); }
    static Action* gormok_tank_off_snobold(PlayerbotAI* ai) { return new GormokTankOffSnoboldAction(ai); }
    static Action* gormok_melee_focus_boss(PlayerbotAI* ai) { return new GormokMeleeFocusBossAction(ai); }
    static Action* gormok_spread(PlayerbotAI* ai) { return new GormokSpreadAction(ai); }
    static Action* gormok_snobbled_run_in(PlayerbotAI* ai) { return new GormokSnobbledRunInAction(ai); }
    static Action* worms_focus_dreadscale(PlayerbotAI* ai) { return new WormsFocusDreadscaleAction(ai); }
    static Action* worms_run_to_burning_bile(PlayerbotAI* ai) { return new WormsRunToBurningBileAction(ai); }
    static Action* worms_avoid_slime_pool(PlayerbotAI* ai) { return new WormsAvoidSlimePoolAction(ai); }
    static Action* worms_tank_face_away(PlayerbotAI* ai) { return new WormsTankFaceAwayAction(ai); }
    static Action* worms_avoid_burning_bile(PlayerbotAI* ai) { return new WormsAvoidBurningBileAction(ai); }
    static Action* icehowl_dodge_charge(PlayerbotAI* ai) { return new IcehowlDodgeChargeAction(ai); }
    static Action* gormok_self_bop_clear_impale(PlayerbotAI* ai) { return new GormokSelfBopClearImpaleAction(ai); }
    static Action* gormok_remove_self_bop(PlayerbotAI* ai) { return new GormokRemoveSelfBopAction(ai); }
};

#endif
