#ifndef _PLAYERBOT_RAIDNAXXBOSSHELPER_H
#define _PLAYERBOT_RAIDNAXXBOSSHELPER_H

#include <string>

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "Group.h"
#include "Log.h"
#include "NamedObjectContext.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"
#include "RaidNaxxSpellIds.h"

const uint32 NAXX_MAP_ID = 533;

// Noth the Plaguebringer's summoned adds (entries from boss_noth.cpp). The
// off-tank gathers these and drags them onto the boss for the raid to cleave.
namespace NothAddEntries
{
    constexpr uint32 PlaguedWarrior = 16984;
    constexpr uint32 PlaguedChampion = 16983;
    constexpr uint32 PlaguedGuardian = 16981;

    inline bool IsNothAdd(uint32 entry)
    {
        return entry == PlaguedWarrior || entry == PlaguedChampion || entry == PlaguedGuardian;
    }
}

// True while Heigan is in his fast-dance phase. Used by triggers (to pick
// the right strategy node) and by the dance action (to pick timing
// constants). Three checks cover the full window: TeleportSelf aura at
// phase entry, PlagueCloud aura during the ~45s channel, and an in-flight
// cast of either to catch the 1-second gap between TeleportSelf and the
// scheduled PlagueCloud.
inline bool HeiganIsFastDancing(PlayerbotAI* botAI, Unit* heigan)
{
    if (!heigan)
        return false;

    if (botAI->HasAura(NaxxSpellIds::TeleportSelf, heigan))
        return true;

    if (botAI->HasAura(NaxxSpellIds::PlagueCloud, heigan))
        return true;

    if (heigan->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* spell = heigan->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = heigan->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (spell && NaxxSpellIds::MatchesAnySpellId(spell->GetSpellInfo(),
                {NaxxSpellIds::PlagueCloud, NaxxSpellIds::TeleportSelf}))
            return true;
    }

    return false;
}

template <class BossAiType>
class GenericBossHelper : public AiObject
{
public:
    GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
    virtual bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            _unit = nullptr;

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            _unit = nullptr;

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", _name);
            if (!_unit)
                return false;

            _target = _unit->ToCreature();
            if (!_target)
                return false;

            _ai = dynamic_cast<BossAiType*>(_target->GetAI());
            if (!_ai)
                return false;

            _event_map = &_ai->events;
            if (!_event_map)
                return false;
        }
        if (!_event_map)
            return false;

        _timer = getMSTime();
        return true;
    }
    virtual void Reset()
    {
        _unit = nullptr;
        _target = nullptr;
        _ai = nullptr;
        _event_map = nullptr;
        _timer = 0;
    }

protected:
    std::string _name;
    Unit* _unit = nullptr;
    Creature* _target = nullptr;
    BossAiType* _ai = nullptr;
    EventMap* _event_map = nullptr;
    uint32 _timer = 0;
};

