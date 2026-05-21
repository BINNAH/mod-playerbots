#include "RaidNaxxActions.h"

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"

bool GluthChooseTargetAction::Execute(Event /*event*/)
{
    Unit* target_boss = AI_VALUE2(Unit*, "find target", "gluth");
    if (!target_boss || !target_boss->IsAlive())
        return false;

    if (context->GetValue<Unit*>("current target")->Get() == target_boss)
        return false;

    return Attack(target_boss, true);
}

bool GluthPositionAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    bool raid25 = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL;

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0) || botAI->IsAssistTankOfIndex(bot, 1))
    {
        if (!AI_VALUE2(bool, "has aggro", "boss target"))
            return false;

        float tankX = raid25 ? helper.mainTankPos25.first : helper.mainTankPos10.first;
        float tankY = raid25 ? helper.mainTankPos25.second : helper.mainTankPos10.second;
        return MoveTo(NAXX_MAP_ID, tankX, tankY, bot->GetPositionZ(), false, false, false,
                      false, MovementPriority::MOVEMENT_COMBAT);
    }

    float tankX = raid25 ? helper.mainTankPos25.first : helper.mainTankPos10.first;
    float tankY = raid25 ? helper.mainTankPos25.second : helper.mainTankPos10.second;

    if (botAI->IsRangedDps(bot))
        return MoveTo(NAXX_MAP_ID, tankX + 10.0f, tankY + 10.0f, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    if (botAI->IsHeal(bot))
        return MoveTo(NAXX_MAP_ID, tankX + 7.0f, tankY + 7.0f, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    return false;
}

bool GluthSlowdownAction::Execute(Event /*event*/)
{
    return false;
}
