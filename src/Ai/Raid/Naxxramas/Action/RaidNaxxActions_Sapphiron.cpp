#include "RaidNaxxActions.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidNaxxBossHelper.h"
#include "RaidNaxxSpellIds.h"

bool SapphironGroundPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (botAI->IsMainTank(bot))
    {
        // Drag the boss to mainTankPos unconditionally. Gating on "has
        // aggro" left the tank standing still whenever threat blipped
        // (Life Drain victim, brief taunt swap, post-land aggro reset) —
        // the boss then sat happily on the raid 30-40y NW of center
        // until aggro re-established. Force the spot continuously so
        // the boss holds the tank spot even through threat hiccups.
        return MoveTo(NAXX_MAP_ID, helper.mainTankPos.first, helper.mainTankPos.second, helper.GENERIC_HEIGHT,
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // Chill avoidance trumps formation: the chill DO ticks hard and the
    // angle calc has a per-bot offset so dodge spots don't pile up.
    std::vector<float> dest;
    if (helper.FindPosToAvoidChill(dest))
        return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);

    // Ranged + healers: enforce the NW formation arc throughout ground
    // phase, not just the 5s JustLanded window. Without this, ranged
    // never repositioned between landings — they engaged wherever combat
    // started (often at spawn, same Y as ranged spot) and stayed there
    // through Frost Aura.
    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
    {
        // Melee: keep them off Sapphiron's tail. The rear-cone Tail Sweep
        // recurs every ~10s; left to free DpsAssist chase, melee routinely
        // stand directly behind and get whipped. In the rear danger arc, slide
        // to a side flank (clear of both tail and frontal cleave); otherwise
        // yield so DpsAssist keeps attacking. FindMeleePosToAvoidTail only
        // hands back a dest while unsafe, so once on the flank this falls
        // through to DPS — no dancing.
        std::vector<float> tailDest;
        if (helper.FindMeleePosToAvoidTail(tailDest))
            return MoveTo(NAXX_MAP_ID, tailDest[0], tailDest[1], tailDest[2], false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        return false;
    }

    uint32 index = botAI->GetGroupSlotIndex(bot);
    float angle = 0.85f * M_PI + M_PI * 0.02f * index;
    float distance = botAI->IsRanged(bot) ? 35.0f : 30.0f;
    float posX = helper.center.first + cos(angle) * distance;
    float posY = helper.center.second + sin(angle) * distance;

    // Already parked: don't re-issue MoveTo. Same anti-dance pattern as
    // SapphironFlightPositionAction::MoveToNearestIcebolt.
    if (bot->GetExactDist2d(posX, posY) <= 2.0f)
        return false;

    return MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool SapphironFlightPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    if (helper.WaitForExplosion())
        return MoveToNearestIcebolt();
    else
    {
        std::vector<float> dest;
        if (helper.FindPosToAvoidChill(dest))
            return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool SapphironFlightPositionAction::MoveToNearestIcebolt()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Player* playerWithIcebolt = nullptr;
    float minDistance = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;
        if (NaxxSpellIds::HasAnyAura(botAI, member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
            botAI->HasAura("icebolt", member, false, false, -1, true))
        {
            float d = bot->GetDistance(member);
            if (!playerWithIcebolt || minDistance > d)
            {
                playerWithIcebolt = member;
                minDistance = d;
            }
        }
    }
    if (!playerWithIcebolt)
        return false;

    // Anchor the "behind the block" direction to the room center, not the
    // live boss position. The flying boss drifts and rotates every tick,
    // which shifts boss->GetAngle(player) by a degree or two per server
    // pulse — the bot then re-pathfinds to a slightly different spot and
    // the result is the dance behind the block. The room center is stable.
    float angle = std::atan2(playerWithIcebolt->GetPositionY() - helper.center.second,
                             playerWithIcebolt->GetPositionX() - helper.center.first);
    float posX = playerWithIcebolt->GetPositionX() + cos(angle) * 3.0f;
    float posY = playerWithIcebolt->GetPositionY() + sin(angle) * 3.0f;

    // Already parked behind the block: yield the tick (return false) instead
    // of consuming it (return true). Returning true broke out of the engine's
    // action loop (Engine::DoNextAction) every tick the bot sat behind its
    // block, so the heal/dps rotation never ran for the whole Deep Breath /
    // air phase — healers went silent right when the raid is eating Frost Aura
    // + chill damage. Returning false lets lower-priority actions (heals) fire
    // while the bot stays put: we still don't re-issue a MoveTo, so the
    // anti-dance guarantee holds. Movement that could pull the bot off the
    // block (assist/flee/formation) is already zeroed for the flight phase in
    // SapphironGenericMultiplier, so nothing yanks it out of LOS.
    if (bot->GetExactDist2d(posX, posY) <= 1.5f)
        return false;

    return MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
