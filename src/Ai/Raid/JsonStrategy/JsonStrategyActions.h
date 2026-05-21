/*
 * JSON-driven raid strategy — generic Level-2 actions.
 *
 * One C++ class per catalog "shape". Parameters arrive through the factory's
 * "::qualifier" channel (see NamedObjectContext): the factory builds the base
 * object then calls Qualify(<csv params>), which we parse. Each distinct
 * qualifier is cached as its own instance, so editing JSON params and reloading
 * yields a fresh action with the new numbers (no rebuild).
 */
#ifndef _PLAYERBOT_JSONSTRATEGYACTIONS_H
#define _PLAYERBOT_JSONSTRATEGYACTIONS_H

#include "MovementActions.h"        // RotateAroundTheCenterPointAction
#include "NamedObjectContext.h"     // Qualified

// Shape A2 "orbit_point": continuously walk the ring around (x, y).
//
// NOTE: this is the GENERIC orbit, not a clone of the bespoke C++
// AnubrekhanPositionAction (which only kites the tank during Locust Swarm and
// spreads ranged otherwise). Here every bot under the trigger orbits while the
// trigger is active. Qualifier format: "x,y,radius,segments,clockwise".
class JsonOrbitPointAction : public RotateAroundTheCenterPointAction, public Qualified
{
public:
    JsonOrbitPointAction(PlayerbotAI* ai)
        : RotateAroundTheCenterPointAction(ai, "json orbit", 0.0f, 0.0f, 40.0f, 16) {}

    void Qualify(std::string const qual) override;
    bool isUseful() override { return _valid; }
    uint32 GetCurrWaypoint() override;
    std::string const getName() override { return "json orbit::" + qualifier; }

private:
    bool _valid = false;
};

#endif
