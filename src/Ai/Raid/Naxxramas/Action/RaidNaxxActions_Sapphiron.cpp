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
        // Holding center through the pre-liftoff/post-land passive windows too
        // keeps the tank ready to re-pick the boss the instant it re-aggros;
        // the flight action backs the tank out to hide once it actually flies.
        return MoveTo(NAXX_MAP_ID, helper.mainTankPos.first, helper.mainTankPos.second, helper.GENERIC_HEIGHT,
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // A couple seconds before liftoff the boss goes passive and glides to
    // center. Spend that lead time backing the non-tanks out onto the spread
    // ring, so the icebolts about to land hit spread-out players and the
    // resulting blocks are distributed — and nobody is bunched at center when
    // the air phase hits. The flight multiplier zeros assist/flee for this
    // window too, so bots hold the spread instead of chasing the disengaging
    // boss back to center. (If this also catches the brief post-land passive
    // window it's harmless — non-tanks simply re-form when the boss re-aggros.)
    if (helper.IsPreAirPhase())
    {
        std::pair<float, float> spread = helper.PreAirSpreadPos();
        if (bot->GetExactDist2d(spread.first, spread.second) <= 2.0f)
            return false;
        return MoveTo(NAXX_MAP_ID, spread.first, spread.second, helper.GENERIC_HEIGHT, false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);
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
        // Melee: keep them on Sapphiron's side flanks, clear of both melee-range
        // cones. Left to free DpsAssist chase, melee stand in the frontal Cleave
        // arc or directly behind in the Tail Sweep arc — both recur every ~10s.
        // While inside either cone, slide to the nearer side flank; otherwise
        // yield so DpsAssist keeps attacking. FindMeleePosToAvoidCleaveAndTail
        // only hands back a dest while unsafe, so once on the flank this falls
        // through to DPS — no dancing.
        std::vector<float> safeDest;
        if (helper.FindMeleePosToAvoidCleaveAndTail(safeDest))
            return MoveTo(NAXX_MAP_ID, safeDest[0], safeDest[1], safeDest[2], false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        return false;
    }

    uint32 index = botAI->GetGroupSlotIndex(bot);
    float angle = 0.85f * M_PI + M_PI * 0.02f * index;
    float distance = botAI->IsRanged(bot) ? 35.0f : 30.0f;
    float posX = helper.center.first + cos(angle) * distance;
    float posY = helper.center.second + sin(angle) * distance;

    // Already parked: don't re-issue MoveTo (anti-dance — same park-and-yield
    // pattern the flight/iceblock positioning uses).
    if (bot->GetExactDist2d(posX, posY) <= 2.0f)
        return false;

    return MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool SapphironFlightPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    // Encased bots are frozen solid: they can't move and they ARE a block the
    // rest hide behind. Yield the tick so heals/dps still fire.
    if (helper.HasIcebolt(bot))
        return false;

    // Hold off until the full set of ice blocks has formed (2 on 10-man, 3 on
    // 25-man) rather than diving at the first one. Once committed, hide behind
    // the assigned block; staggered slots keep bots from piling on one point.
    if (helper.ReadyToHideBehindIceblock())
    {
        std::vector<float> dest;
        if (helper.GetIceblockHidePos(dest))
        {
            // Already parked behind the block: yield (return false) instead of
            // consuming the tick. Returning true would break out of the engine's
            // action loop every tick, silencing the heal/dps rotation right when
            // the raid is eating Frost Aura + chill. We still don't re-issue a
            // MoveTo, so the anti-dance guarantee holds — and the GUID-stable
            // slot keeps the destination fixed for the whole phase. Assist/flee/
            // formation are zeroed in SapphironGenericMultiplier, so nothing
            // yanks the bot back out of LOS.
            if (bot->GetExactDist2d(dest[0], dest[1]) <= 1.5f)
                return false;
            return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
    }

    // Not committed yet (still waiting for the blocks to finish forming): dodge
    // chill if it's ticking on us, otherwise hold the backed-up spread ring so
    // the blocks form spread out and we're already clear of center.
    std::vector<float> chillDest;
    if (helper.FindPosToAvoidChill(chillDest))
        return MoveTo(NAXX_MAP_ID, chillDest[0], chillDest[1], chillDest[2], false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);

    std::pair<float, float> spread = helper.PreAirSpreadPos();
    if (bot->GetExactDist2d(spread.first, spread.second) <= 2.0f)
        return false;
    return MoveTo(NAXX_MAP_ID, spread.first, spread.second, helper.GENERIC_HEIGHT, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