class KelthuzadBossHelper : public AiObject
{
public:
    KelthuzadBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    const std::pair<float, float> center = {3716.19f, -5106.58f};
    const std::pair<float, float> tank_pos = {3709.19f, -5104.86f};
    const std::pair<float, float> assist_tank_pos = {3746.05f, -5112.74f};
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "kel'thuzad");

        return _unit != nullptr;
    }
    bool IsPhaseOne() { return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    bool IsPhaseTwo() { return _unit && !_unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE); }
    Unit* GetAnyShadowFissure()
    {
        Unit* shadow_fissure = nullptr;
        GuidVector units = *context->GetValue<GuidVector>("nearest triggers");
        for (auto i = units.begin(); i != units.end(); i++)
        {
            Unit* unit = botAI->GetUnit(*i);
            if (!unit)
                continue;
            if (botAI->EqualLowercaseName(unit->GetName(), "shadow fissure"))
                shadow_fissure = unit;
        }
        return shadow_fissure;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class RazuviousBossHelper : public AiObject
{
public:
    RazuviousBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "instructor razuvious");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class SapphironBossHelper : public AiObject
{
public:
    // Tank spot pulled up to room center (~22y north of the old {3512.07,
    // -5274.06}). The old spot sat near the south entrance, so the boss —
    // chasing the tank — faced the entrance and was tanked right on top of the
    // doorway. Holding it here keeps Sapphiron off the entrance.
    const std::pair<float, float> mainTankPos = {3518.64f, -5252.45f};
    const std::pair<float, float> center = {3517.31f, -5253.74f};
    const float GENERIC_HEIGHT = 137.29f;
    SapphironBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "sapphiron");
            if (!_unit)
                return false;
        }
        bool now_flying = _unit->IsFlying();
        if (_was_flying && !now_flying)
            _last_land_ms = getMSTime();

        _was_flying = now_flying;
        return true;
    }
    bool IsPhaseGround() { return _unit && !_unit->IsFlying(); }
    bool IsPhaseFlight() { return _unit && _unit->IsFlying(); }
    bool JustLanded()
    {
        if (!_last_land_ms)
            return false;

        return getMSTime() - _last_land_ms <= POSITION_TIME_AFTER_LANDED;
    }
    bool WaitForExplosion()
    {
        if (!IsPhaseFlight())
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member &&
                (NaxxSpellIds::HasAnyAura(botAI, member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
                 botAI->HasAura("icebolt", member, false, false, -1, true)))
            {
                return true;
            }
        }
        return false;
    }
    bool FindPosToAvoidChill(std::vector<float>& dest)
    {
        Aura* aura = NaxxSpellIds::GetAnyAura(bot, {NaxxSpellIds::Chill10, NaxxSpellIds::Chill25});
        if (!aura)
        {
            // Fallback to name for custom spell data.
            aura = botAI->GetAura("chill", bot);
        }
        if (!aura)
            return false;

        DynamicObject* dyn_obj = aura->GetDynobjOwner();
        if (!dyn_obj)
            return false;

        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        float angle = 0;
        uint32 index = botAI->GetGroupSlotIndex(bot);
        if (currentTarget)
        {
            if (botAI->IsRanged(bot))
            {
                if (bot->GetExactDist2d(currentTarget) <= 45.0f)
                    angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;
                else
                {
                    if (index % 2 == 0)
                        angle = bot->GetAngle(currentTarget) + M_PI / 2;
                    else
                        angle = bot->GetAngle(currentTarget) - M_PI / 2;
                }
            }
            else
            {
                if (index % 3 == 0)
                    angle = bot->GetAngle(currentTarget);
                else if (index % 3 == 1)
                    angle = bot->GetAngle(currentTarget) + M_PI / 2;
                else
                    angle = bot->GetAngle(currentTarget) - M_PI / 2;
            }
        }
        else
            angle = bot->GetAngle(dyn_obj) - M_PI + (rand_norm() - 0.5) * M_PI / 2;

        dest = {bot->GetPositionX() + cos(angle) * 5.0f, bot->GetPositionY() + sin(angle) * 5.0f, bot->GetPositionZ()};
        return true;
    }
    // Tail Sweep (spell 55697) is a wide rear-cone frost hit + knockback that
    // recurs every ~10s. Melee that chase Sapphiron into the arc behind it eat
    // it repeatedly. Keep melee on a side flank (boss facing +/- 90 deg), which
    // sits clear of both the rear tail cone and the frontal cleave. The safe
    // direction is anchored to the boss's *facing*, and we only reposition while
    // the bot is actually inside the rear danger cone — otherwise the boss
    // re-facing the tank every tick makes melee jitter (the same dance the
    // chill/iceblock code fights). dest is written only when a move is needed.
    bool FindMeleePosToAvoidTail(std::vector<float>& dest)
    {
        if (!_unit)
            return false;

        // signed angular difference in -PI..PI
        auto angleDiff = [](float a, float b) { return std::atan2(std::sin(a - b), std::cos(a - b)); };

        float facing = _unit->GetOrientation();
        float bossToBot = _unit->GetAngle(bot);  // direction boss -> bot
        float rear = facing + M_PI;              // tail points opposite the facing

        // Rear danger half-angle. Sapphiron's tail arc is wide; ~75 deg either
        // side of dead-behind is a safe over-estimate, so the bot starts sliding
        // out well before it clips the real cone and parks within ~15 deg of the
        // 90 deg side flank.
        const float TAIL_HALF_ANGLE = 5.0f * M_PI / 12.0f;  // 75 degrees
        if (std::fabs(angleDiff(bossToBot, rear)) > TAIL_HALF_ANGLE)
            return false;  // already clear of the tail — let DpsAssist attack

        // In the cone: head for the nearer side flank.
        float leftFlank = facing + M_PI / 2.0f;
        float rightFlank = facing - M_PI / 2.0f;
        float flank = std::fabs(angleDiff(bossToBot, leftFlank)) <= std::fabs(angleDiff(bossToBot, rightFlank))
                          ? leftFlank
                          : rightFlank;

        float meleeDist = _unit->GetCombatReach() + bot->GetCombatReach() + 1.0f;
        dest = {_unit->GetPositionX() + std::cos(flank) * meleeDist,
                _unit->GetPositionY() + std::sin(flank) * meleeDist, _unit->GetPositionZ()};
        return true;
    }

