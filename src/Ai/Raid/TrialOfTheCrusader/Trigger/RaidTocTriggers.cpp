#include "RaidTocTriggers.h"

#include <cmath>

#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RaidTocBossHelper.h"

namespace
{
// Evenly place the alive ranged DPS + healers on a ring centred on the arena,
// assigning each a unique slot in GUID order (no collisions, spacing adapts to
// headcount). Returns false if this bot isn't one of the spreaders. Kept file-
// local in both the trigger and action TUs to avoid touching shared headers.
bool ComputeBeastSpreadPos(PlayerbotAI* botAI, Player* bot, float radius, float& outX, float& outY)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid myGuid = bot->GetGUID();
    uint32 total = 0;
    uint32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || !m->IsAlive())
            continue;
        if (!GET_PLAYERBOT_AI(m))  // skip the human; bots own the spread ring
            continue;
        if (botAI->IsTank(m))
            continue;
        if (!botAI->IsRanged(m) && !botAI->IsHeal(m))
            continue;
        ++total;
        if (m->GetGUID() < myGuid)
            ++index;
    }
    if (total == 0)
        return false;

    float angle = index * (TOC_TWO_PI / total);
    outX = TOC_CENTER_X + std::cos(angle) * radius;
    outY = TOC_CENTER_Y + std::sin(angle) * radius;
    return true;
}

// Gormok and the worm pair are the two phases we keep the raid spread for.
bool AnyEarlyBeastAlive(PlayerbotAI* botAI)
{
    return GetFirstAliveUnitByEntry(botAI, NPC_GORMOK_THE_IMPALER) ||
           GetFirstAliveUnitByEntry(botAI, NPC_ACIDMAW) ||
           GetFirstAliveUnitByEntry(botAI, NPC_DREADSCALE);
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

    // All DPS (melee + ranged) peel to Snobolds so they die fast; tanks hold
    // Gormok and healers keep healing.
    if (!botAI->IsDps(bot))
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

    // Impale's stacking aura uses per-difficulty spell ids (66331 is only the
    // base/10N variant; 25N etc. are substituted via SpellDifficulty.dbc), so
    // try the id then fall back to the difficulty-proof name lookup.
    Aura* victimImpale = victim->GetAura(SPELL_IMPALE);
    if (!victimImpale)
        victimImpale = botAI->GetAura("impale", victim, false, true);
    uint32 victimStacks = victimImpale ? victimImpale->GetStackAmount() : 0;
    if (victimStacks < TOC_IMPALE_SWAP_STACKS)
        return false; // not dangerous yet — let the active tank hold

    // Only take over when the active tank is meaningfully more loaded than me.
    // (Taunting purely on a stack threshold makes the two tanks ping-pong the
    //  boss every taunt once both are stacked.)
    Aura* myImpale = bot->GetAura(SPELL_IMPALE);
    if (!myImpale)
        myImpale = botAI->GetAura("impale", bot, false, true);
    uint32 myStacks = myImpale ? myImpale->GetStackAmount() : 0;
    if (victimStacks < myStacks + TOC_IMPALE_SWAP_LEAD)
        return false;

    return true;
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

    float x, y;
    if (!ComputeBeastSpreadPos(botAI, bot, TOC_SPREAD_RADIUS, x, y))
        return false;
    return bot->GetExactDist2d(x, y) > TOC_SPREAD_TOLERANCE;
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

bool IcehowlChargeTrigger::IsActive()
{
    // Surge of Adrenaline is cast on every player the instant Icehowl readies his
    // charge (25N), and the speed buff lasts the charge — perfect "clear now" cue.
    if (!bot->HasAura(SPELL_SURGE_OF_ADRENALINE))
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_ICEHOWL) != nullptr;
}
