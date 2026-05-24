/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "PartyMemberToBattleRez.h"

#include "Group.h"
#include "Playerbots.h"

// Same "is this spell a resurrect" test used by PartyMemberToResurrect, so we
// don't pick a corpse another rezzer is already casting on.
class IsTargetOfResurrectSpellBR : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT_NEW ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_SELF_RESURRECT)
                return true;
        }

        return false;
    }
};

// True if this (dead) druid still has a Rebirth rank known AND off cooldown, i.e.
// rezzing them would actually hand a battle rez back to the raid.
static bool DruidHasRebirthReady(Player* player)
{
    if (!player || player->getClass() != CLASS_DRUID)
        return false;

    // Rebirth ranks 1-7 (3.3.5a).
    static const uint32 rebirthRanks[] = {20484, 20739, 20742, 20747, 20748, 26994, 48477};
    for (uint32 spellId : rebirthRanks)
        if (player->HasSpell(spellId) && !player->HasSpellCooldown(spellId))
            return true;

    return false;
}

Unit* PartyMemberToBattleRez::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    IsTargetOfResurrectSpellBR resurrectPredicate;

    // Priority buckets, highest first.
    std::vector<Player*> mainTanks;
    std::vector<Player*> offTanks;
    std::vector<Player*> otherTanks;
    std::vector<Player*> healers;
    std::vector<Player*> druids;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot)
            continue;

        // Only dead, releasable corpses that nobody is already rezzing.
        if (member->isResurrectRequested() || member->getDeathState() != DeathState::Corpse)
            continue;
        if (IsTargetOfSpellCast(member, resurrectPredicate))
            continue;

        if (botAI->IsMainTank(member))
            mainTanks.push_back(member);
        else if (botAI->IsAssistTank(member))
            offTanks.push_back(member);
        else if (botAI->IsTank(member))
            otherTanks.push_back(member);
        else if (botAI->IsHeal(member))
            healers.push_back(member);  // resto druids land here too, ahead of the druid bucket
        else if (DruidHasRebirthReady(member))
            druids.push_back(member);
        // Everything else (DPS, CD-down non-healer druids) is intentionally skipped.
    }

    std::vector<std::vector<Player*>*> lists = {&mainTanks, &offTanks, &otherTanks, &healers, &druids};
    for (std::vector<Player*>* list : lists)
        for (Player* candidate : *list)
            if (Check(candidate))  // map / range / LOS / not-GM gate from PartyMemberValue
                return candidate;

    return nullptr;
}
