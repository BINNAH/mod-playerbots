#include "JsonStrategyTriggers.h"

#include "JsonStrategyShapeUtil.h"
#include "Playerbots.h"

#include <algorithm>
#include <cctype>

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

// Match `bot` against a comma-separated role list (OR semantics).
static bool RoleMatches(PlayerbotAI* botAI, Player* bot, std::string const& roleCsv)
{
    if (roleCsv.empty())
        return true;

    for (std::string const& role : JsonSplit(roleCsv, ','))
    {
        if (role == "all")
            return true;
        else if (role == "maintank" && botAI->IsMainTank(bot))
            return true;
        else if (role == "offtank" && botAI->IsAssistTank(bot))
            return true;
        else if (role == "tank" && botAI->IsTank(bot))
            return true;
        else if (role == "notmaintank" && !botAI->IsMainTank(bot))
            return true;
        else if (role == "nontank" && !botAI->IsTank(bot))
            return true;
        else if (role == "ranged" && botAI->IsRanged(bot))
            return true;
        else if (role == "melee" && botAI->IsMelee(bot))
            return true;
        else if (role == "healer" && botAI->IsHeal(bot))
            return true;
        else if (role == "dps" && botAI->IsDps(bot))
            return true;
    }
    return false;
}

bool JsonEncounterActiveTrigger::IsActive()
{
    if (qualifier.empty())
        return false;

    std::string boss, role, aura;
    bool requirePresent = true;
    bool haveAura = false;
    bool includeCast = true;

    if (qualifier.find('=') == std::string::npos)
    {
        // Bare boss name (back-compat / simplest form).
        boss = qualifier;
    }
    else
    {
        boss = JsonKv(qualifier, "boss");
        role = JsonKv(qualifier, "role");
        aura = JsonKv(qualifier, "aura");
        if (!aura.empty())
        {
            haveAura = true;
            requirePresent = JsonKv(qualifier, "has", "1") != "0";
            includeCast = JsonKv(qualifier, "cast", "1") != "0";
        }
    }

    if (boss.empty())
        return false;

    Unit* bossUnit = AI_VALUE2(Unit*, "find target", boss);
    if (!bossUnit)
        return false;

    if (!RoleMatches(botAI, bot, role))
        return false;

    if (haveAura)
    {
        // Count the phase as "present" from the moment the boss starts casting
        // it (not just once the aura lands), so the raid reacts on cast-start.
        bool present = botAI->HasAura(aura, bossUnit) || (includeCast && BossCastingNamed(bossUnit, aura));
        if (present != requirePresent)
            return false;
    }

    return true;
}
