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

    // Platform stand spot, pulled ~8y back toward Heigan's home
    // (HeiganPos = (2796, -3707)) from the old edge spot (2789.82, -3695.14).
    // The eruption wedges fan out from HeiganPos into the x<2795 / y>-3706
    // region; the old spot sat ~13y out in that fan, so melee stacked on the
    // tank clipped the section tiles and Heigan's Spell Disruption aura
    // reached the ranged behind him. Moving along the vector toward HeiganPos
    // lands the stack ~5.4y from his home — clear of the dance-floor tiles,
    // still melee range. The platform itself has no eruption GOs.
    constexpr float kPlatformX = 2795.52f;
    constexpr float kPlatformY = -3705.23f;
    constexpr float kPlatformZ = 274.88f;
    constexpr float kPlatformTolerance = 3.0f;

    // Slow phase runs exactly 90s before Heigan teleports and the fast dance
    // begins (boss_heigan.cpp StartFightPhase: the +90s task switches to
    // PHASE_FAST_DANCE). Melee DPS abandon the platform this many ms early and
    // join the slow dance, so they're already on the floor — not sprinting the
    // ~34y off the platform — when the +7s/+4s fast cadence opens.
    constexpr uint32 kSlowPhaseLengthMs = 90000;
    constexpr uint32 kPreDanceLeadMs = 8000;

    // Predicted safe section for the eruption in progress. Pure function of the
    // phase clock so every caller agrees: the dance action (ranged through the
    // slow phase, everyone through the fast phase) and the platform action's
    // pre-dance lead-in both route through it.
    uint8 ComputeSafeSectionAt(uint32 phase_start_ms, uint32 now, bool fast_phase)
    {
        uint32 first_delay = fast_phase ? kFastFirstEruptionDelayMs : kSlowFirstEruptionDelayMs;
        uint32 interval = fast_phase ? kFastEruptionIntervalMs : kSlowEruptionIntervalMs;

        uint32 elapsed = now - phase_start_ms;
        uint32 k;
        if (elapsed < first_delay)
            k = 0;
        else
            k = (elapsed - first_delay) / interval + 1;
        return kPattern[k % 6];
    }
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
    uint32 now = getMSTime();

    // Slow-phase clock. This action only runs during the slow phase — the
    // fast-dance trigger outranks it the instant Heigan teleports — so it sits
    // dormant for the whole ~45s fast phase. Anchor on first sight (the pull)
    // and re-anchor whenever we resume after that dormancy; the resume instant
    // coincides with the real slow-phase start (fast ends -> StartFightPhase
    // flips back to slow on the same tick HeiganIsFastDancing goes false). The
    // 30s threshold sits above any single melee GCD/swing but well under the
    // 45s fast phase, so only the genuine fast->slow gap re-anchors.
    constexpr uint32 kStaleStateGapMs = 30000;
    bool fresh = (slow_phase_start_ms == 0);
    bool long_gap = (last_seen_ms != 0) && (now - last_seen_ms > kStaleStateGapMs);
    if (fresh || long_gap)
        slow_phase_start_ms = now;
    last_seen_ms = now;

    // Final seconds before the teleport: melee DPS bleed off the platform and
    // pick up the slow dance early, dodging the remaining slow eruptions via the
    // same safe-section math the ranged use. When the fast phase opens,
    // HeiganFastDanceTrigger takes over mid-dance and the hand-off is seamless —
    // no platform->floor sprint against the +7s/+4s cadence. The main tank stays
    // parked so Heigan keeps tanked on the platform until he teleports himself.
    if (!botAI->IsTank(bot) && (now - slow_phase_start_ms) >= kSlowPhaseLengthMs - kPreDanceLeadMs)
    {
        uint8 section = ComputeSafeSectionAt(slow_phase_start_ms, now, /*fast_phase=*/false);
        float x = kSafeSpotsXY[section * 2];
        float y = kSafeSpotsXY[section * 2 + 1];
        if (!bot->IsWithinDist2d(x, y, kInPositionTolerance))
        {
            botAI->InterruptSpell();
            MoveTo(bot->GetMapId(), x, y, kSafeSpotZ, false, false, false, false,
                   MovementPriority::MOVEMENT_COMBAT);
        }
        // Hold the tick whether moving or parked on the wedge. Unlike the fast
        // phase (boss teleported to center, passive, unreachable), Heigan is
        // still tanked on the platform here, so handing control back would let
        // melee DpsAssist drag the bot straight back into him off the safe spot.
        return true;
    }

    // Already on the spot — let the bot's tank/melee logic take over
    // (threat, white attacks, boss pathing into melee from his spawn).
    if (bot->IsWithinDist2d(kPlatformX, kPlatformY, kPlatformTolerance))
        return false;

    MoveTo(bot->GetMapId(), kPlatformX, kPlatformY, kPlatformZ, false, false, false,
           false, MovementPriority::MOVEMENT_COMBAT);
    // Hold the tick while en route (see HeiganDanceAction) so the move to the
    // platform isn't cut short by an attack/assist action the moment MoveTo
    // reports a duplicate.
    return true;
}

uint8 HeiganDanceAction::ComputeSafeSection(uint32 now) const
{
    // Before the first eruption the shared helper returns pattern[0] = section
    // 3, the safe opening tile. After it, the index walks to the NEXT eruption:
    // the bot holds safeSpot[pattern[k]] through that eruption, then k advances
    // and it slides to the new safe spot during the lull.
    return ComputeSafeSectionAt(phase_start_ms, now, fast_phase);
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
    //
    // The threshold MUST stay well above a single cast/channel. A casting bot
    // parks its AI for the whole cast (PlayerbotAI sets nextCheckDelay =
    // castTime + reactDelay), so this action isn't re-entered until the cast
    // ends. At 2000ms, every ranged nuke (2-3.5s) looked like a "gap", reset
    // the clock to phase start every cast, and pinned ranged DPS at the
    // pre-position section forever (melee, with sub-2s GCDs, danced fine). The
    // real dormancy we need to catch is the ~90s a melee spends on the platform
    // across a slow phase, so anything between the longest channel (~10s) and
    // that 90s works; 30s leaves margin on both sides.
    constexpr uint32 kStaleStateGapMs = 30000;
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
    MoveTo(bot->GetMapId(), x, y, kSafeSpotZ, false, false, false, false,
           MovementPriority::MOVEMENT_COMBAT);
    // Hold the tick while still en route. MoveTo returns false once the move
    // is a duplicate (already heading to this spot), and returning that false
    // would yield to lower-priority actions — dps-assist (relevance 50) and
    // heal-reach — which then fire a cast that halts the bot mid-floor. The
    // engine breaks on the first action that returns true, so returning true
    // here keeps the dance owning movement until the bot actually reaches the
    // safe wedge, at which point the in-position check above hands control back.
    return true;
}
