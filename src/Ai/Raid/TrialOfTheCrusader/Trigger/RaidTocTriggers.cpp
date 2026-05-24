#include "RaidTocTriggers.h"

#include <cmath>

#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RaidTocBossHelper.h"

namespace
{
// Nearest fellow spreader (a bot ranged DPS / healer) within `dist`, else null.
// Used purely to detect clumping; the human is excluded (they position
// themselves). Kept file-local in both the trigger and action TUs to avoid
// touching shared headers.
Player* NearestSpreaderWithin(PlayerbotAI* botAI, Player* bot, float dist)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* best = nullptr;
    float bestDist = dist;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || m == bot || !m->IsAlive())
            continue;
        if (!GET_PLAYERBOT_AI(m))  // ignore the human; they position themselves
            continue;
        if (botAI->IsTank(m))
            continue;
        if (!botAI->IsRanged(m) && !botAI->IsHeal(m))
            continue;
        float d = bot->GetExactDist2d(m);
        if (d < bestDist)
        {
            bestDist = d;
            best = m;
        }
    }
    return best;
}

// Gormok and the worm pair are the two phases we keep the raid spread for.
bool AnyEarlyBeastAlive(PlayerbotAI* botAI)
{
    return GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER) ||
           GetFirstAliveUnitByEntry(botAI, NPC_ACIDMAW) ||
           GetFirstAliveUnitByEntry(botAI, NPC_DREADSCALE);
}

// Impale stacks on a unit, difficulty-proof (id first, then name fallback).
uint32 ImpaleStacks(PlayerbotAI* botAI, Unit* unit)
{
    Aura* a = unit->GetAura(SPELL_IMPALE);
    if (!a)
        a = botAI->GetAura("impale", unit, false, true);
    return a ? a->GetStackAmount() : 0;
}

// Mirror of the helpers in RaidTocActions.cpp (kept file-local in both TUs so we
// don't disturb shared headers / the build glob).
Unit* WormTankedBy(PlayerbotAI* botAI, Player* bot)
{
    for (uint32 entry : { NPC_DREADSCALE, NPC_ACIDMAW })
    {
        Unit* worm = GetFirstAliveUnitByEntry(botAI, entry);
        if (worm && worm->GetVictim() == bot)
            return worm;
    }
    return nullptr;
}

void RaidBacklineCentroid(PlayerbotAI* botAI, Player* bot, Unit* worm, float& cx, float& cy)
{
    float sx = 0.0f, sy = 0.0f;
    int n = 0;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || m == bot || !m->IsAlive())
                continue;
            if (m->GetExactDist2d(worm) < TOC_WORM_MELEE_RANGE)
                continue;
            sx += m->GetPositionX();
            sy += m->GetPositionY();
            ++n;
        }
    }
    if (n == 0)
    {
        cx = TOC_CENTER_X;
        cy = TOC_CENTER_Y;
        return;
    }
    cx = sx / n;
    cy = sy / n;
}
}  // namespace

bool GormokNearFireBombTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    Creature* bomb = bot->FindNearestCreature(NPC_FIRE_BOMB, TOC_FIRE_BOMB_AVOID_RADIUS);
    return bomb != nullptr;
}

bool GormokSnoboldUpTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    // Only RANGED DPS peel to Snobolds. Melee stay glued to Gormok and let their
    // cleave/AoE chip the snobold (it gets run into the melee stack) — keeps boss
    // uptime high instead of bleeding melee DPS onto an add. Tanks hold Gormok and
    // healers keep healing. Exclude tanks explicitly — IsDps and IsTank aren't
    // mutually exclusive (a DPS-spec off-tank reads as both).
    if (botAI->IsTank(bot))
        return false;
    if (!botAI->IsDps(bot))
        return false;
    if (botAI->IsMelee(bot))
        return false;

    Creature* snobold = bot->FindNearestCreature(NPC_SNOBOLD_VASSAL, TOC_SNOBOLD_SEARCH_RADIUS);
    if (!snobold || !snobold->IsAlive())
        return false;

    // Already on a snobold? Don't re-trigger.
    Unit* currentTarget = botAI->GetUnit(bot->GetTarget());
    if (currentTarget && currentTarget->GetEntry() == NPC_SNOBOLD_VASSAL)
        return false;

    return true;
}

bool GormokImpaleTankSwapTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    // Only an off-duty tank taunts: I'm a tank, but Gormok isn't on me.
    if (!botAI->IsTank(bot))
        return false;

    Unit* victim = boss->GetVictim();
    if (!victim || victim == bot)
        return false;

    // The current tank must be another (player) tank.
    Player* victimPlayer = victim->ToPlayer();
    if (!victimPlayer || !botAI->IsTank(victimPlayer))
        return false;

    // (Impale uses per-difficulty spell ids, so ImpaleStacks falls back to the
    //  name lookup.) Don't swap below the danger threshold...
    uint32 victimStacks = ImpaleStacks(botAI, victim);
    if (victimStacks < TOC_IMPALE_SWAP_STACKS)
        return false;

    // ...and only take the boss back once my OWN Impale has fully fallen off.
    // Taunting while I still carry stacks just re-bleeds me on top of a
    // half-decayed stack and risks yanking aggro back before the active tank's
    // bleed has dropped — exactly the ping-pong we want to avoid.
    if (ImpaleStacks(botAI, bot) > 0)
        return false;

    return true;
}

bool GormokOffTankBackoffTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    // I'm an off-duty tank (Gormok is on another tank).
    if (!botAI->IsTank(bot))
        return false;
    Unit* victim = boss->GetVictim();
    if (!victim || victim == bot)
        return false;
    Player* victimPlayer = victim->ToPlayer();
    if (!victimPlayer || !botAI->IsTank(victimPlayer))
        return false;

    // Stop building threat for as long as I carry ANY Impale stack. The boss
    // must stay on the active tank until my bleed has fully fallen off, so I
    // never re-damage or snap aggro back mid-stack. Once I'm clean (0 stacks)
    // I resume threat and become eligible to taunt the next swap.
    return ImpaleStacks(botAI, bot) > 0;
}

bool GormokTankOffSnoboldTrigger::IsActive()
{
    if (!botAI->IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "gormok the impaler");
    if (!boss || !boss->IsAlive())
        return false;

    // Tanks never chase Snobolds — if one slipped onto my target, get back on Gormok.
    // (Melee DPS are kept on Gormok by the melee-focus rule + dps-assist suppression
    // instead, so they don't thrash their rotation here.)
    Unit* target = botAI->GetUnit(bot->GetTarget());
    return target && target->GetEntry() == NPC_SNOBOLD_VASSAL;
}

bool GormokMeleeFocusBossTrigger::IsActive()
{
    // Melee DPS stay glued to Gormok the whole phase (ranged peel to Snobolds).
    // Exclude tanks explicitly — IsDps/IsTank aren't mutually exclusive.
    if (botAI->IsTank(bot))
        return false;
    if (!botAI->IsDps(bot) || !botAI->IsMelee(bot))
        return false;

    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER);
    if (!boss)
        return false;

    // Self-limiting: only fires to (re)acquire Gormok. Once we're on him this goes
    // quiet and the rotation runs — provided nothing re-pulls us (the keep-melee-on-
    // boss multiplier suppresses generic dps-assist so it can't).
    return bot->GetTarget() != boss->GetGUID();
}

bool GormokSpreadTrigger::IsActive()
{
    if (!AnyEarlyBeastAlive(botAI))
        return false;

    // Tanks/melee stay on the boss; only ranged DPS and healers fan out.
    if (botAI->IsTank(bot))
        return false;
    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
        return false;

    // Spacing-based: only reposition when actually clumped with another
    // spreader. Once everyone is apart this goes quiet and bots just DPS;
    // avoid-aoe (prio 90) outranks spread (60), so dodging always wins.
    return NearestSpreaderWithin(botAI, bot, TOC_SPREAD_MIN_DIST) != nullptr;
}

bool GormokSnobbledRunInTrigger::IsActive()
{
    // 66406 (Snobbled) is removed from the rider when the snobold dies, so its
    // presence reliably means a snobold is riding me right now.
    if (!bot->HasAura(SPELL_SNOBOLLED))
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER) != nullptr;
}

bool WormsFocusDreadscaleTrigger::IsActive()
{
    // Burn Dreadscale first: all DPS prioritize it until it's dead, then the
    // raid rolls onto (the now-enraged) Acidmaw. Tanks keep their worms; healers
    // heal — only DPS target choice matters here. (Exclude tanks explicitly:
    // IsDps and IsTank aren't mutually exclusive for a DPS-spec off-tank.)
    if (botAI->IsTank(bot))
        return false;
    if (!botAI->IsDps(bot))
        return false;

    Unit* dreadscale = GetFirstAliveUnitByEntry(botAI, NPC_DREADSCALE);
    if (!dreadscale)
        return false;

    // Already locked on — don't re-issue the switch every tick.
    return bot->GetTarget() != dreadscale->GetGUID();
}