private:
    void Reset()
    {
        _unit = nullptr;
        _was_flying = false;
        _last_land_ms = 0;
    }

    const uint32 POSITION_TIME_AFTER_LANDED = 5000;
    Unit* _unit = nullptr;
    bool _was_flying = false;
    uint32 _last_land_ms = 0;
};

class GluthBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos25 = {3331.48f, -3109.06f};
    const std::pair<float, float> mainTankPos10 = {3278.29f, -3162.06f};
    const std::pair<float, float> beforeDecimatePos = {3267.34f, -3175.68f};
    const std::pair<float, float> leftSlowDownPos = {3290.68f, -3141.65f};
    const std::pair<float, float> rightSlowDownPos = {3300.78f, -3151.98f};
    const std::pair<float, float> rangedPos = {3301.45f, -3139.29f};
    const std::pair<float, float> healPos = {3303.09f, -3135.24f};

    const float decimatedZombiePct = 10.0f;
    GluthBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "gluth");
            if (!_unit)
                return false;
        }
        if (_unit->IsInCombat())
        {
            if (_combat_start_ms == 0)
                _combat_start_ms = getMSTime();
        }
        else
            _combat_start_ms = 0;

        return true;
    }
    bool BeforeDecimate()
    {
        if (!_unit || !_unit->HasUnitState(UNIT_STATE_CASTING))
            return false;

        Spell* spell = _unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = _unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

        if (!spell)
            return false;

        SpellInfo const* info = spell->GetSpellInfo();
        if (!info)
            return false;

        if (NaxxSpellIds::MatchesAnySpellId(
                info, {NaxxSpellIds::Decimate10, NaxxSpellIds::Decimate25, NaxxSpellIds::Decimate25Alt}))
            return true;

        // Fallback to name for custom spell data.
        return info->SpellName[LOCALE_enUS] && botAI->EqualLowercaseName(info->SpellName[LOCALE_enUS], "decimate");
    }
    bool JustStartCombat() const { return _combat_start_ms != 0 && getMSTime() - _combat_start_ms < 10000; }
    bool IsZombieChow(Unit* unit) const { return unit && botAI->EqualLowercaseName(unit->GetName(), "zombie chow"); }

private:
    void Reset()
    {
        _unit = nullptr;
        _combat_start_ms = 0;
    }

    Unit* _unit = nullptr;
    uint32 _combat_start_ms = 0;
};

class LoathebBossHelper : public AiObject
{
public:
    const std::pair<float, float> mainTankPos = {2877.57f, -3967.00f};
    const std::pair<float, float> rangePos = {2896.96f, -3980.61f};
    LoathebBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "loatheb");

        return _unit != nullptr;
    }

