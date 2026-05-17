/*
 * Custom CreatureScript: Gimped Enchanter (entry 600004)
 *
 * One-click "enchant my gear" NPC. Reuses the exact code path the .autogear
 * command runs after it equips fresh gear on a bot, applied to the player's
 * own equipped items instead. See TrainerAction.cpp:305 for the original
 * call site -- the function picks the best legal enchant per slot via
 * StatsWeightCalculator and also fills empty gem sockets.
 *
 * The function is written against a Player*, so it works on real players as
 * well as bots; nothing inside it reaches into PlayerbotAI. The static enchant
 * and gem caches it reads (PlayerbotFactory::enchantSpellIdCache /
 * enchantGemIdCache) are populated once at server startup, regardless of bot
 * presence.
 *
 * Lives in mod-playerbots/Script because PlayerbotFactory is module-private;
 * the NPC has a hard dependency on the playerbots module being loaded.
 */

#include "Chat.h"
#include "Player.h"
#include "PlayerbotFactory.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

namespace
{
    constexpr uint32 ENCHANTER_TEXT_ID  = 60009;

    constexpr uint32 ACTION_ENCHANT     = GOSSIP_ACTION_INFO_DEF + 1;
    constexpr uint32 ACTION_CLOSE       = GOSSIP_ACTION_INFO_DEF + 2;
}

class npc_gimped_enchanter : public CreatureScript
{
public:
    npc_gimped_enchanter() : CreatureScript("npc_gimped_enchanter") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                         "Enchant my gear.",
                         GOSSIP_SENDER_MAIN, ACTION_ENCHANT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         "Nevermind.",
                         GOSSIP_SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player, ENCHANTER_TEXT_ID, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/,
                        uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        if (action != ACTION_ENCHANT)
            return true;

        // The factory's quality/gearScoreLimit args drive its own item-picking
        // decisions; ApplyEnchantAndGemsNew only reads the bound Player. Defaults
        // (0, 0) are fine.
        PlayerbotFactory factory(player, player->GetLevel());
        factory.ApplyEnchantAndGemsNew();

        ChatHandler(player->GetSession()).PSendSysMessage(
            "Your equipped gear has been enchanted.");
        return true;
    }
};

void AddSC_npc_gimped_enchanter()
{
    new npc_gimped_enchanter();
}
