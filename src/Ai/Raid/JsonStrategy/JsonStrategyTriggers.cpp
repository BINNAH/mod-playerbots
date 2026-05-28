#include "JsonStrategyTriggers.h"

#include "JsonStrategyLoader.h"  // RaidJsonMode (manual_engage flag)
#include "JsonStrategyShapeUtil.h"
#include "Playerbots.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

static bool IEquals(std::string a, std::string b)
{
    if (a.size() != b.size())
        return false;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return a == b;
}

// True while `boss` is mid-cast on a (generic or channeled) spell named `name`.
// Lets a phase react at cast-start, before the resulting aura is applied.
static bool BossCastingNamed(Unit* boss, std::string const& name)
{
    for (CurrentSpellTypes slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        if (Spell* spell = boss->GetCurrentSpell(slot))
        {
            if (SpellInfo const* info = spell->GetSpellInfo())
            {
                if (IEquals(std::string(info->SpellName[0]), name))
                    return true;
            }
        }
    }
    return false;
}

// Evaluate a single role token against `bot`. Unknown token -> false.
static bool RoleToken(PlayerbotAI* botAI, Player* bot, std::string const& role)
{
    if (role == "all")         return true;
    if (role == "maintank")    return botAI->IsMainTank(bot);
    if (role == "offtank")     return botAI->IsAssistTank(bot);
    if (role == "offtank1")    return botAI->IsAssistTankOfIndex(bot, 0);
    if (role == "offtank2")    return botAI->IsAssistTankOfIndex(bot, 1);
    if (role == "offtank3")    return botAI->IsAssistTankOfIndex(bot, 2);
    if (role == "tank")        return botAI->IsTank(bot);
    if (role == "notmaintank") return !botAI->IsMainTank(bot);
    if (role == "nontank")     return !botAI->IsTank(bot);
    if (role == "ranged")      return botAI->IsRanged(bot);
    if (role == "melee")       return botAI->IsMelee(bot);
    if (role == "healer")      return botAI->IsHeal(bot);
    if (role == "dps")         return botAI->IsDps(bot);
    return false;
}

// Match `bot` against a comma-separated role list. Positive tokens are OR-ed (at
// least one must match); tokens prefixed with `!` are EXCLUSIONS, AND-ed in (each
// must be false). This is essential because the predicates overlap: IsMelee is just
// !IsRanged, so a melee TANK is `melee` too -- without `!tank` a `"melee"` rule also
// catches both tanks, and on Thaddius the main tank then matched both `maintank`
// (-> Stalagg, left) and `melee` (-> Feugen, right) and ping-ponged through center.
// So "melee DPS only" is `"melee,!tank,!healer"`. A list of only exclusions (e.g.
// `"!tank"`) matches anyone who trips none of them. Empty list = match all.
static bool RoleMatches(PlayerbotAI* botAI, Player* bot, std::string const& roleCsv)
{
    if (roleCsv.empty())
        return true;

    bool hasPositive = false;
    bool positiveHit = false;
    for (std::string token : JsonSplit(roleCsv, ','))
    {
        bool negate = (!token.empty() && token[0] == '!');
        if (negate)
            token = token.substr(1);

        bool match = RoleToken(botAI, bot, token);
        if (negate)
        {
            if (match)
                return false;  // a forbidden role -> reject outright
        }
        else
        {
            hasPositive = true;
            if (match)
                positiveHit = true;
        }
    }
    // Passed every exclusion; require a positive hit only if positives were listed.
    return !hasPositive || positiveHit;
}

// Stable role-split partition. `split` is "i/n": divide the BOTS in the group that
// match `role` into n contiguous partitions (by the group's member order, which is
// stable across ticks) and fire only for the i-th (1-indexed). This sends one role
// to two targets -- e.g. healers `split:"1/2"` -> left add, `split:"2/2"` -> right --
// which a single role token can't express. Each member is classified with its OWN
// bot AI, so the human (no PlayerbotAI) isn't counted and the BOTS divide evenly.
// Dead bots still count (the side assignment stays put when someone dies, instead of
// the survivors re-shuffling sides mid-fight). Returns true (no-op) when `split` is
// absent or malformed.
static bool RoleSplitMatches(Player* bot, std::string const& role, std::string const& split)
{
    if (split.empty())
        return true;
    int idx = 0, parts = 0;
    if (std::sscanf(split.c_str(), "%d/%d", &idx, &parts) != 2 || parts < 1 || idx < 1 || idx > parts)
        return true;  // malformed -> don't filter

    Group* group = bot->GetGroup();
    if (!group)
        return idx == 1;  // ungrouped lone bot takes the first partition

    int myRank = -1, count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !RoleMatches(memberAI, member, role))
            continue;
        if (member == bot)
            myRank = count;
        ++count;
    }
    if (myRank < 0 || count == 0)
        return false;
    // Contiguous partitioning: rank r of c -> partition floor(r*parts/c), 0-based.
    return ((myRank * parts) / count) + 1 == idx;
}