private:
    void Reset() { _unit = nullptr; }

    Unit* _unit = nullptr;
};

class FourHorsemenBossHelper : public AiObject
{
public:
    const float posZ = 241.27f;
    const std::pair<float, float> attractPos[2] = {{2502.03f, -2910.90f},
                                                   {2484.61f, -2947.07f}};  // left (sir zeliek), right (lady blaumeux)
    // Split-corner strategy: each tank parks his melee boss in his own back
    // corner so the two 45y Mark-of-X auras don't overlap. Coordinates are
    // the final waypoints from boss_four_horsemen.cpp:92-110.
    const std::pair<float, float> tankPosThane = {2539.5f, -3018.6f};
    const std::pair<float, float> tankPosBaron = {2587.3f, -2968.0f};
    const std::pair<float, float> healerMidPos = {2563.4f, -2993.3f};
    // Center-back pocket for the back healers (25-man). Sits near the midpoint
    // of the two attract spots (~20-25y from each), so a parked healer covers
    // both ranged soakers without the corner-to-corner ping-pong. It is inside
    // both back Mark auras (Blaumeux/Zeliek), so these healers still rely on the
    // bleed-off-at-4-stacks behavior; they are out of range of the front tanks.
    // TUNABLE: verify heal range to both soakers in-game and nudge if needed.
    const std::pair<float, float> healerBackPos = {2500.0f, -2935.0f};
    // The other back healers park a few yards off so they don't pile onto one
    // point and jitter against each other (collision resolution shows up as one
    // bot nudging while another holds). All three stay in range of both soakers.
    const std::pair<float, float> healerBackPos2 = {2506.0f, -2940.0f};
    const std::pair<float, float> healerBackPos3 = {2494.0f, -2941.0f};
    // Lady Blaumeux's Void Zone (spell 28863) spawns NPC 16697 ("Void Zone")
    // at her current target's location and ticks ~3k shadow/sec in a small
    // radius. The visual disk and damage aura are ~6y; the buffer absorbs the
    // tick interval before the bot finishes side-stepping.
    static constexpr uint32 VOID_ZONE_NPC = 16697;
    static constexpr float VOID_ZONE_RADIUS = 6.5f;
    // Mark bleed-off spot: ~55-65y from each horseman (outside the 45y mark
    // aura but inside the room), reachable from midPos and tankPosBaron in
    // a few seconds. Non-attractor healers park here when a mark hits 4
    // stacks and stay until the aura fully decays.
    const std::pair<float, float> markBleedoffPos = {2530.0f, -2965.0f};
    static constexpr uint32 MARK_BLEEDOFF_STACKS = 4;
    // The pull is the roughest stretch — all four Marks start ramping and the
    // healers haven't settled yet — so bots pop their personal damage-reduction
    // cooldowns during this opening window.
    static constexpr uint32 OPENING_WINDOW_MS = 6000;
    FourHorsemenBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        else if (_combat_start_ms == 0)
            _combat_start_ms = getMSTime();

        if (_sir && (!_sir->IsInWorld() || !_sir->IsAlive()))
            Reset();

