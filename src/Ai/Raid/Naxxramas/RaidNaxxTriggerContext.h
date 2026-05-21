// /*
//  * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
//  and/or modify it under version 3 of the License, or (at your option), any later version.
//  */

#ifndef _PLAYERBOT_RAIDNAXXTRIGGERCONTEXT_H
#define _PLAYERBOT_RAIDNAXXTRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "RaidNaxxTriggers.h"

class RaidNaxxTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidNaxxTriggerContext()
    {
        creators["mutating injection melee"] = &RaidNaxxTriggerContext::mutating_injection_melee;
        creators["mutating injection ranged"] = &RaidNaxxTriggerContext::mutating_injection_ranged;
        creators["mutating injection removed"] = &RaidNaxxTriggerContext::mutating_injection_removed;
        creators["grobbulus cloud"] = &RaidNaxxTriggerContext::grobbulus_cloud;
        creators["heigan fast dance"] = &RaidNaxxTriggerContext::heigan_fast_dance;
        creators["heigan slow dance platform"] = &RaidNaxxTriggerContext::heigan_slow_dance_platform;
        creators["heigan slow dance ranged"] = &RaidNaxxTriggerContext::heigan_slow_dance_ranged;

        creators["thaddius phase pet"] = &RaidNaxxTriggerContext::thaddius_phase_pet;
        creators["thaddius phase pet lose aggro"] = &RaidNaxxTriggerContext::thaddius_phase_pet_lose_aggro;
        creators["thaddius phase transition"] = &RaidNaxxTriggerContext::thaddius_phase_transition;
        creators["thaddius phase thaddius"] = &RaidNaxxTriggerContext::thaddius_phase_thaddius;

        creators["razuvious tank"] = &RaidNaxxTriggerContext::razuvious_tank;
        creators["razuvious nontank"] = &RaidNaxxTriggerContext::razuvious_nontank;

        creators["four horsemen attractors"] = &RaidNaxxTriggerContext::four_horsemen_attractors;
        creators["four horsemen except attractors"] = &RaidNaxxTriggerContext::four_horsemen_except_attractors;
        creators["four horsemen void zone"] = &RaidNaxxTriggerContext::four_horsemen_void_zone;
        creators["four horsemen healer high mark"] = &RaidNaxxTriggerContext::four_horsemen_healer_high_mark;
        creators["four horsemen opening defensive"] = &RaidNaxxTriggerContext::four_horsemen_opening_defensive;

        creators["sapphiron ground"] = &RaidNaxxTriggerContext::sapphiron_ground;
        creators["sapphiron flight"] = &RaidNaxxTriggerContext::sapphiron_flight;
        creators["sapphiron frost resistance trigger"] = &RaidNaxxTriggerContext::sapphiron_frost_resistance_trigger;

        creators["kel'thuzad"] = &RaidNaxxTriggerContext::kelthuzad;

        creators["anub'rekhan"] = &RaidNaxxTriggerContext::anubrekhan;
        creators["faerlina"] = &RaidNaxxTriggerContext::faerlina;
        creators["maexxna"] = &RaidNaxxTriggerContext::maexxna;
        creators["maexxna web wrap"] = &RaidNaxxTriggerContext::maexxna_web_wrap;
        creators["maexxna pre web spray hand of sacrifice"] =
            &RaidNaxxTriggerContext::maexxna_pre_web_spray_hand_of_sacrifice;
        creators["maexxna pre web spray guardian spirit"] =
            &RaidNaxxTriggerContext::maexxna_pre_web_spray_guardian_spirit;
        creators["maexxna pre web spray tank defensive"] =
            &RaidNaxxTriggerContext::maexxna_pre_web_spray_tank_defensive;
        //creators["patchwerk tank"] = &RaidNaxxTriggerContext::patchwerk_tank;
        //creators["patchwerk non-tank"] = &RaidNaxxTriggerContext::patchwerk_non_tank;
        //creators["patchwerk ranged"] = &RaidNaxxTriggerContext::patchwerk_ranged;

        creators["gluth"] = &RaidNaxxTriggerContext::gluth;
        creators["gluth main tank mortal wound"] = &RaidNaxxTriggerContext::gluth_main_tank_mortal_wound;

        creators["loatheb"] = &RaidNaxxTriggerContext::loatheb;

        creators["noth add tank"] = &RaidNaxxTriggerContext::noth_add_tank;
    }

