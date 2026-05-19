/*
 * Custom CreatureScript: Instance Manager (entry 600005)
 *
 * Lists the player's persistent instance binds (raids + heroic dungeons) and
 * lets them reset any one of them, or reset all at once. Free for now; the
 * thought is to later hook a currency cost.
 *
 * Why this exists: solo-raid play means re-running tiers from the start with a
 * bot group. The vanilla weekly reset is too slow; .instance unbind is GM-only
 * and only unbinds the issuer. With bots, we need to unbind the entire saved
 * roster -- otherwise the player resets but the bots stay locked, and the
 * group can't re-enter the fresh instance.
 *
 * How it actually wipes the lock for everyone:
 *   sInstanceSaveMgr->UnbindAllFor(save)
 *
 * iterates save->m_playerList -- which is every character (including bots)
 * currently saved to that specific instance id -- and calls
 * PlayerUnbindInstance(..., deleteFromDB=true) on each. Offline bots get
 * unbound via DB write; online ones also have their live Player state
 * updated. Once the last player is unbound, AC auto-cleans the InstanceSave,
 * and the next zone-in spawns a fresh instance.
 *
 * AC's Group object has no separate bind storage in this fork -- group binds
 * piggyback on the group leader's player binds (see Group.cpp:2147), so
 * "unbind every player saved to this instance" also implicitly clears the
 * group bind.
 *
 * Skips the current map: if the player is standing inside an instance they're
 * bound to, we can't reset that one (matches the .instance unbind safety).
 */

#include "Chat.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "InstanceSaveMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

#include <sstream>
#include <string>

namespace
{
    constexpr uint32 INSTANCE_MANAGER_TEXT_ID = 60010;
    constexpr uint32 SENDER_MAIN              = 300;

    // Control codes: high bits well clear of any packed (mapId,diff) value.
    constexpr uint32 ACTION_CLOSE             = 0xFA000001;
    constexpr uint32 ACTION_BACK              = 0xFA000002;
    constexpr uint32 ACTION_RESET_ALL_PROMPT  = 0xFA000003;
    constexpr uint32 ACTION_RESET_ALL_CONFIRM = 0xFA000004;

    // Per-bind actions encode (mapId << 4) | difficulty into the low 20 bits.
    // mapId is uint16 (16 bits), difficulty is 0-3 (2 bits) -- fits cleanly.
    // Prefix byte distinguishes "open confirm submenu" from "perform reset".
    constexpr uint32 ACTION_BIND_PROMPT_BASE  = 0xC0000000;
    constexpr uint32 ACTION_BIND_CONFIRM_BASE = 0xC1000000;
    constexpr uint32 ACTION_BIND_PACK_MASK    = 0x00FFFFFF;

    uint32 PackBind(uint32 mapId, uint8 difficulty)
    {
        return ((mapId & 0xFFFF) << 4) | (difficulty & 0xF);
    }

    void UnpackBind(uint32 packed, uint32& mapId, uint8& difficulty)
    {
        mapId      = (packed >> 4) & 0xFFFF;
        difficulty = packed & 0xF;
    }

    char const* RaidDifficultyLabel(Difficulty d)
    {
        switch (d)
        {
            case RAID_DIFFICULTY_10MAN_NORMAL: return "10N";
            case RAID_DIFFICULTY_25MAN_NORMAL: return "25N";
            case RAID_DIFFICULTY_10MAN_HEROIC: return "10H";
            case RAID_DIFFICULTY_25MAN_HEROIC: return "25H";
            default:                           return "?";
        }
    }

    char const* DungeonDifficultyLabel(Difficulty d)
    {
        switch (d)
        {
            case DUNGEON_DIFFICULTY_NORMAL: return "Normal";
            case DUNGEON_DIFFICULTY_HEROIC: return "Heroic";
            default:                        return "?";
        }
    }

    std::string DifficultyLabelFor(MapEntry const* mapEntry, Difficulty d)
    {
        if (mapEntry && mapEntry->IsRaid())
            return RaidDifficultyLabel(d);
        return DungeonDifficultyLabel(d);
    }

    std::string TimeToResetStr(InstanceSave const* save, bool extended)
    {
        time_t resetTime = extended ? save->GetExtendedResetTime() : save->GetResetTime();
        time_t now       = GameTime::GetGameTime().count();
        uint32 ttr       = uint32(resetTime >= now ? resetTime - now : 0);
        return secsToTimeString(ttr);
    }

    std::string BindLabel(uint32 mapId, InstancePlayerBind const& bind)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        char const* mapName = (mapEntry && mapEntry->name[0]) ? mapEntry->name[0] : "Unknown";

        InstanceSave const* save = bind.save;
        Difficulty diff = save ? save->GetDifficulty() : Difficulty(DUNGEON_DIFFICULTY_NORMAL);