bool WormsParalyticToxinTrigger::IsActive()
{
    // Name lookup is difficulty-proof (Paralytic Toxin is 66823/67618/67619/67620).
    if (!botAI->HasAura("paralytic toxin", bot))
        return false;

    // Only worth moving if some ally is carrying Burning Bile to burn it off.
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && botAI->HasAura("burning bile", member))
            return true;
    }
    return false;
}

bool WormsSlimePoolTrigger::IsActive()
{
    Creature* pool = bot->FindNearestCreature(NPC_SLIME_POOL, TOC_SLIME_POOL_AVOID_RADIUS);
    return pool != nullptr;
}

bool WormsTankFaceAwayTrigger::IsActive()
{
    if (!botAI->IsTank(bot))
        return false;

    Unit* worm = WormTankedBy(botAI, bot);
    if (!worm)
        return false;

    float cx = TOC_CENTER_X, cy = TOC_CENTER_Y;
    RaidBacklineCentroid(botAI, bot, worm, cx, cy);

    // The worm faces its tank (us). It's dangerous when the backline sits in the
    // same hemisphere as us relative to the worm — i.e. the cone toward us also
    // sweeps them. Reposition only then (otherwise we'd micro-dance).
    float fx = bot->GetPositionX() - worm->GetPositionX();
    float fy = bot->GetPositionY() - worm->GetPositionY();
    float rx = cx - worm->GetPositionX();
    float ry = cy - worm->GetPositionY();
    float fl = std::sqrt(fx * fx + fy * fy);
    float rl = std::sqrt(rx * rx + ry * ry);
    if (fl < 1.0f || rl < 1.0f)
        return false;

    float dot = (fx * rx + fy * ry) / (fl * rl);
    return dot > TOC_WORM_FACE_DANGER_DOT;
}

bool WormsAvoidBurningBileTrigger::IsActive()
{
    // Ranged/healers should stay out of the 10y Burning Bile fire AoE. Melee and
    // the tank can't (they're on the worm); toxin'd players must be IN it to get
    // cured (the cure action, higher priority, handles them) — so exclude both.
    if (botAI->IsTank(bot))
        return false;
    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
        return false;
    if (botAI->HasAura("paralytic toxin", bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || m == bot || !m->IsAlive())
            continue;
        if (botAI->HasAura("burning bile", m) && bot->GetExactDist2d(m) < TOC_BILE_AVOID_RADIUS)
            return true;
    }
    return false;
}

bool IcehowlChargeTrigger::IsActive()
{
    // Surge of Adrenaline is cast on every player the instant Icehowl readies his
    // charge (25N), and the speed buff lasts the charge — perfect "clear now" cue.
    if (!bot->HasAura(SPELL_SURGE_OF_ADRENALINE))
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_ICEHOWL) != nullptr;
}

bool JaraxxusNetherPowerTrigger::IsActive()
{
    // Mages only — Spellsteal is what lifts Nether Power (and grants the mage a
    // copy). Other classes have no business reacting to this.
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_JARAXXUS);
    if (!boss)
        return false;

    // Name lookup is difficulty-proof (Nether Power is 66228/67106/67107/67108).
    if (!botAI->HasAura("nether power", boss))
        return false;

    // Let CanCastSpell gate range / line-of-sight / mana / the GCD, so this fires
    // once per GCD while stacks remain and goes quiet the instant they're gone.
    return botAI->CanCastSpell("spellsteal", boss);
}

bool JaraxxusLegionFlameTrigger::IsActive()
{
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_JARAXXUS);
    if (!boss)
        return false;

    Creature* flame = bot->FindNearestCreature(NPC_LEGION_FLAME, TOC_LEGION_FLAME_AVOID_RADIUS);
    return flame != nullptr;
}

bool GormokImpaleSelfBopTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    // Only the active tank ever climbs to a dangerous Impale stack.
    Aura* impale = botAI->GetAura("impale", bot, false, true);
    if (!impale || impale->GetStackAmount() < TOC_IMPALE_BOP_STACKS)
        return false;

    if (botAI->HasAura("hand of protection", bot))
        return false; // already up

    return botAI->CanCastSpell("hand of protection", bot);
}

bool GormokRemoveSelfBopTrigger::IsActive()
{
    if (bot->getClass() != CLASS_PALADIN)
        return false;
    if (!botAI->HasAura("hand of protection", bot))
        return false;

    // Only auto-strip BoP during the Gormok fight, where we self-cast it purely
    // to wipe Impale; elsewhere a genuine BoP must be left alone.
    return GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER) != nullptr;
}
