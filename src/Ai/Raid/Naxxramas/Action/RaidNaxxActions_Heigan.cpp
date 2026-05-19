#include "Playerbots.h"
#include "RaidNaxxActions.h"
#include "RaidNaxxBossHelper.h"
#include "Timer.h"

namespace
{
    // Safe-zone centers, indexed by the boss script's `_currentSection`
    // value. Verified against instance_naxxramas.cpp's GetEruptionSection
    // (wedges fan out from HeiganPos = (2796, -3707); section 3 is nearest
    // the spawn corner, section 0 is the far SW strip).
    constexpr float kSafeSpotsXY[8] = {
        2756.0f, -3704.0f,  // section 0
        2762.3f, -3684.6f,  // section 1
        2775.5f, -3674.4f,  // section 2
        2794.9f, -3668.1f,  // section 3
    };
    constexpr float kSafeSpotZ = 276.54f;

    // Triangle wave with period 6 — the safe section for the K-th eruption
    // in either phase. The boss script resets `_currentSection = 3` on each
    // phase entry so the pattern is shared.
    constexpr uint8 kPattern[6] = { 3, 2, 1, 0, 1, 2 };

    // Phase timing from boss_heigan.cpp StartFightPhase().
    constexpr uint32 kSlowFirstEruptionDelayMs = 15000;
    constexpr uint32 kSlowEruptionIntervalMs = 10000;
    constexpr uint32 kFastFirstEruptionDelayMs = 7000;
    constexpr uint32 kFastEruptionIntervalMs = 4000;

    constexpr float kInPositionTolerance = 5.0f;

    // Platform stand spot — confirmed in-game by walking the player there
    // and reading the position. Geometrically section 1 by
    // GetEruptionSection, but the eruption GOs are only on the dance floor
    // proper so the platform stays clean during slow phase. Tank pulls
    // Heigan into melee from his spawn ~14y away; melee DPS stack here on
    // the tank.
    constexpr float kPlatformX = 2789.82f;
    constexpr float kPlatformY = -3695.14f;
    constexpr float kPlatformZ = 274.88f;
    constexpr float kPlatformTolerance = 3.0f;
}

bool HeiganFollowMasterAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master || !master->IsInWorld() || master->isDead())
        return false;

    if (bot->GetMapId() != master->GetMapId())
        return false;

    // Drop whatever the bot is doing — casting through the dance kills you.
    botAI->InterruptSpell();

    // Stick to the master's exact tile. MOVEMENT_COMBAT outranks DPS movement
    // so the bot stays glued even mid-rotation.
    return MoveTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(),
                  master->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool HeiganPlatformAction::Execute(Event /*event*/)
{
    // Already on the spot — let the bot's tank/melee logic take over
    // (threat, white attacks, boss pathing into melee from his spawn).
    if (bot->IsWithinDist2d(kPlatformX, kPlatformY, kPlatformTolerance))
        return false;

    return MoveTo(bot->GetMapId(), kPlatformX, kPlatformY, kPlatformZ, false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT);
}

uint8 HeiganDanceAction::ComputeSafeSection(uint32 now) const
{
    uint32 first_delay = fast_phase ? kFastFirstEruptionDelayMs : kSlowFirstEruptionDelayMs;
    uint32 interval = fast_phase ? kFastEruptionIntervalMs : kSlowEruptionIntervalMs;

    uint32 elapsed = now - phase_start_ms;
    uint32 k;
    if (elapsed < first_delay)
    {
        // Before the first eruption — pre-position at section 3, the safe
        // section for that opening tick.
        k = 0;
    }
    else
    {
        // Index of the NEXT eruption. The bot stays at safeSpot[pattern[k]]
        // through that eruption, then k advances and it moves to the new
        // safe spot during the lull.
        k = (elapsed - first_delay) / interval + 1;
    }
    return kPattern[k % 6];
}

bool HeiganDanceAction::Execute(Event /*event*/)
{
    Unit* heigan = AI_VALUE2(Unit*, "find target", "heigan the unclean");
    if (!heigan)
        return false;

    uint32 now = getMSTime();
    bool is_fast_now = HeiganIsFastDancing(botAI, heigan);

    // Restart the clock on:
    //  - Fresh pull (phase_start==0).
    //  - Observed phase flag change (slow↔fast within consecutive ticks).
    //  - Long gap since last invocation. Critical for tank/melee: their
    //    slow-phase trigger goes to "heigan platform" instead of "heigan
    //    dance", so this action sits idle for ~90s between fast phases.
    //    Without this branch they'd resume fast phase 2 with stale state
    //    from fast phase 1 — phase_start_ms reads ~135s old, the section
    //    index lands at pattern[33 % 6] = 0, and they sprint to the SW
    //    corner instead of the NE one.
    constexpr uint32 kStaleStateGapMs = 2000;
    bool phase_changed = (phase_start_ms != 0) && (is_fast_now != fast_phase);
    bool fresh = (phase_start_ms == 0);
    bool long_gap = (last_seen_ms != 0) && (now - last_seen_ms > kStaleStateGapMs);
    if (phase_changed || fresh || long_gap)
    {
        phase_start_ms = now;
        fast_phase = is_fast_now;
    }
    last_seen_ms = now;

    uint8 section = ComputeSafeSection(now);
    float x = kSafeSpotsXY[section * 2];
    float y = kSafeSpotsXY[section * 2 + 1];

    // Already in the right wedge — hand control back so DPS / threat /
    // healing rotations run.
    if (bot->IsWithinDist2d(x, y, kInPositionTolerance))
        return false;

    botAI->InterruptSpell();
    return MoveTo(bot->GetMapId(), x, y, kSafeSpotZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