        if (!_sir)
        {
            _sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
            if (!_sir)
                return false;
        }
        _lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
        return true;
    }
    void Reset()
    {
        _sir = nullptr;
        _lady = nullptr;
        _combat_start_ms = 0;
        posToGo = 0;
    }
    // 0-based rank of `bot` among same-role *bot* group members by item level
    // (highest first), ties broken by lower GUID. The GUID tiebreak makes this a
    // stable total order — it must NOT flicker tick-to-tick, or the back-team
    // selection would flip and the bot would dance between its spot and the
    // front. Item level doesn't change mid-fight, so this is stable in practice.
    // Real players are excluded from the count: the human is never required to
    // hold a back slot, so only bots are ranked.
    uint8 RoleRankByItemLevel(Player* bot, bool heal)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return 0;
        float botIlvl = bot->GetAverageItemLevel();
        ObjectGuid botGuid = bot->GetGUID();
        uint8 ahead = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || m == bot)
                continue;
            if (!GET_PLAYERBOT_AI(m))  // skip real players — never required for the back team
                continue;
            if (heal ? !PlayerbotAI::IsHeal(m) : !PlayerbotAI::IsRangedDps(m))
                continue;
            float ilvl = m->GetAverageItemLevel();
            if (ilvl > botIlvl || (ilvl == botIlvl && m->GetGUID() < botGuid))
                ++ahead;
        }
        return ahead;
    }
    // True if `bot` is one of the top `topN` healers / ranged DPS by item level.
    // Used to send the strongest players to the back instead of whoever happens
    // to be first in the roster.
    bool IsTopHealByItemLevel(Player* bot, uint8 topN)
    {
        return PlayerbotAI::IsHeal(bot) && RoleRankByItemLevel(bot, true) < topN;
    }
    bool IsTopRangedDpsByItemLevel(Player* bot, uint8 topN)
    {
        return PlayerbotAI::IsRangedDps(bot) && RoleRankByItemLevel(bot, false) < topN;
    }
    bool IsAttracter(Player* bot)
    {
        Difficulty diff = bot->GetRaidDifficulty();
        if (diff == RAID_DIFFICULTY_25MAN_NORMAL)
        {
            // The two highest-item-level ranged DPS soak Lady/Sir in the back and
            // swap sides; the back healers stay parked at healerBackPos. Picking
            // by ilvl (not roster order) puts your strongest players on the back,
            // which is the rough spot.
            return IsTopRangedDpsByItemLevel(bot, 2);
        }
        return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0);
    }
    // 25-man back healers: the three highest-ilvl healers, parked center-back
    // instead of soaking a corner. Used to route their park spot.
    bool IsBackHealer(Player* bot)
    {
        return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL && IsTopHealByItemLevel(bot, 3);
    }
    // Park spot for a non-attractor healer. Back healers spread across the
    // center-back pocket by ilvl rank (1st→healerBackPos, 2nd→healerBackPos2,
    // 3rd→healerBackPos3) so they don't pile on one point; everyone else (10-man,
    // or any healer past the back count) covers the front tanks from healerMidPos.
    const std::pair<float, float>* HealerParkPos(Player* bot)
    {
        if (!IsBackHealer(bot))
            return &healerMidPos;
        if (IsTopHealByItemLevel(bot, 1))
            return &healerBackPos;
        if (IsTopHealByItemLevel(bot, 2))
            return &healerBackPos2;
        return &healerBackPos3;
    }
    // First few seconds of the encounter — see OPENING_WINDOW_MS.
    bool IsOpeningWindow() const
    {
        return _combat_start_ms != 0 && (getMSTime() - _combat_start_ms) < OPENING_WINDOW_MS;
    }
    void CalculatePosToGo(Player* bot)
    {
        Unit* lady = _lady;
        if (!lady)
            posToGo = 0;
        else
        {
            uint32 elapsed_ms = _combat_start_ms ? getMSTime() - _combat_start_ms : 0;
            // Interval: 24s - 15s - 15s - ...
            posToGo = !(elapsed_ms <= 9000 || ((elapsed_ms - 9000) / 67500) % 2 == 0);
            // One soaker takes the spot opposite the other so they sit on
            // different horsemen and cross over on each swap. 25-man uses the
            // top-ilvl ranged pair (flip the #1); 10-man keeps the roster-order
            // ranged #0 (its attractor selection is unchanged).
            bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
            bool flip = raid25 ? IsTopRangedDpsByItemLevel(bot, 1) : botAI->IsAssistRangedDpsOfIndex(bot, 0);
            if (flip)
                posToGo = 1 - posToGo;
        }
    }
    std::pair<float, float> CurrentAttractPos()
    {
        bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;
        float posX = attractPos[posToGo].first, posY = attractPos[posToGo].second;
        if (posToGo == 1)
        {
            float offset_x = 0.0f;
            float offset_y = 0.0f;
            float bias = 4.5f;
            if (raid25)
            {
                offset_x = -bias;
                offset_y = bias;
            }
            posX += offset_x;
            posY += offset_y;
        }
        return {posX, posY};
    }
    Unit* CurrentAttackTarget()
    {
        if (posToGo == 0)
            return _sir;

        return _lady;
    }
    // The other attract spot — used as a "safe direction" hint when fleeing
    // a void zone, so the attractor stays in heal range of the bot on the
    // opposite side instead of skittering away from the raid.
    std::pair<float, float> OppositeAttractPos() const
    {
        return attractPos[posToGo == 0 ? 1 : 0];
    }
    Unit* GetVoidZoneStandingIn(Player* p) { return FindVoidZoneAt(p->GetPositionX(), p->GetPositionY()); }
    Unit* FindVoidZoneAt(float x, float y)
    {
        GuidVector npcs = *context->GetValue<GuidVector>("nearest npcs");
        for (auto const& g : npcs)
        {
            Unit* unit = botAI->GetUnit(g);
            if (!unit || unit->GetEntry() != VOID_ZONE_NPC)
                continue;
            if (unit->GetExactDist2d(x, y) < VOID_ZONE_RADIUS)
                return unit;
        }
        return nullptr;
    }
    uint32 GetHighestMarkStacks(Player* p)
    {
        uint32 max_stacks = 0;
        for (uint32 spellId : {NaxxSpellIds::MarkOfKorthazz, NaxxSpellIds::MarkOfBlaumeux,
                               NaxxSpellIds::MarkOfRivendare, NaxxSpellIds::MarkOfZeliek})
        {
            if (Aura* a = p->GetAura(spellId))
                max_stacks = std::max<uint32>(max_stacks, a->GetStackAmount());
        }
        return max_stacks;
    }
    // Hysteresis: leave at >= MARK_BLEEDOFF_STACKS, only return when the
    // aura has fully decayed (otherwise the bot walks back to midPos at 3
    // stacks, eats the next mark, hits 4 again, and bounces).
    bool ShouldHealerBleedOffMark(Player* p)
    {
        uint32 stacks = GetHighestMarkStacks(p);
        if (stacks >= MARK_BLEEDOFF_STACKS)
            return true;
        if (stacks > 0 && p->GetDistance2d(markBleedoffPos.first, markBleedoffPos.second) < 6.0f)
            return true;
        return false;
    }
    // If (posX, posY) is contaminated by a void zone, return a point just
    // outside the puddle along the line toward the opposite attractor.
    // Otherwise return the input unchanged. The bias direction is what
    // breaks the ping-pong loop with FourHorsemenAvoidVoidZoneAction —
    // FleePosition steps out, this re-routes the attract to the same
    // outside-the-puddle spot instead of yanking back to center.
    std::pair<float, float> ResolveSafeAttractPos(float posX, float posY)
    {
        Unit* vz = FindVoidZoneAt(posX, posY);
        if (!vz)
            return {posX, posY};

        auto [oppX, oppY] = OppositeAttractPos();
        float vzX = vz->GetPositionX();
        float vzY = vz->GetPositionY();
        float dx = oppX - vzX;
        float dy = oppY - vzY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f)
            return {posX, posY};

        float step = VOID_ZONE_RADIUS + 1.5f;
        return {vzX + dx / len * step, vzY + dy / len * step};
    }

    // Like ResolveSafeAttractPos but for a parked back healer: if a void zone is
    // sitting on its park spot, step just outside the puddle toward the midpoint
    // of the two soak spots (keeps the healer in range of both soakers and
    // inside the room). Both the park MoveTo and the void-avoid action run a
    // destination through this, so they agree on one point instead of fighting
    // over the contaminated spot — that fight is the 5-6y back-and-forth.
    std::pair<float, float> ResolveSafeHealerPos(float posX, float posY)
    {
        Unit* vz = FindVoidZoneAt(posX, posY);
        if (!vz)
            return {posX, posY};

        float vzX = vz->GetPositionX();
        float vzY = vz->GetPositionY();
        float refX = (attractPos[0].first + attractPos[1].first) * 0.5f;
        float refY = (attractPos[0].second + attractPos[1].second) * 0.5f;
        float dx = refX - vzX;
        float dy = refY - vzY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f)
            return {posX, posY};

        float step = VOID_ZONE_RADIUS + 1.5f;
        return {vzX + dx / len * step, vzY + dy / len * step};
    }

