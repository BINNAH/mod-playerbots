#include "Playerbots.h"
#include "RaidNaxxActions.h"

bool HeiganFollowMasterAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master || !master->IsInWorld() || master->isDead())
        return false;

    if (bot->GetMapId() != master->GetMapId())
        return false;

    // Drop whatever the bot is doing — casting through the dance kills you.
    botAI->InterruptSpell();

    // Stick to the master's exact tile. MOVEMENT_COMBAT outranks DPS movement
    // so the bot stays glued even mid-rotation.
    return MoveTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(),
                  master->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool HeiganRangedPositionAction::Execute(Event /*event*/)
{
    // If we're already on the stand spot, hand control back so the normal
    // DPS rotation can run.
    if (bot->IsWithinDist2d(kRangedX, kRangedY, kInPositionTolerance))
        return false;

    return MoveTo(bot->GetMapId(), kRangedX, kRangedY, kRangedZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
