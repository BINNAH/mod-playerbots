#include "JsonStrategyCommands.h"

#include "JsonStrategyLoader.h"
#include "Playerbots.h"

#include <fmt/core.h>
#include <functional>
#include <string>
#include <vector>

using namespace Acore::ChatCommands;

// Mirrors PlayerbotAI::ApplyInstanceStrategies' instance-strategy list. Used by
// `.rjson on` to strip whatever C++ instance strategy a bot currently runs so
// the JSON rules don't double-fire against it. `.rjson off` restores via
// ApplyInstanceStrategies, so it doesn't need this list.
static std::vector<std::string> const kInstanceStrategies = {
    "aq20", "bwl", "karazhan", "gruulslair", "icc", "magtheridon", "moltencore",
    "naxx", "onyxia", "ssc", "tbc-ac", "tempestkeep", "ulduar", "voa", "wotlk-an", "wotlk-cos",
    "wotlk-dtk", "wotlk-eoe", "wotlk-fos", "wotlk-gd", "wotlk-hol", "wotlk-hor",
    "wotlk-hos", "wotlk-nex", "wotlk-occ", "wotlk-ok", "wotlk-os", "wotlk-pos",
    "wotlk-toc", "wotlk-uk", "wotlk-up", "wotlk-vh", "zulaman"
};

static Player* MasterFromHandler(ChatHandler* handler)
{
    return handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
}

// Run fn on each playerbot owned by `master`. Returns how many bots were visited.
static uint32 ForEachOwnedBot(Player* master, std::function<void(PlayerbotAI*, Player*)> const& fn)
{
    PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(master);
    if (!mgr)
        return 0;

    uint32 count = 0;
    for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
    {
        Player* bot = it->second;
        if (!bot)
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (!ai)
            continue;
        fn(ai, bot);
        ++count;
    }
    return count;
}

ChatCommandTable RaidJsonCommandScript::GetCommands() const
{
    static ChatCommandTable rjsonTable =
    {
        { "reload", HandleReloadCommand, SEC_GAMEMASTER, Console::Yes },
        { "on",     HandleOnCommand,     SEC_GAMEMASTER, Console::No  },
        { "off",    HandleOffCommand,    SEC_GAMEMASTER, Console::No  },
        { "status", HandleStatusCommand, SEC_GAMEMASTER, Console::Yes },
    };

    static ChatCommandTable table =
    {
        { "rjson", rjsonTable }
    };

    return table;
}

bool RaidJsonCommandScript::HandleReloadCommand(ChatHandler* handler)
{
    RaidJsonRuleSet& rs = RaidJsonRuleSet::instance();
    rs.Load();

    uint32 reinit = 0;
    if (Player* master = MasterFromHandler(handler))
    {
        ForEachOwnedBot(master, [&reinit](PlayerbotAI* ai, Player* /*bot*/)
        {
            bool did = false;
            if (ai->HasStrategy("json-raid", BOT_STATE_COMBAT))
            {
                ai->ChangeStrategy("-json-raid,+json-raid", BOT_STATE_COMBAT);
                did = true;
            }
            if (ai->HasStrategy("json-raid", BOT_STATE_NON_COMBAT))
            {
                ai->ChangeStrategy("-json-raid,+json-raid", BOT_STATE_NON_COMBAT);
                did = true;
            }
            if (did)
                ++reinit;
        });
    }

    handler->SendSysMessage(fmt::format("RaidJson: reloaded {} rule(s) from {} file(s), {} error(s); re-inited {} bot(s).",
                                        rs.RuleCount(), rs.FileCount(), (uint32)rs.Errors().size(), reinit));
    for (size_t i = 0; i < rs.Errors().size() && i < 8; ++i)
        handler->SendSysMessage(fmt::format("  err: {}", rs.Errors()[i]));
    return true;
}

