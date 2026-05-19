-- Instance Manager NPC (entry 600005): lists the player's persistent instance
-- binds and lets them reset any one, or all of them. Unbinds every player
-- saved to the affected instance (including bots) so a fresh raid lockout
-- can be acquired without per-bot DB surgery.
--
-- Sits alongside the Botmaster (600001) / Bot Creator (600002) in front of
-- the Stormwind bank.

-- creature_template ---------------------------------------------------------
DELETE FROM `creature_template` WHERE `entry` = 600005;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `gossip_menu_id`,
     `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
     `speed_walk`, `speed_run`, `detection_range`, `unit_class`,
     `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
     `type`, `RegenHealth`, `flags_extra`, `ScriptName`)
VALUES
    (600005, 'Instance Manager', 'Lockout Wrangler', 60005,
     80, 80, 0, 35, 1,
     1, 1.14286, 20, 1,
     0, 0, 1, 1,
     7, 1, 2, 'npc_instance_manager');

-- Display model: 5037 — same humanoid the other custom NPCs use.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 600005;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (600005, 0, 5037, 1, 1);

-- npc_text: flavor header above the dynamically-built menu.
DELETE FROM `npc_text` WHERE `ID` = 60010;
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`)
VALUES
    (60010, 'Need to reset a raid lock? Pick one below.', 1);

-- creature spawn: clustered with the Botmaster / Bot Creator in front of the
-- SW bank. guid 9000005 sits in our reserved 9000000+ range.
DELETE FROM `creature` WHERE `guid` = 9000005;
INSERT INTO `creature`
    (`guid`, `id1`, `map`, `zoneId`, `areaId`,
     `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`,
     `spawntimesecs`, `wander_distance`, `MovementType`, `curhealth`,
     `Comment`)
VALUES
    (9000005, 600005, 0, 1537, 1538,
     1, 1, 0,
     -4912.78, -968, 501.66, 5.4,
     300, 0, 0, 1,
     'Custom: Instance Manager (next to Bot Creator)');