private:
    static Trigger* mutating_injection_melee(PlayerbotAI* ai) { return new MutatingInjectionMeleeTrigger(ai); }
    static Trigger* mutating_injection_ranged(PlayerbotAI* ai) { return new MutatingInjectionRangedTrigger(ai); }
    static Trigger* mutating_injection_removed(PlayerbotAI* ai) { return new MutatingInjectionRemovedTrigger(ai); }
    static Trigger* grobbulus_cloud(PlayerbotAI* ai) { return new GrobbulusCloudTrigger(ai); }
    static Trigger* heigan_fast_dance(PlayerbotAI* ai) { return new HeiganFastDanceTrigger(ai); }
    static Trigger* heigan_slow_dance_platform(PlayerbotAI* ai) { return new HeiganSlowDancePlatformTrigger(ai); }
    static Trigger* heigan_slow_dance_ranged(PlayerbotAI* ai) { return new HeiganSlowDanceRangedTrigger(ai); }

    static Trigger* thaddius_phase_pet(PlayerbotAI* ai) { return new ThaddiusPhasePetTrigger(ai); }
    static Trigger* thaddius_phase_pet_lose_aggro(PlayerbotAI* ai) { return new ThaddiusPhasePetLoseAggroTrigger(ai); }
    static Trigger* thaddius_phase_transition(PlayerbotAI* ai) { return new ThaddiusPhaseTransitionTrigger(ai); }
    static Trigger* thaddius_phase_thaddius(PlayerbotAI* ai) { return new ThaddiusPhaseThaddiusTrigger(ai); }
    static Trigger* razuvious_tank(PlayerbotAI* ai) { return new RazuviousTankTrigger(ai); }
    static Trigger* razuvious_nontank(PlayerbotAI* ai) { return new RazuviousNontankTrigger(ai); }

    static Trigger* four_horsemen_attractors(PlayerbotAI* ai) { return new FourHorsemenAttractorsTrigger(ai); }
    static Trigger* four_horsemen_except_attractors(PlayerbotAI* ai) { return new FourHorsemenExceptAttractorsTrigger(ai); }
    static Trigger* four_horsemen_void_zone(PlayerbotAI* ai) { return new FourHorsemenVoidZoneTrigger(ai); }
    static Trigger* four_horsemen_healer_high_mark(PlayerbotAI* ai) { return new FourHorsemenHealerHighMarkTrigger(ai); }
    static Trigger* four_horsemen_opening_defensive(PlayerbotAI* ai) { return new FourHorsemenOpeningDefensiveTrigger(ai); }

    static Trigger* sapphiron_ground(PlayerbotAI* ai) { return new SapphironGroundTrigger(ai); }
    static Trigger* sapphiron_flight(PlayerbotAI* ai) { return new SapphironFlightTrigger(ai); }
    static Trigger* sapphiron_frost_resistance_trigger(PlayerbotAI* ai) { return new BossFrostResistanceTrigger(ai, "sapphiron"); }
    static Trigger* kelthuzad(PlayerbotAI* ai) { return new KelthuzadTrigger(ai); }
    static Trigger* anubrekhan(PlayerbotAI* ai) { return new AnubrekhanTrigger(ai); }
    static Trigger* faerlina(PlayerbotAI* ai) { return new FaerlinaTrigger(ai); }
    static Trigger* maexxna(PlayerbotAI* ai) { return new MaexxnaTrigger(ai); }
    static Trigger* maexxna_web_wrap(PlayerbotAI* ai) { return new MaexxnaWebWrapTrigger(ai); }
    static Trigger* maexxna_pre_web_spray_hand_of_sacrifice(PlayerbotAI* ai)
    {
        return new MaexxnaPreWebSprayHandOfSacrificeTrigger(ai);
    }
    static Trigger* maexxna_pre_web_spray_guardian_spirit(PlayerbotAI* ai)
    {
        return new MaexxnaPreWebSprayGuardianSpiritTrigger(ai);
    }
    static Trigger* maexxna_pre_web_spray_tank_defensive(PlayerbotAI* ai)
    {
        return new MaexxnaPreWebSprayTankDefensiveTrigger(ai);
    }
    //static Trigger* patchwerk_tank(PlayerbotAI* ai) { return new PatchwerkTankTrigger(ai); }
    //static Trigger* patchwerk_non_tank(PlayerbotAI* ai) { return new PatchwerkNonTankTrigger(ai); }
    //static Trigger* patchwerk_ranged(PlayerbotAI* ai) { return new PatchwerkRangedTrigger(ai); }
    static Trigger* gluth(PlayerbotAI* ai) { return new GluthTrigger(ai); }
    static Trigger* gluth_main_tank_mortal_wound(PlayerbotAI* ai) { return new GluthMainTankMortalWoundTrigger(ai); }
    static Trigger* loatheb(PlayerbotAI* ai) { return new LoathebTrigger(ai); }
    static Trigger* noth_add_tank(PlayerbotAI* ai) { return new NothAddTankTrigger(ai); }
};

#endif
