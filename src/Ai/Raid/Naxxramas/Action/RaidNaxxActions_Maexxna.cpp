#include "RaidNaxxActions.h"

#include "Playerbots.h"

bool MaexxnaAttackWebWrapAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
        return false;

    Unit* wrap = helper.GetClosestWebWrap();
    if (!wrap)
        return false;

    if (context->GetValue<Unit*>("current target")->Get() == wrap)
        return false;

    return Attack(wrap);
}
