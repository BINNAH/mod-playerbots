#include "RaidNaxxActions.h"

#include <cmath>
#include <vector>

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"

namespace
{
    // Kite tuning (yards / radians). Easy to nudge after an in-game pull.
    constexpr float KITE_LEAD = 0.6f;            // ring step ahead per tick (~34 deg)
    constexpr float ESCAPE_DIST = 6.0f;          // blink/disengage if a chow is this close
    constexpr float MAGE_SNARE_RANGE = 10.0f;    // Frost Nova / Cone of Cold reach
    constexpr float HUNTER_SNARE_RANGE = 35.0f;  // Concussive Shot reach (snipe leaks)
    constexpr float HUNTER_TRAP_RANGE = 10.0f;   // drop Frost Trap when the pack is near
    // Decimate-burn add mop-up: how close a low-HP chow must be before a DPS
    // peels off the boss for it. Ranged catch them crossing the room; melee only
    // swing at ones basically on top of Gluth (they can't chase — see below).
    constexpr float BURN_RANGED_ENGAGE = 35.0f;
    constexpr float BURN_MELEE_ENGAGE = 10.0f;

    // An off-tank's best available AoE-threat ability, by class. Best-effort:
    // unknown / on-cooldown spells just fail and we fall through to the next.
    // Cast directly (the tank rotation is suppressed at Gluth) so the off-tank
    // holds threat on the chow pack. TUNABLE: add/reorder per your specs.
    bool CastZombieThreat(PlayerbotAI* botAI, Player* bot, Unit* target)
    {
        switch (bot->getClass())
        {
            case CLASS_WARRIOR:
                if (botAI->CastSpell("Thunder Clap", target)) return true;
                if (botAI->CastSpell("Shockwave", target)) return true;
                if (botAI->CastSpell("Cleave", target)) return true;
                if (botAI->CastSpell("Demoralizing Shout", target)) return true;
                break;
            case CLASS_PALADIN:
                if (botAI->CastSpell("Consecration", bot)) return true;
                if (botAI->CastSpell("Hammer of the Righteous", target)) return true;
                if (botAI->CastSpell("Holy Wrath", bot)) return true;
                if (botAI->CastSpell("Avenger's Shield", target)) return true;
                break;
            case CLASS_DEATH_KNIGHT:
                if (botAI->CastSpell("Death and Decay", target)) return true;
                if (botAI->CastSpell("Blood Boil", bot)) return true;
                if (botAI->CastSpell("Pestilence", target)) return true;
                break;
            case CLASS_DRUID:
                if (botAI->CastSpell("Swipe", target)) return true;
                if (botAI->CastSpell("Challenging Roar", bot)) return true;
                break;
            default:
                break;
        }
        return false;
    }
}  // namespace

bool GluthChooseTargetAction::Execute(Event /*event*/)
{
    // Kiters and off-tanks keep their own target (the chow), so don't force
    // Gluth on them.
    if (helper.IsKiter(bot) || helper.IsZombieOffTank(bot))
        return false;

    // If the Decimate-burn mop-up handed us a live chow, keep nuking it instead
    // of snapping back to the boss (otherwise we'd flip targets every tick).
    Unit* current = context->GetValue<Unit*>("current target")->Get();
    if (current && current->IsAlive() && helper.IsZombieChow(current))
        return false;

    Unit* target_boss = AI_VALUE2(Unit*, "find target", "gluth");
    if (!target_boss || !target_boss->IsAlive())
        return false;

    if (current == target_boss)
        return false;

    return Attack(target_boss, true);
}