protected:
    Unit* _sir = nullptr;
    Unit* _lady = nullptr;
    uint32 _combat_start_ms = 0;
    int posToGo = 0;
};
// Maexxna's Web Wrap is a stationary NPC (entry 16486, NullCreatureAI) spawned
// at one of 7 wall positions when a non-tank gets webbed. It stuns the victim
// until killed — so DPS need to break off the boss and free them. The NPC is
// not in any bot's attacker list (it never enters combat), so we scan "nearest
// npcs" by entry instead.
class MaexxnaBossHelper : public AiObject
{
public:
    static constexpr uint32 NPC_WEB_WRAP = 16486;
    // Maexxna casts Frenzy at <30% HP (boss_maexxna.cpp EVENT_HEALTH_CHECK) and
    // keeps it for the rest of the fight — the "go ham" / burn window.
    static constexpr uint32 SPELL_FRENZY = 54123;
    // Web Spray: raid-wide ~3s stun, recast every 40s (first at 40s). It's the
    // sub-30% death window — healers are stunned too, so the Frenzy-boosted tank
    // eats melee with zero active healing. Externals must be PRE-cast so they
    // carry through the stun (boss_maexxna.cpp EVENT_WEB_SPRAY).
    static constexpr uint32 SPELL_WEB_SPRAY = 29484;
    MaexxnaBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            _unit = nullptr;
        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            _unit = nullptr;
        if (!_unit)
            _unit = AI_VALUE2(Unit*, "find target", "maexxna");