bool RaidJsonCommandScript::HandleOnCommand(ChatHandler* handler)
{
    Player* master = MasterFromHandler(handler);
    if (!master)
    {
        handler->SendSysMessage("RaidJson: this command must be used in-game.");
        return true;
    }

    RaidJsonRuleSet& rs = RaidJsonRuleSet::instance();
    if (!rs.Loaded())
        rs.Load();

    uint32 count = ForEachOwnedBot(master, [](PlayerbotAI* ai, Player* bot)
    {
        // Mark this bot json-raid-exclusive FIRST, so the guard in
        // ApplyInstanceStrategies refuses to re-attach the C++ instance strategy
        // on any later worldport / ResetStrategies (the leak we're closing).
        RaidJsonMode::instance().Set(bot->GetGUID(), true);
        for (std::string const& s : kInstanceStrategies)
        {
            if (ai->HasStrategy(s, BOT_STATE_COMBAT))
                ai->ChangeStrategy("-" + s, BOT_STATE_COMBAT);
            if (ai->HasStrategy(s, BOT_STATE_NON_COMBAT))
                ai->ChangeStrategy("-" + s, BOT_STATE_NON_COMBAT);
        }
        ai->ChangeStrategy("+json-raid", BOT_STATE_COMBAT);
        ai->ChangeStrategy("+json-raid", BOT_STATE_NON_COMBAT);
    });

    handler->SendSysMessage(fmt::format("RaidJson: json-raid ON for {} bot(s); {} rule(s) active. C++ instance strategy suppressed.",
                                        count, rs.RuleCount()));
    if (rs.RuleCount() == 0)
        handler->SendSysMessage("RaidJson: WARNING — 0 rules loaded. Check `.rjson status`.");
    return true;
}

bool RaidJsonCommandScript::HandleOffCommand(ChatHandler* handler)
{
    Player* master = MasterFromHandler(handler);
    if (!master)
    {
        handler->SendSysMessage("RaidJson: this command must be used in-game.");
        return true;
    }

    uint32 count = ForEachOwnedBot(master, [](PlayerbotAI* ai, Player* bot)
    {
        // Clear the exclusivity flag FIRST so ApplyInstanceStrategies below is
        // allowed to re-attach the proper C++ instance strategy.
        RaidJsonMode::instance().Set(bot->GetGUID(), false);
        if (ai->HasStrategy("json-raid", BOT_STATE_COMBAT))
            ai->ChangeStrategy("-json-raid", BOT_STATE_COMBAT);
        if (ai->HasStrategy("json-raid", BOT_STATE_NON_COMBAT))
            ai->ChangeStrategy("-json-raid", BOT_STATE_NON_COMBAT);
        // Restore the proper C++ strategy for whatever map the bot is on.
        ai->ApplyInstanceStrategies(bot->GetMapId());
    });

    handler->SendSysMessage(fmt::format("RaidJson: json-raid OFF for {} bot(s); C++ instance strategy restored.", count));
    return true;
}

bool RaidJsonCommandScript::HandleStatusCommand(ChatHandler* handler)
{
    RaidJsonRuleSet& rs = RaidJsonRuleSet::instance();

    handler->SendSysMessage(fmt::format("RaidJson status: loaded={} dir={}", rs.Loaded() ? "yes" : "no", rs.SourceDir()));
    handler->SendSysMessage(fmt::format("  files={} rules={} errors={}", rs.FileCount(), rs.RuleCount(),
                                        (uint32)rs.Errors().size()));
    for (size_t i = 0; i < rs.Errors().size() && i < 8; ++i)
        handler->SendSysMessage(fmt::format("  err: {}", rs.Errors()[i]));

    if (Player* master = MasterFromHandler(handler))
    {
        uint32 active = 0, total = 0, leaked = 0;
        ForEachOwnedBot(master, [&active, &total, &leaked](PlayerbotAI* ai, Player* /*bot*/)
        {
            ++total;
            bool json = ai->HasStrategy("json-raid", BOT_STATE_COMBAT);
            if (json)
                ++active;
            // Leak check: a json-raid bot must have NO C++ instance strategy
            // attached, or it's ambiguous which one drives the fight.
            if (json)
                for (std::string const& s : kInstanceStrategies)
                    if (ai->HasStrategy(s, BOT_STATE_COMBAT))
                    {
                        ++leaked;
                        break;
                    }
        });
        handler->SendSysMessage(fmt::format("  your bots running json-raid: {}/{}", active, total));
        if (leaked > 0)
            handler->SendSysMessage(fmt::format("  WARNING: {} bot(s) ALSO have a C++ instance strategy attached (leak!) — re-run `.rjson on`", leaked));
        else if (active > 0)
            handler->SendSysMessage("  exclusivity OK: no C++ instance strategy on json-raid bots.");
    }
    return true;
}

void AddRaidJsonStrategyCommandScripts()
{
    new RaidJsonCommandScript();
}
