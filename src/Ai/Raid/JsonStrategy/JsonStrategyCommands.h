/*
 * JSON-driven raid strategy — `.rjson` chat commands.
 *
 *   .rjson reload   re-read JSON from disk, rebuild the rule set, and re-init
 *                   the issuing player's json-raid bots (no rebuild/restart).
 *   .rjson on       swap your bots off their C++ instance strategy onto
 *                   "json-raid" (A/B isolation).
 *   .rjson off      remove "json-raid" and restore the proper C++ strategy.
 *   .rjson status   show source dir, file/rule/error counts, and how many of
 *                   your bots are running json-raid.
 *   .rjson pull <boss>
 *                   "call the pull" AND select the active boss whose strategy
 *                   runs: pass a boss name (`.rjson pull thaddius`) or target the
 *                   boss and omit it. ONLY that boss's rules activate; errors if
 *                   the boss has no loaded strategy. Flags your bots engaged so
 *                   the manual_engage rules fire. Announces in party/raid.
 *   .rjson stop     clear the active boss / engage flag (re-arm or abort).
 */
#ifndef _PLAYERBOT_JSONSTRATEGYCOMMANDS_H
#define _PLAYERBOT_JSONSTRATEGYCOMMANDS_H

#include "ScriptMgr.h"
#include "Chat.h"

class RaidJsonCommandScript : public CommandScript
{
public:
    RaidJsonCommandScript() : CommandScript("RaidJsonCommandScript") {}

    Acore::ChatCommands::ChatCommandTable GetCommands() const override;

    static bool HandleReloadCommand(ChatHandler* handler);
    static bool HandleOnCommand(ChatHandler* handler);
    static bool HandleOffCommand(ChatHandler* handler);
    static bool HandleStatusCommand(ChatHandler* handler);
    static bool HandlePullCommand(ChatHandler* handler, Acore::ChatCommands::Tail bossName);
    static bool HandleStopCommand(ChatHandler* handler);
};

void AddRaidJsonStrategyCommandScripts();

#endif