        if (!_unit)
        {
            _combat_start_ms = 0;
            _next_web_spray_ms = 0;
            _web_spray_aura_seen = false;
            return false;
        }

        UpdateWebSprayClock();
        return true;
    }
    // True once the boss has entered her sub-30% Frenzy. Boss HP is monotonic,
    // so the percentage check alone is reliable; the aura is a backstop against
    // absorb/heal jitter right around the threshold.
    bool IsFrenzied()
    {
        if (!_unit)
            return false;
        return _unit->GetHealthPct() < 30.0f || botAI->HasAura(SPELL_FRENZY, _unit);
    }
    // True in the short window just before the next predicted Web Spray. Lets
    // the tank's defensive and the healers' externals (Hand of Sacrifice /
    // Guardian Spirit) be cast BEFORE the stun lands so they're active through
    // it — reactive healing can't, since healers are stunned too. The boss
    // gives no readable timer, so this predicts from a 40s cadence and
    // re-anchors whenever the raid-wide stun aura is observed (see
    // UpdateWebSprayClock), keeping boss-cast jitter from accumulating drift.
    // Only meaningful sub-30%; callers gate on IsFrenzied().
    bool WebSprayImminent(uint32 leadMs = 4000)
    {
        if (!_unit || _next_web_spray_ms == 0)
            return false;
        uint32 now = getMSTime();
        // Open the window leadMs before the prediction and hold it open for a
        // tail past the prediction (covers a spray delayed by an in-progress
        // boss cast). The aura re-anchor snaps _next_web_spray_ms forward the
        // moment the spray actually lands, which closes the window cleanly.
        return now + leadMs >= _next_web_spray_ms && now <= _next_web_spray_ms + WEB_SPRAY_LATE_TAIL_MS;
    }
    Unit* GetClosestWebWrap()
    {
        Unit* best = nullptr;
        float bestDist = 0.0f;
        GuidVector npcs = *context->GetValue<GuidVector>("nearest npcs");
        for (ObjectGuid const& g : npcs)
        {
            Unit* unit = botAI->GetUnit(g);
            if (!unit || !unit->IsAlive())
                continue;
            if (unit->GetEntry() != NPC_WEB_WRAP)
                continue;
            float d = bot->GetExactDist2d(unit);
            if (!best || d < bestDist)
            {
                best = unit;
                bestDist = d;
            }
        }
        return best;
    }

