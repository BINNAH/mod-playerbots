/*
 * Non-combat movement action that walks the bot toward the per-bot target set
 * via AutopilotControl. Because it runs in the non-combat engine, the combat
 * engine naturally preempts it when trash aggros, and it resumes once combat
 * ends -- so "fight what you walk into, then keep going" works for free.
 */

#ifndef _PLAYERBOT_AUTOPILOTMOVEACTION_H
#define _PLAYERBOT_AUTOPILOTMOVEACTION_H

#include "MovementActions.h"

class PlayerbotAI;

class AutopilotMoveAction : public MovementAction
{
public:
    AutopilotMoveAction(PlayerbotAI* botAI) : MovementAction(botAI, "autopilot move") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif // _PLAYERBOT_AUTOPILOTMOVEACTION_H