bool GluthPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    // Kiters and off-tanks are driven by GluthSlowdownAction, not the raid stack.
    if (helper.IsKiter(bot) || helper.IsZombieOffTank(bot))
        return false;

    bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0) || botAI->IsAssistTankOfIndex(bot, 1))
    {
        if (!AI_VALUE2(bool, "has aggro", "boss target"))
            return false;

        float tankX = raid25 ? helper.mainTankPos25.first : helper.mainTankPos10.first;
        float tankY = raid25 ? helper.mainTankPos25.second : helper.mainTankPos10.second;
        return MoveTo(NAXX_MAP_ID, tankX, tankY, bot->GetPositionZ(), false, false, false,
                      false, MovementPriority::MOVEMENT_COMBAT);
    }

    float tankX = raid25 ? helper.mainTankPos25.first : helper.mainTankPos10.first;
    float tankY = raid25 ? helper.mainTankPos25.second : helper.mainTankPos10.second;

    // 25-man: stack the ranged/healer pack a few yards off the tank and fan it
    // out by FIXED group slot, so it nukes Gluth point-blank instead of pathing
    // NE out the north door. The fan spreads sideways (perpendicular to the
    // tank->anchor line) with only a small outward step, so no slot drifts back
    // toward the exit. Inputs are all constant (anchor, tank spot, slot) → the
    // destination is constant, so bots path once and park (no tick-to-tick
    // dance). 10-man's near-boss offset already works, so it's left untouched.
    if (raid25 && (botAI->IsRangedDps(bot) || botAI->IsHeal(bot)))
    {
        float ax = helper.rangedClusterPos25.first;
        float ay = helper.rangedClusterPos25.second;

        // Unit "outward" vector (tank -> anchor) and its left perpendicular.
        float dx = ax - tankX;
        float dy = ay - tankY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
        dx /= len;
        dy /= len;
        float perpX = -dy, perpY = dx;  // lateral spread axis

        uint32 slot = botAI->GetGroupSlotIndex(bot);
        uint32 col = slot % 7;  // up to 7 abreast before stepping a row back
        uint32 row = slot / 7;
        int colSign = (col % 2 == 0) ? 1 : -1;
        float lateral = float((col + 1) / 2) * 3.0f * colSign;  // 0,+3,-3,+6,-6,...
        float depth = float(row) * 3.0f;                        // back rows nudge out

        float px = ax + dx * depth + perpX * lateral;
        float py = ay + dy * depth + perpY * lateral;
        return MoveTo(NAXX_MAP_ID, px, py, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    if (botAI->IsRangedDps(bot))
        return MoveTo(NAXX_MAP_ID, tankX + 10.0f, tankY + 10.0f, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    if (botAI->IsHeal(bot))
        return MoveTo(NAXX_MAP_ID, tankX + 7.0f, tankY + 7.0f, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    return false;
}

bool GluthSlowdownAction::Execute(Event /*event*/)
{
    bool isKiter = helper.IsKiter(bot);
    bool isOffTank = helper.IsZombieOffTank(bot);
    // Only the designated 25-man chow handlers run this — the kiter hunters/
    // mages and the off-tanks. Everyone else (and all of 10-man) returns false
    // and is handled by GluthPositionAction / the normal rotation above.
    if (!isKiter && !isOffTank)
        return false;

    // Threat-independent gating: the handlers never threaten Gluth, so the old
    // find-target gate forced them to HIT him first just to "see" him — that was
    // the off-tank aggro-steal on the pull. UpdateBossAI() is still called (best
    // effort, to keep the threat-cache warm/cleared), but we run as long as the
    // encounter is live (instance state) and we can locate the boss via the room
    // scan, so the handlers head for the SW chow ring from the very first tick.
    helper.UpdateBossAI();
    if (!helper.GluthEngaged() && !helper.Boss())
        return false;

    Unit* boss = helper.BossAnyway();
    if (!boss)
        return false;

    bool isMage = bot->getClass() == CLASS_MAGE;
    std::vector<Unit*> chow = helper.GetAliveZombieChow();

    // Where a chow handler runs when kiting: a step ahead along the kite ring.
    // Aiming a fixed arc ahead of its own bearing off the ring center keeps it
    // circling at full speed (chasing chow strung out behind) with no fixed
    // waypoint to stutter on, and handlers hold their natural spacing instead of
    // stacking. Off the ring, it's pulled back onto it.
    const float cx = helper.kiteCenter25.first;
    const float cy = helper.kiteCenter25.second;
    float bearing = std::atan2(bot->GetPositionY() - cy, bot->GetPositionX() - cx);
    float aheadX = cx + std::cos(bearing - KITE_LEAD) * helper.kiteRadius25;
    float aheadY = cy + std::sin(bearing - KITE_LEAD) * helper.kiteRadius25;
    auto orbit = [this, aheadX, aheadY]()
    {
        // lessDelay=true: this re-targets a point that moves with us each tick,
        // so keep the path responsive instead of stuttering between orders.
        return MoveTo(NAXX_MAP_ID, aheadX, aheadY, bot->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true);
    };

    // Nearest chow to us, the "leak" (chow nearest Gluth — most likely to reach
    // him and get eaten, healing him 5%), and how many are piled on us.
    Unit* nearest = nullptr;
    float nearestDist = 1e9f;
    Unit* leak = nullptr;
    float leakDist = 1e9f;
    uint32 onMe = 0;
    for (Unit* z : chow)
    {
        float dBot = bot->GetExactDist2d(z);
        if (dBot < nearestDist) { nearestDist = dBot; nearest = z; }
        if (dBot <= helper.OT_HOLD_RADIUS) ++onMe;
        float dBoss = boss->GetExactDist2d(z);
        if (dBoss < leakDist) { leakDist = dBoss; leak = z; }
    }

    // ---- Off-tank: anchor the chow pack with AoE threat to keep it off Gluth;
    // once OT_KITE_THRESHOLD+ are piled on us, kite it so we aren't bursted. ----
    if (isOffTank)
    {
        if (onMe >= helper.OT_KITE_THRESHOLD)
            return orbit();
        // Hold out on the LEFT / RIGHT flanks of the ring so the chow stack on us,
        // away from Gluth, and so we cover the side lanes they were sneaking up.
        // The two off-tanks split OT_FLANK_OFFSET yards either side of center.
        float holdX = cx + (botAI->IsAssistTankOfIndex(bot, 1) ? helper.OT_FLANK_OFFSET : -helper.OT_FLANK_OFFSET);
        auto moveHold = [this, holdX, cy]()
        {
            return MoveTo(NAXX_MAP_ID, holdX, cy, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        };
        // Travel to the anchor first; only AoE-threat once we're actually there,
        // so a self-centered cast (Consecration etc.) doesn't strand us mid-room.
        if (bot->GetExactDist2d(holdX, cy) > 8.0f)
            return moveHold();
        if (nearest && CastZombieThreat(botAI, bot, nearest))
            return true;
        return moveHold();
    }

    // ---- Kiter (hunter / mage) ----
    if (chow.empty())
        return orbit();  // staged on the ring, waiting for the next spawn

    // --- Decimate burn: chow are at ~5% and sprinting Gluth. Root + AoE them
    // down on the ring before they reach him. ---
    if (helper.InDecimateBurn(chow))
    {
        if (isMage)
        {
            if (nearestDist <= MAGE_SNARE_RANGE && botAI->CastSpell("Frost Nova", bot))
                return true;
            if (nearest && nearestDist <= MAGE_SNARE_RANGE && botAI->CastSpell("Cone of Cold", nearest))
                return true;
            if (botAI->CastSpell("Arcane Explosion", bot))
                return true;
        }
        else
        {
            if (botAI->CastSpell("Frost Trap", bot))
                return true;
            if (nearest && botAI->CastSpell("Multi-Shot", nearest))
                return true;
            if (nearest && botAI->CastSpell("Concussive Shot", nearest))
                return true;
        }
        return orbit();
    }

    // --- Normal kite ---
    // Instant escape if a chow is about to land a melee swing.
    if (nearest && nearestDist < ESCAPE_DIST)
    {
        if (isMage && botAI->CastSpell("Blink", bot))
            return true;
        if (!isMage && botAI->CastSpell("Disengage", bot))
            return true;
    }

    // Snare on cooldown. Mages root/slow the pack chasing them (short range);
    // hunters snipe the leak heading for the boss (Concussive Shot, ~35y) and
    // drop a Frost Trap when the pack is on top of them.
    if (isMage)
    {
        // Bank Frost Nova (the AoE root, ~25s CD) for the Decimate burn above —
        // that's when we need the whole pack rooted in place to kill them before
        // Gluth eats them. Normal kiting gets by on Cone of Cold (a slow) plus the
        // Blink escape, so the root is always up when Decimate lands.
        if (nearest && nearestDist <= MAGE_SNARE_RANGE && botAI->CastSpell("Cone of Cold", nearest))
            return true;
    }
    else
    {
        if (leak && leakDist <= HUNTER_SNARE_RANGE && botAI->CastSpell("Concussive Shot", leak))
            return true;
        if (nearest && nearestDist <= HUNTER_TRAP_RANGE && botAI->CastSpell("Frost Trap", bot))
            return true;
    }

    return orbit();
}

bool GluthBurnAddsAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    // 25-man only. The chow handlers (kiters / off-tanks), the main tank and the
    // healers don't mop adds — everyone else's DPS does.
    if (bot->GetRaidDifficulty() != RAID_DIFFICULTY_25MAN_NORMAL)
        return false;
    if (helper.IsKiter(bot) || helper.IsZombieOffTank(bot) || botAI->IsMainTank(bot) || botAI->IsHeal(bot))
        return false;

    std::vector<Unit*> chow = helper.GetAliveZombieChow();
    if (!helper.InDecimateBurn(chow))
        return false;  // only turn off the boss during the burn window

    // Target the nearest LOW-HP (decimated, ~5%) chow within engage range. The
    // high-HP spawns are left to the kiters/off-tanks. Ranged catch them crossing
    // the room; melee only swing at ones basically on Gluth (they can't chase —
    // their assist movement is suppressed during this fight).
    float maxEngage = botAI->IsRangedDps(bot) ? BURN_RANGED_ENGAGE : BURN_MELEE_ENGAGE;
    Unit* target = nullptr;
    float best = 1e9f;
    for (Unit* z : chow)
    {
        if (z->GetHealthPct() > helper.decimatedZombiePct)
            continue;
        float d = bot->GetExactDist2d(z);
        if (d <= maxEngage && d < best) { best = d; target = z; }
    }
    if (!target)
        return false;  // nothing low-HP in range → stay on the boss

    if (context->GetValue<Unit*>("current target")->Get() == target)
        return false;  // already mopping it → let the rotation AoE it down

    return Attack(target, true);
}
