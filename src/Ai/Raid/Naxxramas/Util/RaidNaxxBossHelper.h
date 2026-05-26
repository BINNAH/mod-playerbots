#ifndef _PLAYERBOT_RAIDNAXXBOSSHELPER_H
#define _PLAYERBOT_RAIDNAXXBOSSHELPER_H

#include <algorithm>
#include <string>
#include <vector>

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "Group.h"
#include "InstanceScript.h"
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
    // Where the boss actually hovers during the AIR phase: its spawn/home
    // position (creature.guid 133932), ~18y NE of `center`. EVENT_FLIGHT_START
    // flies Sapphiron home and it MoveIdle()s there, so the Frost Breath
    // originates HERE — not at `center`, which is the GROUND tank spot. The
    // explosion's LoS check (block.IsInBetween(boss, target, 2.0)) uses this
    // real position, and the safe corridor behind a block is only ~2y wide, so
    // the air-phase spread + "behind the block" geometry must be anchored here
    // too. Anchoring on `center` lined bots up ~18y off-axis (worse the deeper
    // they stood) and clipped the breath. It's a constant (not the live boss)
    // to stay drift-free — the original code used `center` for exactly that
    // stability, it just picked the wrong fixed point.
    const std::pair<float, float> flightCenter = {3522.39f, -5236.78f};
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
    // True while `unit` is encased in an ice block (the Icebolt trigger aura).
    // Encased players are the LOS shields the rest of the raid hides behind.
    bool HasIcebolt(Unit* unit)
    {
        return NaxxSpellIds::HasAnyAura(botAI, unit, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
               botAI->HasAura("icebolt", unit, false, false, -1, true);
    }
    // How many players Sapphiron encases per air phase: RAID_MODE(2, 3) in
    // boss_sapphiron.cpp. Bots wait for the whole set to form before picking a
    // block, so they don't all dive on the first (often badly-placed) one.
    uint32 ExpectedIceblockCount() const
    {
        return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 3 : 2;
    }
    // The alive, encased group members = the ice blocks. Sorted by GUID so every
    // bot computes the SAME ordering/assignment (no tick-to-tick flicker → no
    // dance). Encased players are rooted, so these positions are stable for the
    // whole air phase.
    std::vector<Player*> GetIceblocks()
    {
        std::vector<Player*> blocks;
        Group* group = bot->GetGroup();
        if (!group)
            return blocks;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (m && m->IsAlive() && HasIcebolt(m))
                blocks.push_back(m);
        }
        std::sort(blocks.begin(), blocks.end(),
                  [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });
        return blocks;
    }
    // Alive players in the raid (human + bots). Sapphiron can't encase more
    // distinct players than are alive, so this caps how many blocks we can wait
    // for in a depleted raid.
    uint32 AliveRaidPlayerCount()
    {
        uint32 n = 0;
        if (Group* group = bot->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* m = ref->GetSource())
                    if (m->IsAlive())
                        ++n;
        return n;
    }
    // Don't commit to a block until the full set of ice blocks has formed
    // (waiting for 2-3 instead of running at the first one we see). Capped by
    // the alive player count: in a depleted raid the boss physically can't
    // encase the full set, so without the cap bots would wait forever for a
    // block that can never form and eat the breath standing on the spread ring.
    // Stateless on purpose — the flight action's helper never observes the
    // ground phase between air phases, so any cross-phase timer kept here would
    // go stale; the live block + alive counts are always correct.
    bool ReadyToHideBehindIceblock()
    {
        if (!IsPhaseFlight())
            return false;
        size_t blocks = GetIceblocks().size();
        if (blocks == 0)
            return false;
        uint32 needed = std::min<uint32>(ExpectedIceblockCount(), AliveRaidPlayerCount());
        return blocks >= needed;
    }
    // Fixed formation "home" for any group member, derived ONLY from its stable
    // group-slot index — never its live position. Two radius layers + a per-slot
    // angle fan bots out on a conservative arc centered on the proven NW ranged
    // spot (0.85*PI). Used for the pre-air spread ring AND (critically) the
    // iceblock assignment below, so the assignment is a pure function of fixed
    // inputs and can't chase live movement. TUNABLE: widen the arc span to push
    // the blocks further apart (mind the room walls / south entrance).
    std::pair<float, float> SpreadHomePos(Player* m)
    {
        uint32 index = botAI->GetGroupSlotIndex(m);
        float distance = (index % 2 == 0) ? 30.0f : 36.0f;
        uint32 layer = index / 2;
        float angle = 0.7f * M_PI + 0.03f * M_PI * layer;
        // Centered on the flight-hover point so blocks form spread AROUND the
        // boss and "behind the block" points cleanly radially outward.
        return {flightCenter.first + std::cos(angle) * distance,
                flightCenter.second + std::sin(angle) * distance};
    }
    std::pair<float, float> PreAirSpreadPos() { return SpreadHomePos(bot); }
    // Block (from the GUID-sorted list) nearest to a fixed point, GUID tiebreak
    // so it's deterministic when two are equidistant.
    Player* NearestIceblockToPos(float x, float y, std::vector<Player*> const& blocks)
    {
        Player* best = nullptr;
        float bestDist = 0.0f;
        for (Player* b : blocks)
        {
            float d = b->GetExactDist2d(x, y);
            if (!best || d < bestDist || (d == bestDist && b->GetGUID() < best->GetGUID()))
            {
                best = b;
                bestDist = d;
            }
        }
        return best;
    }
    // Which block a member hides behind: the one nearest its FIXED formation
    // home. Because both the home and the (rooted) block positions are constant
    // for the whole air phase, every bot computes the same assignment for every
    // member — so nobody's target depends on where anyone currently is.
    Player* AssignedIceblock(Player* m, std::vector<Player*> const& blocks)
    {
        std::pair<float, float> home = SpreadHomePos(m);
        return NearestIceblockToPos(home.first, home.second, blocks);
    }
    // Where THIS bot should stand to break LOS for the frost breath. Each bot
    // hides behind its assigned (nearest-to-home) block; bots sharing a block
    // tile the safe lane at staggered depths/columns instead of stacking on the
    // single point behind it — that pile-up is why "the close block" left people
    // exposed. The safe corridor is only ~2y wide and 10y deep, so depth stays
    // <=7y and lateral offsets stay small. EVERY input here is fixed for the air
    // phase (assignment + slot use formation homes and GUIDs, not live positions;
    // the boss anchor is the constant flightCenter), so the destination is
    // constant: the bot paths to it once and parks — no tick-to-tick dance.
    bool GetIceblockHidePos(std::vector<float>& dest)
    {
        std::vector<Player*> blocks = GetIceblocks();
        if (blocks.empty())
            return false;

        Player* myBlock = AssignedIceblock(bot, blocks);
        if (!myBlock)
            return false;

        // Slot = rank among the bot-hiders assigned to the SAME block, ordered
        // by GUID. Real players self-manage and are skipped — bots only
        // coordinate slots among themselves (the human is never required).
        uint32 slot = 0;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* m = ref->GetSource();
                if (!m || m == bot || !m->IsAlive())
                    continue;
                if (!GET_PLAYERBOT_AI(m))  // human picks its own block
                    continue;
                if (HasIcebolt(m))  // encased bots are blocks, not hiders
                    continue;
                if (AssignedIceblock(m, blocks) == myBlock && m->GetGUID() < bot->GetGUID())
                    ++slot;
            }
        }

        // "Behind the block" = directly away from the boss's real hover point,
        // so the block sits on the boss->bot line (perp distance ~0). This is
        // the LoS axis the explosion checks; using `center` here put bots ~18y
        // off-axis. Perp error is ~0 on this axis regardless of depth, so depth
        // is "free" — only the lateral column costs perp tolerance, kept small.
        float ang = std::atan2(myBlock->GetPositionY() - flightCenter.second,
                               myBlock->GetPositionX() - flightCenter.first);
        uint32 row = slot % 3;  // depth row behind the block: 3 / 5 / 7y (<<10y)
        uint32 col = slot / 3;  // sideways column within the ~2y-wide safe lane
        float depth = 3.0f + row * 2.0f;
        int colSign = (col % 2 == 0) ? 1 : -1;
        float lateral = float((col + 1) / 2) * 1.2f * colSign;  // 0, +1.2, -1.2, ...
        float px = myBlock->GetPositionX() + std::cos(ang) * depth - std::sin(ang) * lateral;
        float py = myBlock->GetPositionY() + std::sin(ang) * depth + std::cos(ang) * lateral;
        dest = {px, py, GENERIC_HEIGHT};
        return true;
    }
    // True in the brief window after EVENT_FLIGHT_START but before liftoff: the
    // boss has gone passive and is gliding to center, not yet flying. Gives bots
    // a head start on backing up + spreading. Excludes the equally-passive
    // post-landing settle window (JustLanded) so we don't yank bots back out
    // right after they land.
    bool IsPreAirPhase()
    {
        if (!IsPhaseGround() || !_unit || JustLanded())
            return false;
        Creature* c = _unit->ToCreature();
        return c && c->GetReactState() == REACT_PASSIVE;
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
    // Sapphiron's two melee-range cone attacks bracket the boss front-to-back,
    // both recurring every ~10s:
    //   * Cleave (spell 19983) — frontal cone cast on the boss's victim. Melee
    //     DPS that drift into the front arc get cleaved alongside the tank.
    //   * Tail Sweep (spell 55697) — wide rear cone + knockback.
    // The only safe melee real estate is the two side flanks (boss facing
    // +/- 90 deg), which clear BOTH cones. Keep melee there: hand back a flank
    // destination while the bot is inside either danger cone, and return false
    // once it's parked on a flank so DpsAssist can attack. The safe direction is
    // anchored to the boss's *facing*, and we only reposition while actually in
    // a cone — otherwise the boss re-facing the tank every tick makes melee
    // jitter (the same dance the chill/iceblock code fights). The bot's own
    // chase target (when it is the victim) is exempt from the front check: it
    // can't dodge a cleave aimed at itself by running, and trying to would just
    // drag the boss around. dest is written only when a move is needed.
    bool FindMeleePosToAvoidCleaveAndTail(std::vector<float>& dest)
    {
        if (!_unit)
            return false;

        // signed angular difference in -PI..PI
        auto angleDiff = [](float a, float b) { return std::atan2(std::sin(a - b), std::cos(a - b)); };

        float facing = _unit->GetOrientation();
        float bossToBot = _unit->GetAngle(bot);  // direction boss -> bot
        float rear = facing + M_PI;              // tail points opposite the facing

        // Danger half-angles. Both are safe over-estimates so the bot starts
        // sliding out before it clips the real cone; their gap keeps the 90 deg
        // side flank clear of both (75 < 90 and 60 < 90), and the flank parks
        // ~15 deg off the tail / ~30 deg off the cleave.
        const float TAIL_HALF_ANGLE = 5.0f * M_PI / 12.0f;  // 75 degrees
        const float CLEAVE_HALF_ANGLE = M_PI / 3.0f;        // 60 degrees

        bool inTail = std::fabs(angleDiff(bossToBot, rear)) <= TAIL_HALF_ANGLE;
        // Don't try to flee a cleave aimed at us — only avoid the front arc when
        // someone else (the tank) is the boss's target.
        bool inCleave = _unit->GetVictim() != bot &&
                        std::fabs(angleDiff(bossToBot, facing)) <= CLEAVE_HALF_ANGLE;
        if (!inTail && !inCleave)
            return false;  // already on a side flank — let DpsAssist attack

        // In a danger cone: head for the nearer side flank.
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
    // 25-man: tank Gluth on the NE side at {3319, -3120} (the old spot sat ~58y
    // NE, right by the exit, which dragged the ranged pack out the north door).
    // The raid stacks on the SW / far side of the boss (rangedClusterPos25)
    // while designated kiters work the zombie chow in the SW of the room near
    // the gates — see GluthSlowdownAction.
    const std::pair<float, float> mainTankPos25 = {3319.0f, -3120.0f};
    const std::pair<float, float> mainTankPos10 = {3278.29f, -3162.06f};
    // Where the 25-man ranged/healer pack stacks: SW / behind the boss, opposite
    // the NE tank, leaning toward the chow gates. Keeps casters in range to DPS
    // Gluth AND to heal/support the kiters working the zombie stream in the SW.
    // Bots fan out around this anchor by group slot. TUNABLE: nudge SW (toward
    // the gates) if healers can't reach the kiters, NE (toward the boss) if
    // kited zombies clip the pack.
    const std::pair<float, float> rangedClusterPos25 = {3300.0f, -3140.0f};
    const std::pair<float, float> beforeDecimatePos = {3267.34f, -3175.68f};
    const std::pair<float, float> leftSlowDownPos = {3290.68f, -3141.65f};
    const std::pair<float, float> rightSlowDownPos = {3300.78f, -3151.98f};
    const std::pair<float, float> rangedPos = {3301.45f, -3139.29f};
    const std::pair<float, float> healPos = {3303.09f, -3135.24f};
    // Where the 25-man kiters loop the zombie chow: a ring in the SW of the room
    // among the chow gates, well clear of the boss (~43y NE) and the raid stack
    // (~16y NE of the ring edge). Kiters orbit this ring dragging the aggroed
    // chow with them, away from Gluth. TUNABLE: move the center SW / shrink the
    // radius if chow leak toward the raid; the ring must stay on the chow's path
    // up from the gates so new spawns get picked up and snared.
    const std::pair<float, float> kiteCenter25 = {3276.0f, -3160.0f};
    // Kite ring radius. Widened 15 → 20 so the kiters/off-tanks sweep further out
    // to the LEFT and RIGHT, catching chow that were sneaking up the side lanes
    // toward Gluth. TUNABLE: crank higher to cover more flank (mind the room
    // walls — at 20 the ring edge still sits ~38y from the NE tank spot).
    const float kiteRadius25 = 20.0f;
    // Designated kiters: the first KITERS_PER_CLASS *bot* hunters and mages.
    static constexpr uint32 KITERS_PER_CLASS = 2;
    static constexpr uint32 NPC_ZOMBIE_CHOW = 16360;  // boss_gluth.cpp
    static constexpr uint32 NPC_GLUTH = 15932;        // boss_gluth.cpp
    // naxxramas.h BOSS_GLUTH encounter index, mirrored here (that header isn't on
    // the playerbots include path — same trick as the Four Horsemen helper). Used
    // for a THREAT-INDEPENDENT "is the pull live" check, so the off-tanks/kiters,
    // who must never threaten Gluth, can still do their jobs from the pull.
    static constexpr uint32 BOSS_GLUTH_ENCOUNTER = 2;
    // Off-tanks also work the chow in the SW: they anchor the pack with AoE
    // threat to keep it off Gluth, and start kiting (circling) it once
    // OT_KITE_THRESHOLD+ chow are piled on them, so they aren't bursted down.
    static constexpr uint32 OT_KITE_THRESHOLD = 6;
    static constexpr float OT_HOLD_RADIUS = 9.0f;  // a chow within this is "on me"
    // How far (yards) the two off-tanks split LEFT/RIGHT of the ring center to
    // guard the side lanes. Widened 6 → 12 so they body+AoE the flanks the chow
    // were slipping through, instead of bunching near center. TUNABLE.
    static constexpr float OT_FLANK_OFFSET = 12.0f;

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
        // BossAnyway() (not the threat-cached _unit) so the kiters — who never
        // threaten Gluth — still detect the Decimate cast and bank their roots.
        Unit* boss = BossAnyway();
        if (!boss || !boss->HasUnitState(UNIT_STATE_CASTING))
            return false;

        Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

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
    Unit* Boss() const { return _unit; }

    // Gluth via the room scan (by entry), NOT the threat-based find-target. The
    // chow handlers never threaten Gluth, so find-target returns null for them;
    // this lets them read his position (leak detection) and lets the multiplier
    // govern them without their ever having to touch the boss. "nearest npcs"
    // reaches across the room (same as GetAliveZombieChow).
    Unit* GluthNearby()
    {
        GuidVector npcs = *context->GetValue<GuidVector>("nearest npcs");
        for (ObjectGuid const& g : npcs)
        {
            Unit* u = botAI->GetUnit(g);
            if (u && u->IsAlive() && u->GetEntry() == NPC_GLUTH)
                return u;
        }
        return nullptr;
    }
    // Best available Gluth pointer: the threat-cached one if we have it, else the
    // room scan. Lets a chow handler position off the boss without first hitting
    // him (the old find-target-only path forced that hit → the pull aggro-steal).
    Unit* BossAnyway() { return _unit ? _unit : GluthNearby(); }
    // True for the whole encounter, from the pull, regardless of who has threat
    // (instance boss state — threat-independent, like FourHorsemen::EncounterEngaged).
    bool GluthEngaged()
    {
        InstanceScript* instance = bot->GetInstanceScript();
        return instance && instance->GetBossState(BOSS_GLUTH_ENCOUNTER) == IN_PROGRESS;
    }

    // Is `member` one of the designated zombie kiters? 25-man only; the first
    // KITERS_PER_CLASS *bot* hunters and the first KITERS_PER_CLASS *bot* mages.
    // Ranked by GUID among bots of the class (a stable total order), so the set
    // never reshuffles tick-to-tick or on a death — and the human is never
    // auto-assigned a kite slot (they can kite by hand if they want). 10-man
    // always returns false, so its (already-working) positioning is untouched.
    bool IsKiter(Player* member) const
    {
        if (!member || member->GetRaidDifficulty() != RAID_DIFFICULTY_25MAN_NORMAL)
            return false;
        uint8 cls = member->getClass();
        if (cls != CLASS_HUNTER && cls != CLASS_MAGE)
            return false;
        Group* group = member->GetGroup();
        if (!group)
            return false;
        uint32 ahead = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (!m || m == member)
                continue;
            if (!GET_PLAYERBOT_AI(m))  // human is never required to kite
                continue;
            if (m->getClass() != cls)
                continue;
            if (m->GetGUID() < member->GetGUID())
                ++ahead;
        }
        return ahead < KITERS_PER_CLASS;
    }
    // Is `member` an off-tank assigned to the chow (25-man only)? The main tank
    // stays on Gluth; the assist tanks go back to the SW and tank/kite the chow.
    // 10-man returns false, so its assist tanks keep tanking the boss as before.
    bool IsZombieOffTank(Player* member) const
    {
        if (!member || member->GetRaidDifficulty() != RAID_DIFFICULTY_25MAN_NORMAL)
            return false;
        if (botAI->IsMainTank(member))
            return false;
        return botAI->IsAssistTankOfIndex(member, 0) || botAI->IsAssistTankOfIndex(member, 1);
    }
    // Alive Zombie Chow the bot can currently see, by entry (more robust than
    // the localized name). "nearest npcs" reaches far enough for a hunter to
    // spot leaks heading for the boss across the room.
    std::vector<Unit*> GetAliveZombieChow()
    {
        std::vector<Unit*> chow;
        GuidVector npcs = *context->GetValue<GuidVector>("nearest npcs");
        for (ObjectGuid const& g : npcs)
        {
            Unit* u = botAI->GetUnit(g);
            if (u && u->IsAlive() && u->GetEntry() == NPC_ZOMBIE_CHOW)
                chow.push_back(u);
        }
        return chow;
    }
    // True while the raid should stop kiting and burn the chow: Gluth is casting
    // Decimate, or chow are already at the post-Decimate ~5% and sprinting him
    // (he heals 5% of max HP for every chow he eats, so they must die first).
    bool InDecimateBurn(std::vector<Unit*> const& chow)
    {
        if (BeforeDecimate())
            return true;
        for (Unit* z : chow)
            if (z && z->GetHealthPct() <= decimatedZombiePct)
                return true;
        return false;
    }

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
    // Back-healer side spots (25-man). The old center-back pocket sat inside
    // BOTH back Mark auras (Blaumeux + Zeliek), so a parked healer collected two
    // marks at once and leaned on the 4-stack bleed-off constantly. Instead each
    // back healer parks next to ONE back caster — only that caster's Mark lands —
    // and the set rotates sides on the Mark cadence (see BackHealerSide) so the
    // mark being left decays while the new one ramps from zero. Each spot sits
    // ~5y off its soaker spot (attractPos), safely inside exactly one 45y Mark
    // aura and clear of the other (~55-60y away). Index 0 = Sir Zeliek side
    // (Mark of Zeliek), index 1 = Lady Blaumeux side (Mark of Blaumeux).
    // TUNABLE: verify single-mark coverage + heal range to the soaker in-game.
    const std::pair<float, float> healerSidePos[2] = {{2505.7f, -2907.6f}, {2479.6f, -2947.2f}};
    // Side-swap cadence. Lady/Sir cast their Mark every 15s (boss_four_horsemen
    // .cpp), and per-application damage spikes hard at 4 stacks (4000) vs 3
    // (1500). Flipping sides about every 3 applications keeps a back healer at
    // <=3 stacks of either back Mark. Derived from a free-running clock (not a
    // per-instance combat timer) so a bot's positioning and void-avoid helpers
    // always agree on the side without shared state; the 4-stack bleed-off
    // (markBleedoffPos) stays as the backstop if a swap lands late. TUNABLE.
    static constexpr uint32 HEALER_SIDE_SWAP_MS = 38000;
    // No horseman realistically dies before this; gate the "pull an extra healer
    // back" check past it so a healer still building threat on all four during
    // the pull doesn't read the un-threatened ones as dead and get yanked back.
    static constexpr uint32 FIRST_KILL_EARLIEST_MS = 20000;
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
    // --- Back phase (the front melee pair is dead; the raid collapses onto the
    // two stationary casters Lady Blaumeux + Sir Zeliek) ---------------------
    // Mirror of naxxramas.h ids. That header isn't on the playerbots include
    // path, so we duplicate the few values we need; they're stable instance
    // constants. BOSS_HORSEMAN is the encounter index used for GetBossState();
    // DATA_*_BOSS look the creatures up by GUID from the instance script so a
    // bot can find a horseman it has NEVER threatened — the threat-based
    // "find target" returns null for, say, a front DPS that only ever hit
    // Thane/Baron, which left it idle once the front pair died (it could not
    // acquire Lady/Sir 100+y away across the room). NPC entries identify which
    // caster a unit is (→ which Mark spell it applies).
    static constexpr uint32 BOSS_HORSEMAN = 12;
    static constexpr uint32 DATA_BARON_RIVENDARE_BOSS = 107;
    static constexpr uint32 DATA_SIR_ZELIEK_BOSS = 108;
    static constexpr uint32 DATA_LADY_BLAUMEUX_BOSS = 109;
    static constexpr uint32 DATA_THANE_KORTHAZZ_BOSS = 110;
    static constexpr uint32 NPC_SIR_ZELIEK = 16063;
    static constexpr uint32 NPC_LADY_BLAUMEUX = 16065;
    // Switch casters once this bot reaches this many stacks of the current
    // caster's Mark — i.e. before the 4th application (per-hit damage spikes
    // 1500 at 3 stacks → 4000 at 4; see spell_four_horsemen_mark_aura). Running
    // to the *other* caster (~70y away, outside the first's 45y Mark aura) sheds
    // the stack while keeping DPS uptime, instead of parking at a dead spot.
    static constexpr uint32 MARK_SWITCH_STACKS = 3;
    // Minimum time committed to a caster after a switch, so a bot that arrives
    // still carrying a high (not-yet-decayed) stack of the new caster's Mark
    // doesn't immediately bounce back — that would be a run-back-and-forth dance.
    static constexpr uint32 BACK_SWITCH_MIN_DWELL_MS = 12000;
    // Rough room center; back-phase home spots are fanned on the arc of each
    // caster that faces this point, so melee stack on the open (inward) side and
    // never path through the boss or into the wall behind it.
    const std::pair<float, float> roomCenter = {2528.0f, -2957.0f};
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
        _backBossCommit = ObjectGuid::Empty;
        _lastSwitchMs = 0;
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
            // swap sides; the back healers rotate beside them (BackHealerSide).
            // Picking by ilvl (not roster order) puts your strongest players on
            // the back, which is the rough spot.
            return IsTopRangedDpsByItemLevel(bot, 2);
        }
        return botAI->IsAssistRangedDpsOfIndex(bot, 0) || botAI->IsAssistHealOfIndex(bot, 0);
    }
    // How many of the highest-ilvl healers run the back-caster rotation. Three
    // by default; once the first horseman is down (the raid burns Thane first,
    // so a front boss) the front needs less healing, so a fourth healer joins
    // the back — the "extra healer back from the front" on first kill.
    uint8 BackHealerCount() { return FirstHorsemanDead() ? 4 : 3; }
    // 25-man back healers: the top-N highest-ilvl healers, rotating beside the
    // two back casters instead of soaking the center. Used to route their park
    // spot. (10-man keeps every healer at healerMidPos — unchanged.)
    bool IsBackHealer(Player* bot)
    {
        return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL &&
               IsTopHealByItemLevel(bot, BackHealerCount());
    }
    // Which back caster a back healer is currently parked next to (0 = Sir
    // Zeliek, 1 = Lady Blaumeux). Healers are split across the two sides by
    // ilvl-rank parity so both soakers always have a healer nearby, and the
    // whole set flips every HEALER_SIDE_SWAP_MS — they trade sides, preserving
    // per-side coverage while each sheds the Mark it was accumulating. Computed
    // from a free-running clock + rank so every helper instance agrees.
    int BackHealerSide(Player* bot)
    {
        int startSide = RoleRankByItemLevel(bot, true) % 2;
        uint32 swaps = getMSTime() / HEALER_SIDE_SWAP_MS;
        return (startSide + swaps) % 2;
    }
    // Park spot for a non-attractor healer. Back healers go to their current
    // side's spot (rotating); a second healer sharing a side is nudged a few
    // yards toward the same caster (staying inside that one Mark) so they don't
    // pile on one point and jitter. Everyone else (10-man, or any healer past
    // the back count) covers the front tanks from healerMidPos.
    std::pair<float, float> HealerParkPos(Player* bot)
    {
        if (!IsBackHealer(bot))
            return healerMidPos;
        int side = BackHealerSide(bot);
        std::pair<float, float> base = healerSidePos[side];
        uint8 pairIdx = RoleRankByItemLevel(bot, true) / 2;  // 0 or 1 per side
        float nudge = pairIdx * 4.0f;
        float dirX = (side == 0) ? 1.0f : -1.0f;  // toward the assigned caster
        return {base.first + dirX * nudge, base.second};
    }
    // Count of horsemen still alive, read from the bot's threat list (range-
    // independent — a healer keeps threat on every engaged horseman). A dead
    // horseman drops off the list, so "find target" returns null for it.
    uint8 AliveHorsemenCount()
    {
        Unit* thane = AI_VALUE2(Unit*, "find target", "thane korth'azz");
        Unit* lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
        Unit* sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
        Unit* baron = AI_VALUE2(Unit*, "find target", "baron rivendare");
        if (!baron)
            baron = AI_VALUE2(Unit*, "find target", "highlord mograine");
        uint8 n = 0;
        for (Unit* u : {thane, lady, sir, baron})
            if (u && u->IsAlive())
                ++n;
        return n;
    }
    // True once at least one horseman is down. The alive>=1 guard keeps a
    // not-yet-threatened pull (all four read as "missing") from looking like
    // four deaths, and the time gate covers the brief window before threat
    // settles on every horseman.
    bool FirstHorsemanDead()
    {
        if (_combat_start_ms == 0 || getMSTime() - _combat_start_ms < FIRST_KILL_EARLIEST_MS)
            return false;
        uint8 alive = AliveHorsemenCount();
        return alive >= 1 && alive <= 3;
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

    // --- Instance-script lookups (threat-independent) -----------------------
    // True while the Four Horsemen encounter is actually in progress. Gates the
    // back-phase logic so a bot fighting another Naxx wing never picks up the
    // idle horsemen, and so nothing fires before the pull.
    bool EncounterEngaged()
    {
        InstanceScript* instance = bot->GetInstanceScript();
        return instance && instance->GetBossState(BOSS_HORSEMAN) == IN_PROGRESS;
    }
    // The horseman behind a DATA_*_BOSS id if it's alive, else null. Uses the
    // instance script, so it works regardless of whether THIS bot has ever
    // threatened that horseman (unlike "find target").
    Unit* GetHorsemanAlive(uint32 dataId)
    {
        InstanceScript* instance = bot->GetInstanceScript();
        if (!instance)
            return nullptr;
        Creature* c = instance->GetCreature(dataId);
        return (c && c->IsAlive()) ? c : nullptr;
    }
    Unit* ThaneAlive() { return GetHorsemanAlive(DATA_THANE_KORTHAZZ_BOSS); }
    Unit* BaronAlive() { return GetHorsemanAlive(DATA_BARON_RIVENDARE_BOSS); }
    Unit* LadyAlive() { return GetHorsemanAlive(DATA_LADY_BLAUMEUX_BOSS); }
    Unit* SirAlive() { return GetHorsemanAlive(DATA_SIR_ZELIEK_BOSS); }
    // The two melee bosses (tanked in the front corners) are both dead → the
    // raid should collapse onto the back casters. Threat-independent so the
    // whole front team detects it, not just bots that touched the casters.
    bool FrontPairDead() { return !ThaneAlive() && !BaronAlive(); }

    // Which Mark spell a back caster applies, identified by entry (the Mark ids
    // are the same on 10/25, cast self-AoE by the boss — see boss_four_horsemen).
    uint32 MarkSpellForBoss(Unit* boss)
    {
        if (!boss)
            return 0;
        switch (boss->GetEntry())
        {
            case NPC_LADY_BLAUMEUX:
                return NaxxSpellIds::MarkOfBlaumeux;
            case NPC_SIR_ZELIEK:
                return NaxxSpellIds::MarkOfZeliek;
            default:
                return 0;
        }
    }
    uint32 MarkStacksFromBoss(Player* p, Unit* boss)
    {
        uint32 spellId = MarkSpellForBoss(boss);
        if (!spellId)
            return 0;
        Aura* a = p->GetAura(spellId);
        return a ? a->GetStackAmount() : 0;
    }
    // Back-phase target selection: ping-pong between Lady and Sir to keep this
    // bot's Mark stacks low. Stays committed to one caster until its Mark hits
    // MARK_SWITCH_STACKS (and at least BACK_SWITCH_MIN_DWELL_MS have passed),
    // then commits to the other. Commit + dwell give hysteresis so bots don't
    // oscillate every tick. `lady`/`sir` are the alive units (null if dead).
    Unit* PickBackBoss(Player* p, Unit* lady, Unit* sir)
    {
        if (!lady && !sir)
        {
            _backBossCommit = ObjectGuid::Empty;
            return nullptr;
        }
        if (lady && !sir)
        {
            _backBossCommit = lady->GetGUID();
            return lady;
        }
        if (sir && !lady)
        {
            _backBossCommit = sir->GetGUID();
            return sir;
        }

        Unit* committed = nullptr;
        if (_backBossCommit == lady->GetGUID())
            committed = lady;
        else if (_backBossCommit == sir->GetGUID())
            committed = sir;

        if (!committed)
        {
            // First commit (or stale GUID): start on the one we carry fewer
            // stacks of.
            committed = (MarkStacksFromBoss(p, lady) <= MarkStacksFromBoss(p, sir)) ? lady : sir;
            _backBossCommit = committed->GetGUID();
            _lastSwitchMs = getMSTime();
            return committed;
        }

        uint32 dwell = _lastSwitchMs ? getMSTime() - _lastSwitchMs : BACK_SWITCH_MIN_DWELL_MS;
        if (dwell >= BACK_SWITCH_MIN_DWELL_MS && MarkStacksFromBoss(p, committed) >= MARK_SWITCH_STACKS)
        {
            committed = (committed == lady) ? sir : lady;
            _backBossCommit = committed->GetGUID();
            _lastSwitchMs = getMSTime();
        }
        return committed;
    }
    // Where a back-phase DPS should stand to hit `boss`: a fixed per-bot slot on
    // the inward-facing arc (so bots fan out instead of stacking on one point,
    // and stay on the open side of the boss), at melee reach or ~25y for ranged.
    // If the chosen point sits in a Void Zone, the angle is rotated around the
    // boss until it's clear — so the spot is puddle-free by construction. That's
    // what stops the old run-around: the previous back logic just Attack()ed,
    // which yanked melee straight back into Lady's puddle after every dodge.
    std::pair<float, float> BackPhaseHomePos(Player* p, Unit* boss, bool ranged)
    {
        float reach = ranged ? 25.0f : (boss->GetCombatReach() + p->GetCombatReach() + 1.0f);
        uint32 slot = botAI->GetGroupSlotIndex(p);
        float base = std::atan2(roomCenter.second - boss->GetPositionY(),
                                roomCenter.first - boss->GetPositionX());
        float spread = ranged ? 0.18f : 0.30f;
        int sign = (slot % 2 == 0) ? 1 : -1;
        float angle = base + sign * spread * float((slot + 1) / 2);

        for (int i = 0; i < 6; ++i)
        {
            float px = boss->GetPositionX() + std::cos(angle) * reach;
            float py = boss->GetPositionY() + std::sin(angle) * reach;
            if (!FindVoidZoneAt(px, py))
                return {px, py};
            angle += 0.6f;  // ~34 deg steps around the boss until clear of puddles
        }
        return {boss->GetPositionX() + std::cos(base) * (reach + 6.0f),
                boss->GetPositionY() + std::sin(base) * (reach + 6.0f)};
    }

protected:
    Unit* _sir = nullptr;
    Unit* _lady = nullptr;
    uint32 _combat_start_ms = 0;
    int posToGo = 0;
    ObjectGuid _backBossCommit = ObjectGuid::Empty;
    uint32 _lastSwitchMs = 0;
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