// Match `bot` against a comma-separated class list (OR semantics). Empty = any.
// Lets a rule target e.g. only mages/hunters (the Gluth chow kiters).
static bool ClassMatches(Player* bot, std::string const& classCsv)
{
    if (classCsv.empty())
        return true;

    uint8 c = bot->getClass();
    for (std::string const& cls : JsonSplit(classCsv, ','))
    {
        if ((cls == "warrior" && c == CLASS_WARRIOR) || (cls == "paladin" && c == CLASS_PALADIN) ||
            (cls == "hunter" && c == CLASS_HUNTER) || (cls == "rogue" && c == CLASS_ROGUE) ||
            (cls == "priest" && c == CLASS_PRIEST) || (cls == "shaman" && c == CLASS_SHAMAN) ||
            (cls == "mage" && c == CLASS_MAGE) || (cls == "warlock" && c == CLASS_WARLOCK) ||
            (cls == "druid" && c == CLASS_DRUID) ||
            ((cls == "deathknight" || cls == "dk") && c == CLASS_DEATH_KNIGHT))
            return true;
    }
    return false;
}

// Match `unit` against `token`: an all-digit token compares the creature entry
// id, anything else the (case-insensitive) creature name. (Mirrors the same
// helper in JsonStrategyActions.cpp; kept file-local to each translation unit.)
static bool MatchesNameOrEntry(PlayerbotAI* botAI, Unit* unit, std::string const& token)
{
    if (token.empty())
        return false;
    if (token.find_first_not_of("0123456789") == std::string::npos)
        return unit->GetEntry() == (uint32)std::strtoul(token.c_str(), nullptr, 10);
    return botAI->EqualLowercaseName(unit->GetName(), token);
}

// First living NPC matching `token` in an already-fetched proximity list (which
// is distance-sorted, so first ~= nearest). Threat-independent. nullptr if none.
static Unit* FindNearbyNamed(PlayerbotAI* botAI, GuidVector const& npcs, std::string const& token)
{
    if (token.empty())
        return nullptr;
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && MatchesNameOrEntry(botAI, unit, token))
            return unit;
    }
    return nullptr;
}

