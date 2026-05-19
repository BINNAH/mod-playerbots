-- Bot Creator NPC (entry 600002): five-step wizard for spawning new
-- playerbot characters. Sits in front of the IF bank next to the existing
-- Botmaster (600001) and Translocator (600000).

-- creature_template ---------------------------------------------------------
DELETE FROM `creature_template` WHERE `entry` = 600002;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `gossip_menu_id`,
     `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`,
     `speed_walk`, `speed_run`, `detection_range`, `unit_class`,
     `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`,
     `type`, `RegenHealth`, `flags_extra`, `ScriptName`)
VALUES
    (600002, 'Bot Creator', 'Bot Architect', 60003,
     80, 80, 0, 35, 1,
     1, 1.14286, 20, 1,
     0, 0, 1, 1,
     7, 1, 2, 'npc_botcreator');

-- Display model: 5037 — same humanoid as the Botmaster for visual consistency.
-- Swap this row out later if you want a distinct skin.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 600002;
INSERT INTO `creature_template_model`
    (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES
    (600002, 0, 5037, 1, 1);

-- npc_text: shown above the gossip menu. The wizard rebuilds its body
-- per-step in code, so this is just a flavor header.
DELETE FROM `npc_text` WHERE `ID` = 60003;
INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`)
VALUES
    (60003, 'I can spin up a new bot for you. What did you have in mind?', 1);

-- creature spawn: next to the Botmaster in front of IF bank.
-- guid 9000002 sits in our reserved 9000000+ range.
DELETE FROM `creature` WHERE `guid` = 9000002;
INSERT INTO `creature`
    (`guid`, `id1`, `map`, `zoneId`, `areaId`,
     `spawnMask`, `phaseMask`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`,
     `spawntimesecs`, `wander_distance`, `MovementType`, `curhealth`,
     `Comment`)
VALUES
    (9000002, 600002, 0, 1537, 1538,
     1, 1, 0,
     -4914.78, -968, 501.66, 5.4,
     300, 0, 0, 1,
     'Custom: Bot Creator (next to Botmaster)');