private:
    // Predict the next Web Spray from combat start (first at 40s, then every
    // 40s), and re-anchor on the rising edge of the raid-wide stun aura. The
    // boss skips its event queue while mid-cast (Poison Shock / Necrotic
    // Poison), so a pure clock would drift; snapping to each observed spray
    // keeps the prediction tied to the boss's real cadence.
    void UpdateWebSprayClock()
    {
        uint32 now = getMSTime();
        if (_combat_start_ms == 0)
        {
            _combat_start_ms = now;
            _next_web_spray_ms = now + WEB_SPRAY_INTERVAL_MS;
        }
        bool seen = botAI->HasAura(SPELL_WEB_SPRAY, bot);
        if (seen && !_web_spray_aura_seen)
            _next_web_spray_ms = now + WEB_SPRAY_INTERVAL_MS;
        _web_spray_aura_seen = seen;
    }

    static constexpr uint32 WEB_SPRAY_INTERVAL_MS = 40000;
    static constexpr uint32 WEB_SPRAY_LATE_TAIL_MS = 6000;
    Unit* _unit = nullptr;
    uint32 _combat_start_ms = 0;
    uint32 _next_web_spray_ms = 0;
    bool _web_spray_aura_seen = false;
};

class ThaddiusBossHelper : public AiObject
{
public:
    const std::pair<float, float> tankPosFeugen = {3522.94f, -3002.60f};
    const std::pair<float, float> tankPosStalagg = {3436.14f, -2919.98f};
    const std::pair<float, float> rangedPosFeugen = {3500.45f, -2997.92f};
    const std::pair<float, float> rangedPosStalagg = {3441.01f, -2942.04f};
    const float tankPosZ = 312.61f;
    ThaddiusBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI()
    {
        if (!bot->IsInCombat())
            Reset();

        if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
            Reset();

        if (!_unit)
        {
            _unit = AI_VALUE2(Unit*, "find target", "thaddius");
            if (!_unit)
                return false;
        }
        feugen = AI_VALUE2(Unit*, "find target", "feugen");
        stalagg = AI_VALUE2(Unit*, "find target", "stalagg");
        return true;
    }
    bool IsPhasePet() { return (feugen && feugen->IsAlive()) || (stalagg && stalagg->IsAlive()); }
    bool IsPhaseTransition()
    {
        if (IsPhasePet())
            return false;

        return _unit && _unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }
    bool IsPhaseThaddius() { return !IsPhasePet() && !IsPhaseTransition(); }
    Unit* GetNearestPet()
    {
        Unit* unit = nullptr;
        if (feugen && feugen->IsAlive())
            unit = feugen;

        if (stalagg && stalagg->IsAlive() &&
            (!feugen || !feugen->IsAlive() || bot->GetDistance(stalagg) < bot->GetDistance(feugen)))
            unit = stalagg;

        return unit;
    }
    std::pair<float, float> PetPhaseGetPosForTank()
    {
        if (GetNearestPet() == feugen)
            return tankPosFeugen;

        return tankPosStalagg;
    }
    std::pair<float, float> PetPhaseGetPosForRanged()
    {
        if (GetNearestPet() == feugen)
            return rangedPosFeugen;

        return rangedPosStalagg;
    }

protected:
    void Reset()
    {
        _unit = nullptr;
        feugen = nullptr;
        stalagg = nullptr;
    }

    Unit* _unit = nullptr;
    Unit* feugen = nullptr;
    Unit* stalagg = nullptr;
};

#endif