bool JsonEncounterActiveTrigger::IsActive()
{
    if (qualifier.empty())
        return false;

    std::string boss, role, klass, aura, selfAura, detect;
    bool requirePresent = true;
    bool haveAura = false;
    bool includeCast = true;
    bool selfRequirePresent = true;
    bool haveSelfAura = false;

    if (qualifier.find('=') == std::string::npos)
    {
        // Bare boss name (back-compat / simplest form).
        boss = qualifier;
    }
    else
    {
        boss = JsonKv(qualifier, "boss");
        role = JsonKv(qualifier, "role");
        klass = JsonKv(qualifier, "class");
        detect = JsonKv(qualifier, "detect");
        aura = JsonKv(qualifier, "aura");
        if (!aura.empty())
        {
            haveAura = true;
            requirePresent = JsonKv(qualifier, "has", "1") != "0";
            includeCast = JsonKv(qualifier, "cast", "1") != "0";
        }
        // Self-aura phase gate: an aura on the BOT (not the boss) — e.g. carrying
        // Mutating Injection / a polarity charge / a spore. The C++ analog is a
        // HasAuraTrigger/HasNoAuraTrigger on self (MutatingInjection*Trigger).
        selfAura = JsonKv(qualifier, "self");
        if (!selfAura.empty())
        {
            haveSelfAura = true;
            selfRequirePresent = JsonKv(qualifier, "selfhas", "1") != "0";
        }
    }

    if (boss.empty())
        return false;

    // detect=nearest finds the boss by a proximity scan instead of the threat
    // list, so the rule fires even for a bot that never threatens him (a kiter /
    // off-tank who only ever touches the adds). Default stays threat-based.
    Unit* bossUnit = (detect == "nearest")
        ? FindNearbyNamed(botAI, AI_VALUE(GuidVector, "nearest npcs"), boss)
        : AI_VALUE2(Unit*, "find target", boss);
    if (!bossUnit)
        return false;

    if (!RoleMatches(botAI, bot, role))
        return false;

    if (!ClassMatches(bot, klass))
        return false;

    if (haveAura)
    {
        // Count the phase as "present" from the moment the boss starts casting
        // it (not just once the aura lands), so the raid reacts on cast-start.
        bool present = botAI->HasAura(aura, bossUnit) || (includeCast && BossCastingNamed(bossUnit, aura));
        if (present != requirePresent)
            return false;
    }

    if (haveSelfAura)
    {
        bool present = botAI->HasAura(selfAura, bot);
        if (present != selfRequirePresent)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// adds_near
// ---------------------------------------------------------------------------
bool JsonAddsNearTrigger::IsActive()
{
    std::string add = JsonKv(qualifier, "add");
    if (add.empty())
        return false;

    if (!RoleMatches(botAI, bot, JsonKv(qualifier, "role")))
        return false;

    if (!ClassMatches(bot, JsonKv(qualifier, "class")))
        return false;

    float range = (float)std::atof(JsonKv(qualifier, "range", "0").c_str());
    uint32 count = (uint32)std::strtoul(JsonKv(qualifier, "count", "1").c_str(), nullptr, 10);
    bool ofBoss = JsonKv(qualifier, "of") == "boss";

    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");

    // Measure from the bot (of=self) or the boss (of=boss). The boss is found by
    // proximity too, so this whole trigger is threat-independent.
    WorldObject* ref = bot;
    if (ofBoss)
    {
        Unit* bossUnit = FindNearbyNamed(botAI, npcs, JsonKv(qualifier, "boss"));
        if (!bossUnit)
            return false;
        ref = bossUnit;
    }

    uint32 found = 0;
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !MatchesNameOrEntry(botAI, unit, add))
            continue;
        if (range > 0.0f && ref->GetExactDist2d(unit) > range)
            continue;
        if (++found >= count)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// target_hp_ahead
// ---------------------------------------------------------------------------
bool JsonTargetHpAheadTrigger::IsActive()
{
    std::string others = JsonKv(qualifier, "others");
    if (others.empty())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    // Only throttle once my add is in the home stretch (the C++ multiplier's
    // <=40% gate). Default 100 keeps it always active.
    float myHp = target->GetHealthPct();
    float below = (float)std::atof(JsonKv(qualifier, "below", "100").c_str());
    if (myHp > below)
        return false;

    float margin = (float)std::atof(JsonKv(qualifier, "margin", "0").c_str());

    // Threat-independent: read the OTHER adds by proximity, not the threat list,
    // so a bot tanking/DPSing only one add can still compare to the other's HP.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (std::string const& token : JsonSplit(others, ','))
    {
        Unit* other = FindNearbyNamed(botAI, npcs, token);
        if (other && other != target && other->GetHealthPct() >= myHp + margin)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// target_victim
// ---------------------------------------------------------------------------
bool JsonTargetVictimTrigger::IsActive()
{
    std::string role = JsonKv(qualifier, "role");
    if (role.empty())
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;
    Unit* victim = target->GetVictim();
    if (!victim)
        return false;
    Player* vp = victim->ToPlayer();
    if (!vp)
        return false;  // a pet/NPC has it -> not "handled by a <role>"
    PlayerbotAI* vai = GET_PLAYERBOT_AI(vp);
    if (!vai)
        return false;  // the human (or any non-bot) -> let the taunt still recover the add
    return RoleMatches(vai, vp, role);
}

// ---------------------------------------------------------------------------
// manual_engage
// ---------------------------------------------------------------------------
bool JsonManualEngageTrigger::IsActive()
{
    // Only while the raid leader has called the pull (`.rjson pull`).
    if (!RaidJsonMode::instance().IsEngaged(bot->GetGUID()))
        return false;

    // NOTE: the pull stays active in combat ON PURPOSE; the hand-off to the in-combat
    // C++ is done inside the `move_to_target` ACTION (its reached-latch), not here. An
    // earlier `if (bot->IsInCombat()) return false;` here was wrong: a bot flagged in
    // combat early (off-tank taunt / Static Field AoE) while still on the ramp lost
    // its climb action and handed off to the C++ -- which can't climb -- and got stuck
    // at the bottom. The latch hands off only once the bot has REACHED its add.

    std::string role = JsonKv(qualifier, "role");
    if (!RoleMatches(botAI, bot, role))
        return false;

    // Optional "i/n" split: divide same-role bots across two adds (e.g. healers
    // half to each platform). Empty/malformed = no split.
    if (!RoleSplitMatches(bot, role, JsonKv(qualifier, "split")))
        return false;

    // Optional add gate: only while the assigned add is alive AND (if `range` is
    // set) within `range` yards. The proximity gate is the pathing fix: while the
    // bot is FAR from its add it stays out of this rule, so it just FOLLOWS the
    // master up the ramp (the navmesh can't path the long low->high climb on its
    // own — it straight-lines through the slime — but follow rides the leader's
    // route). It only breaks off to engage once you've led it close. It also
    // auto-hands-off on Magnetic Pull: yanked away from its add, the bot drops
    // out of range here and the in-combat nearest-pet rule retargets it to the
    // add it was pulled onto. Detection is a proximity scan (threat-independent).
    std::string add = JsonKv(qualifier, "add");
    if (!add.empty())
    {
        Unit* a = FindNearbyNamed(botAI, AI_VALUE(GuidVector, "nearest npcs"), add);
        if (!a)
            return false;
        float range = (float)std::atof(JsonKv(qualifier, "range", "0").c_str());
        if (range > 0.0f && bot->GetExactDist2d(a) > range)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// pre_cast_window
// ---------------------------------------------------------------------------
void JsonPreCastWindowTrigger::Qualify(std::string const qual)
{
    Qualified::Qualify(qual);
    _boss = JsonKv(qual, "boss");
    _spell = JsonKv(qual, "spell");
    _spellIsId = !_spell.empty() && _spell.find_first_not_of("0123456789") == std::string::npos;
    _spellId = _spellIsId ? (uint32)std::strtoul(_spell.c_str(), nullptr, 10) : 0;
    _aura = JsonKv(qual, "aura");
    _role = JsonKv(qual, "role");
    _interval = (uint32)std::strtoul(JsonKv(qual, "interval", "0").c_str(), nullptr, 10);
    _firstAt = (uint32)std::strtoul(JsonKv(qual, "first", std::to_string(_interval)).c_str(), nullptr, 10);
    _lead = (uint32)std::strtoul(JsonKv(qual, "lead", "4000").c_str(), nullptr, 10);
    _tail = (uint32)std::strtoul(JsonKv(qual, "tail", "6000").c_str(), nullptr, 10);
}

// Rising-edge source for re-anchoring the clock: the boss mid-cast of the
// anchor spell, or its resulting aura already on this bot. The bot-aura path is
// the reliable signal for an instant/short cast like Web Spray's stun (which a
// pure GetCurrentSpell check can miss), and mirrors the original C++ helper
// that anchored on HasAura(SPELL_WEB_SPRAY, bot).
bool JsonPreCastWindowTrigger::AnchorObserved(Unit* boss)
{
    if (_spell.empty())
        return false;

    if (_spellIsId)
    {
        if (botAI->HasAura(_spellId, bot))
            return true;
        for (CurrentSpellTypes slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
            if (Spell* s = boss->GetCurrentSpell(slot))
                if (SpellInfo const* info = s->GetSpellInfo())
                    if (info->Id == _spellId)
                        return true;
        return false;
    }

    return botAI->HasAura(_spell, bot) || BossCastingNamed(boss, _spell);
}

bool JsonPreCastWindowTrigger::IsActive()
{
    if (_boss.empty() || _interval == 0)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", _boss);
    if (!boss)
    {
        // Reset between attempts so a fresh pull re-predicts from combat start.
        _combatStartMs = 0;
        _nextCastMs = 0;
        _castSeen = false;
        return false;
    }

    // Advance the predicted clock whenever the boss is up — independent of the
    // role/aura gates below, so the prediction stays accurate for every bot and
    // is already correct by the time the phase (e.g. frenzy) opens.
    uint32 now = getMSTime();
    if (_combatStartMs == 0)
    {
        _combatStartMs = now;
        _nextCastMs = now + _firstAt;
    }
    bool observed = AnchorObserved(boss);
    if (observed && !_castSeen)
        _nextCastMs = now + _interval;  // snap to the real cast, killing drift
    _castSeen = observed;

    if (!RoleMatches(botAI, bot, _role))
        return false;

    if (!_aura.empty() && !botAI->HasAura(_aura, boss))
        return false;

    if (_nextCastMs == 0)
        return false;

    // Open `lead` before the prediction, hold `tail` past it (covers a cast
    // delayed by an in-progress boss ability); the re-anchor closes it cleanly.
    return now + _lead >= _nextCastMs && now <= _nextCastMs + _tail;
}
