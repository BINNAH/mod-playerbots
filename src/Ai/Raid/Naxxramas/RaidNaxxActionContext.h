// /*
//  * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
//  and/or modify it under version 3 of the License, or (at your option), any later version.
//  */

#ifndef _PLAYERBOT_RAIDNAXXACTIONCONTEXT_H
#define _PLAYERBOT_RAIDNAXXACTIONCONTEXT_H

#include "Action.h"
#include "BossAuraActions.h"
#include "NamedObjectContext.h"
#include "RaidNaxxActions.h"

class RaidNaxxActionContext : public NamedObjectContext<Action>
{
public:
    RaidNaxxActionContext()
    {
        creators["grobbulus go behind the boss"] = &RaidNaxxActionContext::go_behind_the_boss;
        creators["rotate grobbulus"] = &RaidNaxxActionContext::rotate_grobbulus;
        creators["grobbulus move center"] = &RaidNaxxActionContext::grobbulus_move_center;
        creators["grobbulus move away"] = &RaidNaxxActionContext::grobbulus_move_away;

        creators["heigan follow master"] = &RaidNaxxActionContext::heigan_follow_master;
        creators["heigan platform"] = &RaidNaxxActionContext::heigan_platform;
        creators["heigan dance"] = &RaidNaxxActionContext::heigan_dance;
        creators["thaddius attack nearest pet"] = &RaidNaxxActionContext::thaddius_attack_nearest_pet;
        // creators["thaddius melee to place"] = &RaidNaxxActionContext::thaddius_tank_to_place;
        // creators["thaddius ranged to place"] = &RaidNaxxActionContext::thaddius_ranged_to_place;
        creators["thaddius move to platform"] = &RaidNaxxActionContext::thaddius_move_to_platform;
        creators["thaddius move polarity"] = &RaidNaxxActionContext::thaddius_move_polarity;

        creators["razuvious use obedience crystal"] = &RaidNaxxActionContext::razuvious_use_obedience_crystal;
        creators["razuvious target"] = &RaidNaxxActionContext::razuvious_target;

        creators["four horsemen attract alternatively"] = &RaidNaxxActionContext::four_horsemen_attract_alternatively;
        creators["four horsemen attack in order"] = &RaidNaxxActionContext::four_horsemen_attack_in_order;
        creators["four horsemen avoid void zone"] = &RaidNaxxActionContext::four_horsemen_avoid_void_zone;
        creators["four horsemen healer bleed off mark"] = &RaidNaxxActionContext::four_horsemen_healer_bleed_off_mark;
        creators["four horsemen back phase"] = &RaidNaxxActionContext::four_horsemen_back_phase;

        creators["sapphiron ground position"] = &RaidNaxxActionContext::sapphiron_ground_position;
        creators["sapphiron flight position"] = &RaidNaxxActionContext::sapphiron_flight_position;
        creators["sapphiron frost resistance action"] = &RaidNaxxActionContext::sapphiron_frost_resistance_action;

        creators["kel'thuzad choose target"] = &RaidNaxxActionContext::kelthuzad_choose_target;
        creators["kel'thuzad position"] = &RaidNaxxActionContext::kelthuzad_position;
        creators["kel'thuzad control pet"] = &RaidNaxxActionContext::kelthuzad_control_pet;

        creators["anub'rekhan choose target"] = &RaidNaxxActionContext::anubrekhan_choose_target;
        creators["anub'rekhan position"] = &RaidNaxxActionContext::anubrekhan_position;

        creators["maexxna attack web wrap"] = &RaidNaxxActionContext::maexxna_attack_web_wrap;
        creators["hand of sacrifice on main tank"] = &RaidNaxxActionContext::hand_of_sacrifice_on_main_tank;
        creators["guardian spirit on main tank"] = &RaidNaxxActionContext::guardian_spirit_on_main_tank;

        creators["gluth choose target"] = &RaidNaxxActionContext::gluth_choose_target;
        creators["gluth position"] = &RaidNaxxActionContext::gluth_position;
        creators["gluth slowdown"] = &RaidNaxxActionContext::gluth_slowdown;
        creators["gluth burn adds"] = &RaidNaxxActionContext::gluth_burn_adds;

        //creators["patchwerk ranged position"] = &RaidNaxxActionContext::patchwerk_ranged_position;

        creators["loatheb position"] = &RaidNaxxActionContext::loatheb_position;
        creators["loatheb choose target"] = &RaidNaxxActionContext::loatheb_choose_target;

        creators["noth tank adds"] = &RaidNaxxActionContext::noth_tank_adds;
    }

private:
    static Action* go_behind_the_boss(PlayerbotAI* ai) { return new GrobbulusGoBehindAction(ai); }
    static Action* rotate_grobbulus(PlayerbotAI* ai) { return new GrobbulusRotateAction(ai); }
    static Action* grobbulus_move_center(PlayerbotAI* ai) { return new GrobbulusMoveCenterAction(ai); }
    static Action* grobbulus_move_away(PlayerbotAI* ai) { return new GrobbulusMoveAwayAction(ai); }
    static Action* heigan_follow_master(PlayerbotAI* ai) { return new HeiganFollowMasterAction(ai); }
    static Action* heigan_platform(PlayerbotAI* ai) { return new HeiganPlatformAction(ai); }
    static Action* heigan_dance(PlayerbotAI* ai) { return new HeiganDanceAction(ai); }
    static Action* thaddius_attack_nearest_pet(PlayerbotAI* ai) { return new ThaddiusAttackNearestPetAction(ai); }
    // static Action* thaddius_tank_to_place(PlayerbotAI* ai) { return new ThaddiusMeleeToPlaceAction(ai); }
    // static Action* thaddius_ranged_to_place(PlayerbotAI* ai) { return new ThaddiusRangedToPlaceAction(ai); }
    static Action* thaddius_move_to_platform(PlayerbotAI* ai) { return new ThaddiusMoveToPlatformAction(ai); }
    static Action* thaddius_move_polarity(PlayerbotAI* ai) { return new ThaddiusMovePolarityAction(ai); }
    static Action* razuvious_target(PlayerbotAI* ai) { return new RazuviousTargetAction(ai); }
    static Action* razuvious_use_obedience_crystal(PlayerbotAI* ai)
    {
        return new RazuviousUseObedienceCrystalAction(ai);
    }
    static Action* four_horsemen_attract_alternatively(PlayerbotAI* ai) { return new FourHorsemenAttractAlternativelyAction(ai); }
    static Action* four_horsemen_attack_in_order(PlayerbotAI* ai) { return new FourHorsemenAttackInOrderAction(ai); }
    static Action* four_horsemen_avoid_void_zone(PlayerbotAI* ai) { return new FourHorsemenAvoidVoidZoneAction(ai); }
    static Action* four_horsemen_healer_bleed_off_mark(PlayerbotAI* ai) { return new FourHorsemenHealerBleedOffMarkAction(ai); }
    static Action* four_horsemen_back_phase(PlayerbotAI* ai) { return new FourHorsemenBackPhaseAction(ai); }
    // static Action* sapphiron_ground_main_tank_position(PlayerbotAI* ai) { return new
    // SapphironGroundMainTankPositionAction(ai); }
    static Action* sapphiron_ground_position(PlayerbotAI* ai) { return new SapphironGroundPositionAction(ai); }
    static Action* sapphiron_flight_position(PlayerbotAI* ai) { return new SapphironFlightPositionAction(ai); }
    static Action* sapphiron_frost_resistance_action(PlayerbotAI* ai) { return new BossFrostResistanceAction(ai, "sapphiron"); }
    // static Action* sapphiron_avoid_chill(PlayerbotAI* ai) { return new SapphironAvoidChillAction(ai); }
    static Action* kelthuzad_choose_target(PlayerbotAI* ai) { return new KelthuzadChooseTargetAction(ai); }
    static Action* kelthuzad_position(PlayerbotAI* ai) { return new KelthuzadPositionAction(ai); }
    static Action* kelthuzad_control_pet(PlayerbotAI* ai) { return new KelthuzadControlPetAction(ai); }
    static Action* anubrekhan_choose_target(PlayerbotAI* ai) { return new AnubrekhanChooseTargetAction(ai); }
    static Action* anubrekhan_position(PlayerbotAI* ai) { return new AnubrekhanPositionAction(ai); }
    static Action* maexxna_attack_web_wrap(PlayerbotAI* ai) { return new MaexxnaAttackWebWrapAction(ai); }
    static Action* hand_of_sacrifice_on_main_tank(PlayerbotAI* ai)
    {
        return new MaexxnaHandOfSacrificeOnMainTankAction(ai);
    }
    static Action* guardian_spirit_on_main_tank(PlayerbotAI* ai)
    {
        return new MaexxnaGuardianSpiritOnMainTankAction(ai);
    }
    static Action* gluth_choose_target(PlayerbotAI* ai) { return new GluthChooseTargetAction(ai); }
    static Action* gluth_position(PlayerbotAI* ai) { return new GluthPositionAction(ai); }
    static Action* gluth_slowdown(PlayerbotAI* ai) { return new GluthSlowdownAction(ai); }
    static Action* gluth_burn_adds(PlayerbotAI* ai) { return new GluthBurnAddsAction(ai); }
    //static Action* patchwerk_ranged_position(PlayerbotAI* ai) { return new PatchwerkRangedPositionAction(ai); }
    static Action* loatheb_position(PlayerbotAI* ai) { return new LoathebPositionAction(ai); }
    static Action* loatheb_choose_target(PlayerbotAI* ai) { return new LoathebChooseTargetAction(ai); }
    static Action* noth_tank_adds(PlayerbotAI* ai) { return new NothAddTankAction(ai); }
};

#endif
