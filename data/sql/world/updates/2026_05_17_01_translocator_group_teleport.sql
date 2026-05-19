-- Gimped Translocator (entry 600000): teleport the invoker's whole party/raid
-- instead of just the clicker.
--
-- SMART_TARGET_ACTION_INVOKER (7)  -> only the player who picked the option
-- SMART_TARGET_INVOKER_PARTY  (16) -> every group/raid member currently in the
--                                     same map as the invoker (filtered by
--                                     IsInMap in SmartScript.cpp). Raids share
--                                     the same Group object as parties, so the
--                                     same target type covers 5/10/25-man.
--
-- Only the SMART_ACTION_TELEPORT (62) rows change. The SMART_ACTION_CLOSE_GOSSIP
-- (72) rows stay on target type 7 because only the clicker's gossip UI needs
-- closing; firing close-gossip on every party member would be a no-op for any
-- who weren't talking to the NPC but is still wasteful packet traffic.
--
-- includePets is left at 0: cross-map NearTeleportTo on a pet would strand it
-- on the old map; the client/server auto-resummons hunter/warlock pets on the
-- destination map after the master arrives, which is what we want.

UPDATE `smart_scripts`
   SET `target_type` = 16
 WHERE `source_type` = 0
   AND `entryorguid` = 600000
   AND `action_type` = 62;