        std::ostringstream ss;
        ss << mapName << " (" << DifficultyLabelFor(mapEntry, diff) << ")";
        if (save)
            ss << " - " << TimeToResetStr(save, bind.extended);
        return ss.str();
    }

    // Walk every difficulty slot of every bind for the player and run `fn`
    // against each (mapId, difficulty, bind). Skips binds whose map is the
    // one the player is currently standing in.
    template <typename Fn>
    void ForEachResettableBind(Player* player, Fn&& fn)
    {
        uint32 currentMap = player->GetMapId();
        for (uint8 d = 0; d < MAX_DIFFICULTY; ++d)
        {
            BoundInstancesMap const& binds =
                sInstanceSaveMgr->PlayerGetBoundInstances(player->GetGUID(), Difficulty(d));
            for (auto const& [mapId, bind] : binds)
            {
                if (mapId == currentMap)
                    continue;
                fn(mapId, Difficulty(d), bind);
            }
        }
    }

    void ShowRootMenu(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        uint32 listed = 0;
        ForEachResettableBind(player, [&](uint32 mapId, Difficulty d, InstancePlayerBind const& bind)
        {
            std::string label = BindLabel(mapId, bind);
            uint32 action = ACTION_BIND_PROMPT_BASE | PackBind(mapId, uint8(d));
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, SENDER_MAIN, action);
            ++listed;
        });

        if (!listed)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                             "You have no instance locks to reset.",
                             SENDER_MAIN, ACTION_CLOSE);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE,
                             "Reset all instance locks...",
                             SENDER_MAIN, ACTION_RESET_ALL_PROMPT);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nevermind.",
                         SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player, INSTANCE_MANAGER_TEXT_ID, creature->GetGUID());
    }

    void ShowConfirmSingle(Player* player, Creature* creature,
                           uint32 mapId, Difficulty diff)
    {
        InstancePlayerBind* bind =
            sInstanceSaveMgr->PlayerGetBoundInstance(player->GetGUID(), mapId, diff);
        if (!bind)
        {
            ShowRootMenu(player, creature);
            return;
        }

        ClearGossipMenuFor(player);

        std::string label = BindLabel(mapId, *bind);
        std::ostringstream prompt;
        prompt << "Reset " << label
               << "? Everyone saved to this instance gets unbound.";

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, prompt.str(),
                         SENDER_MAIN, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Yes, reset it.",
                         SENDER_MAIN,
                         ACTION_BIND_CONFIRM_BASE | PackBind(mapId, uint8(diff)));
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "No, go back.",
                         SENDER_MAIN, ACTION_BACK);

        SendGossipMenuFor(player, INSTANCE_MANAGER_TEXT_ID, creature->GetGUID());
    }

    void ShowConfirmAll(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         "Reset ALL of your instance locks?",
                         SENDER_MAIN, ACTION_BACK);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Yes, reset them all.",
                         SENDER_MAIN, ACTION_RESET_ALL_CONFIRM);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "No, go back.",
                         SENDER_MAIN, ACTION_BACK);
        SendGossipMenuFor(player, INSTANCE_MANAGER_TEXT_ID, creature->GetGUID());
    }

    // Resets one bind. Caches name/difficulty before the unbind call because
    // UnbindAllFor may destroy the InstanceSave (and the bind row) once the
    // last player is unbound.
    void ResetOne(Player* player, uint32 mapId, Difficulty diff)
    {
        InstancePlayerBind* bind =
            sInstanceSaveMgr->PlayerGetBoundInstance(player->GetGUID(), mapId, diff);
        if (!bind || !bind->save)
            return;

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        std::string mapName = (mapEntry && mapEntry->name[0]) ? mapEntry->name[0] : "Unknown";
        std::string diffStr = DifficultyLabelFor(mapEntry, diff);

        sInstanceSaveMgr->UnbindAllFor(bind->save);

        ChatHandler(player->GetSession()).PSendSysMessage(
            "Reset {} ({}).", mapName, diffStr);
    }

    void ResetAll(Player* player)
    {
        // Snapshot first; UnbindAllFor mutates the underlying bind maps while
        // we iterate, so we'd invalidate the iterator if we acted in-place.
        struct Entry { uint32 mapId; Difficulty diff; };
        std::vector<Entry> targets;
        ForEachResettableBind(player, [&](uint32 mapId, Difficulty d, InstancePlayerBind const&)
        {
            targets.push_back({mapId, d});
        });

        uint32 count = 0;
        for (Entry const& e : targets)
        {
            InstancePlayerBind* bind =
                sInstanceSaveMgr->PlayerGetBoundInstance(player->GetGUID(), e.mapId, e.diff);
            if (!bind || !bind->save)
                continue;
            sInstanceSaveMgr->UnbindAllFor(bind->save);
            ++count;
        }

        ChatHandler(player->GetSession()).PSendSysMessage(
            "Reset {} instance lock{}.", count, count == 1 ? "" : "s");
    }
}

class npc_instance_manager : public CreatureScript
{
public:
    npc_instance_manager() : CreatureScript("npc_instance_manager") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowRootMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 /*sender*/, uint32 action) override
    {
        if (action == ACTION_CLOSE)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (action == ACTION_BACK)
        {
            ShowRootMenu(player, creature);
            return true;
        }

        if (action == ACTION_RESET_ALL_PROMPT)
        {
            ShowConfirmAll(player, creature);
            return true;
        }

        if (action == ACTION_RESET_ALL_CONFIRM)
        {
            ResetAll(player);
            ShowRootMenu(player, creature);
            return true;
        }

        uint32 prefix = action & 0xFF000000;
        if (prefix == ACTION_BIND_PROMPT_BASE)
        {
            uint32 mapId;
            uint8 diff;
            UnpackBind(action & ACTION_BIND_PACK_MASK, mapId, diff);
            ShowConfirmSingle(player, creature, mapId, Difficulty(diff));
            return true;
        }

        if (prefix == ACTION_BIND_CONFIRM_BASE)
        {
            uint32 mapId;
            uint8 diff;
            UnpackBind(action & ACTION_BIND_PACK_MASK, mapId, diff);
            ResetOne(player, mapId, Difficulty(diff));
            ShowRootMenu(player, creature);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_npc_instance_manager()
{
    new npc_instance_manager();
}
