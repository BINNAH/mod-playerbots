# Raid Bot AI — Class Inventory

Per-raid lookup of every boss-specific Action/Trigger class mapped to its shape ID
(see `RAID_AI_PATTERNS.md` for shape definitions). Use this to answer "which
existing class already does shape X?" before writing a new one.

> **Accuracy note:** The Naxxramas section is verified by direct read. The other
> raids were extracted by an automated audit pass from the `Action/` and `Trigger/`
> headers — class names and shape mappings are reliable for navigation, but verify
> exact constructor signatures / line numbers against the cited file before relying
> on them. Files live at `src/Ai/Raid/<Raid>/Action/Raid<X>Actions.h` and
> `.../Trigger/Raid<X>Triggers.h`.

Shape IDs: A1–A25 actions, T1–T12 triggers, plus **NICHE** for irreducibly
boss-specific behavior (keep in C++; see "Bespoke behaviors" in the patterns doc).

---

## Naxxramas *(verified)*

### Actions
| Class | Base | Shape |
|---|---|---|
| GrobbulusGoBehindAction | MovementAction | A3 |
| GrobbulusRotateAction | RotateAroundTheCenterPointAction | A2 |
| GrobbulusMoveCenterAction | MoveInsideAction | A1 |
| GrobbulusMoveAwayAction | MovementAction | A4 |
| HeiganPlatformAction | MovementAction | A1 |
| HeiganDanceAction | MovementAction | A6 (NICHE clock) |
| HeiganFollowMasterAction | MovementAction | A3 (manual fallback, unwired) |
| ThaddiusAttackNearestPetAction | AttackAction | A13 |
| ThaddiusMoveToPlatformAction | MovementAction | A1 |
| ThaddiusMovePolarityAction | MovementAction | A9 |
| RazuviousUseObedienceCrystalAction | MovementAction | A23 |
| RazuviousTargetAction | AttackAction | A12 |
| FourHorsemenAttractAlternativelyAction | AttackAction | A6/A1 |
| FourHorsemenAttackInOrderAction | AttackAction | A12 |
| FourHorsemenAvoidVoidZoneAction | MovementAction | A4 |
| FourHorsemenHealerBleedOffMarkAction | MovementAction | A4/A1 |
| SapphironGroundPositionAction | MovementAction | A1 |
| SapphironFlightPositionAction | MovementAction | A1/A5 |
| KelthuzadChooseTargetAction | AttackAction | A12 |
| KelthuzadPositionAction | MovementAction | A1 |
| KelthuzadControlPetAction | Action | A22 |
| MaexxnaAttackWebWrapAction | AttackAction | A13 |
| MaexxnaHandOfSacrificeOnMainTankAction | BuffOnMainTankAction | A21 |
| MaexxnaGuardianSpiritOnMainTankAction | HealPartyMemberAction | A21 |
| AnubrekhanChooseTargetAction | AttackAction | A12 |
| AnubrekhanPositionAction | RotateAroundTheCenterPointAction | A2 |
| GluthChooseTargetAction | AttackAction | A12 |
| GluthPositionAction | RotateAroundTheCenterPointAction | A2 |
| GluthSlowdownAction | Action | A20/A21 |
| LoathebPositionAction | MovementAction | A1 |
| LoathebChooseTargetAction | AttackAction | A12 |
| NothAddTankAction | AttackAction | A15 |

### Triggers
| Class | Base | Shape |
|---|---|---|
| MutatingInjectionMelee/RangedTrigger | HasAuraTrigger | T1 |
| MutatingInjectionRemovedTrigger | HasNoAuraTrigger | T1 |
| GrobbulusCloudTrigger | Trigger | T6 |
| HeiganFastDance / SlowDancePlatform / SlowDanceRangedTrigger | Trigger | T3 |
| Razuvious Tank / NontankTrigger | Trigger | T4 |
| KelthuzadTrigger | Trigger | T3 |
| AnubrekhanTrigger / FaerlinaTrigger / MaexxnaTrigger | Trigger | T7 |
| MaexxnaWebWrapTrigger | Trigger | T11 |
| Maexxna PreWebSpray Hand/Guardian/TankDefensiveTrigger | Trigger | T6 |
| Thaddius PhasePet / PhaseTransition / PhaseThaddiusTrigger | Trigger | T3 |
| ThaddiusPhasePetLoseAggroTrigger | Trigger | T8 |
| FourHorsemen Attractors / ExceptAttractorsTrigger | Trigger | T4 |
| FourHorsemenVoidZoneTrigger | Trigger | T5 |
| FourHorsemenHealerHighMarkTrigger | Trigger | T2 |
| FourHorsemenOpeningDefensiveTrigger | Trigger | T6 |
| Sapphiron Ground / FlightTrigger | Trigger | T3 |
| GluthTrigger | Trigger | T7 |
| GluthMainTankMortalWoundTrigger | Trigger | T2 (→A16) |
| LoathebTrigger | Trigger | T7 |
| NothAddTankTrigger | Trigger | T4 |

---

## Ulduar

### Actions
| Class | Shape |
|---|---|
| FlameLeviathanVehicleAction / EnterVehicleAction | A24 |
| Razorscale Avoid Devouring Flame / Sentinel / Whirlwind / FuseArmor | A4 |
| RazorscaleHarpoonAction | A23 |
| Razorscale IgnoreBoss / Grounded | A12 |
| XT002MoveAwayFromGroupAction | A7 |
| XT002OffTankPickupPummelerAction | A15 |
| XT002MainTankAttackBossAction | A12 |
| Kologarn MarkDpsTarget / RtiTarget | A19 |
| KologarnCrunchArmorAction | A16 |
| KologarnSpreadPositioningAction | A7 |
| Kologarn FallFromFloor / RubbleSlowdown | A11/A4 |
| KologarnEyebeamAction | A5 |
| AuriayaFallFromFloorAction | A11 |
| HodirMoveSnowpackedIcicleAction | A1 |
| HodirBitingColdJumpAction | A5 |
| FreyaMoveAwayNatureBombAction | A4 |
| FreyaMarkDpsTargetAction | A19 |
| FreyaMoveToHealingSporeAction | A1 |
| Thorim Arena / Gauntlet / Phase2 Positioning | A1 |
| ThorimUnbalancingStrikeAction | A16 |
| ThorimMarkDpsTargetAction | A19 |
| Mimiron Phase1 Positioning / RocketStrike / RapidBurst / ShockBlast | A1/A5 |
| MimironP3Wx2LaserBarrageAction | A5 |
| MimironAerialCommandUnitAction | A24 (NICHE) |
| MimironCheatAction / VezaxCheatAction | A25 |
| VezaxShadowCrashAction | A5 |
| VezaxMarkOfTheFacelessAction | A4 |
| YoggSaronDeathOrbAction | A4 |
| YoggSaronMaladyOfTheMindAction | A7 |
| YoggSaronBrainLinkAction | NICHE (pair-spacing) |
| YoggSaron Enter/Use/ExitPortalAction | A23 |
| YoggSaronIllusionRoomAction | NICHE (multi-room) |
| YoggSaronFallFromFloorAction | A11 |
| YoggSaronLunaticGazeAction | A3 |
| YoggSaronMarkTargetAction | A19 |
| YoggSaronPhase3PositioningAction | A1 |

### Triggers
Mostly T1 (auras), T3 (phase), T5 (near hazard/icicle/rune), T6 (mark timing),
T4 (roles), T7 (vehicle/portal/encounter state). YoggSaron* triggers derive from a
`YoggSaronTrigger` base (T3 multi-phase). `*FallFromFloorTrigger` = T12.

---

## Icecrown (ICC)

### Actions
| Class | Shape |
|---|---|
| IccLmTankPositionAction | A1 |
| IccSpikeAction | A12 |
| IccDarkReckoningAction | A4 |
| IccRangedPositionLadyDeathwhisperAction | A1 |
| IccAddsLadyDeathwhisperAction | A14 |
| IccShadeLadyDeathwhisperAction | A4 |
| IccRottingFrostGiantTankPositionAction | A1 |
| IccCannonFireAction | A24 |
| IccGunshipEnterCannonAction | A24 |
| IccGunshipTeleportAlly/HordeAction | A24 (NICHE traversal) |
| IccDbsTankPositionAction | A1/A15 |
| IccAddsDbsAction | A14 |
| IccFestergutGroupPositionAction | A1 |
| IccFestergutSporeAction | A7 |
| IccRotfaceTankPositionAction | A1/A12 |
| IccRotfaceGroupPositionAction | A4 |
| IccRotfaceMoveAwayFromExplosionAction | A4 |
| IccPutricide VolatileOoze / GasCloud / GrowingOozePuddle / AvoidMalleableGoo | A4/A5 |
| IccBpcKelesethTank / MainTank | A14/A19 |
| IccBpcEmpoweredVortexAction | A7 |
| IccBpcKineticBombAction | A12 |
| IccBpcBallOfFlameAction | A4 |
| IccBqlGroupPositionAction | NICHE (shadow-wall curves) |
| IccBqlPactOfDarkfallenAction | A8 |
| IccBqlVampiricBiteAction | NICHE (bite convert) |
| IccValkyreSpearAction | A14 |
| IccSisterSvalnaAction | A12 |
| IccValithriaGroupAction | A1 |
| IccValithriaPortalAction | A23 |
| IccValithriaHealAction | A21 |
| IccValithriaDreamCloudAction | A4 |
| IccSindragosaGroupPositionAction | A1 |
| IccSindragosaFrostBeaconAction | A7 |
| IccSindragosaBlisteringColdAction | A6 |
| IccSindragosaUnchainedMagic / MysticBuffet | A9 |
| IccSindragosaChilledToTheBoneAction | A12 |
| IccSindragosaFrostBombAction | A4 |
| IccSindragosaTankSwapPositionAction | A16 |
| IccLichKingShadowTrapAction | A4 |
| IccLichKingNecroticPlagueAction | A7 (NICHE kite) |
| IccLichKingWinterAction | NICHE (multi-role) |
| IccLichKingAddsAction | NICHE (heroic add mgmt) |

### Triggers
Same families: T1/T2 (frost/buffet stacks), T3 (phase/gunship), T4 (role), T5
(near ooze/cloud), T6 (mechanic windows), T11 (adds present).

---

## Trial of the Crusader (ToC)

### Actions
| Class | Shape |
|---|---|
| GormokMoveAwayFireBombAction | A4 |
| GormokAttackSnoboldAction | A13 |
| GormokTankSwapTauntAction | A16 |
| GormokSpreadAction | A7 |
| WormsRunToBurningBileAction | A1 |
| WormsAvoidSlimePoolAction | A4 |
| IcehowlDodgeChargeAction | A5 |

### Triggers
| Class | Shape |
|---|---|
| GormokNearFireBombTrigger | T5 |
| GormokSnoboldUpTrigger | T11 |
| GormokImpaleTankSwapTrigger | T2 |
| GormokSpreadTrigger | T9 |
| WormsParalyticToxinTrigger | T1 |
| WormsSlimePoolTrigger | T5 |
| IcehowlChargeTrigger | T6 |

---

## Eye of Eternity

### Actions
| Class | Shape |
|---|---|
| MalygosPositionAction | A1 |
| MalygosTargetAction | A12 |
| KillPowerSparkAction | A13 |
| EoEFlyDrakeAction | A24 |
| EoEDrakeAttackAction | A24 |

### Triggers
| MalygosTrigger | T3 |  | PowerSparkTrigger | T11 |

---

## Obsidian Sanctum

### Actions
| Class | Shape |
|---|---|
| SartharionTankPositionAction | A1/A9 (drake-aware) |
| AvoidTwilightFissureAction | A4 |
| AvoidFlameTsunamiAction | A4/A6 |
| SartharionAttackPriorityAction | A12 |
| Enter/ExitTwilightPortalAction | A23 |

### Triggers
SartharionTank/DpsTrigger = T4; FlameTsunami/TwilightFissureTrigger = T5;
MeleePositioningTrigger = T4+T3; TwilightPortalEnter/ExitTrigger = T4+T1.

---

## Vault of Archavon

### Actions
| EmalonMarkBossAction | A19 |  | EmalonLightingNovaAction | A4 |
| EmalonOverchargeAction | A19 |  | EmalonFallFromFloorAction | A11 |

### Triggers
EmalonMarkBoss/OverchargeTrigger = T4; LightingNovaTrigger = T10+T4;
EmalonFallFromFloorTrigger = T12 (Z<80).

---

## Onyxia

### Actions
| Class | Shape |
|---|---|
| RaidOnyxiaMoveToSideAction | A3 |
| RaidOnyxiaSpreadOutAction | A7 |
| RaidOnyxiaMoveToSafeZoneAction | A6 (deep breath) |
| RaidOnyxiaKillWhelpsAction | A13 |
| OnyxiaAvoidEggsAction | A4/A1 |

### Triggers
DeepBreathTrigger = T10+T3; NearTailTrigger = T4+T3; FireballSplashTrigger = T10;
WhelpsSpawnTrigger = T3+T4; AvoidEggsTrigger = T5.

---

## Molten Core

### Actions
| Class | Shape |
|---|---|
| McMoveFromGroupAction | A7 (living bomb) |
| McMoveFromBaronGeddonAction | A4 |
| McShazzrahMoveAwayAction | A4 |
| McGolemaggMarkBossAction | A19 |
| McGolemaggMainTankAttackGolemaggAction | A14 |
| McGolemaggAssistTankAttackCoreRagerAction | A14/A15 |

### Triggers
LivingBomb / Inferno = T1; ShazzrahRanged / GolemaggMark / IsMainTank /
IsAssistTank = T4.

---

## Karazhan

### Actions (selected; full set in header)
| Class | Shape |
|---|---|
| ManaWarpStunCreatureBeforeWarpBreachAction | A12 |
| Attumen SplitBosses / MarkTarget / StackBehind | A12/A19/A3 |
| AttumenManageDpsTimerAction | A25 |
| MaidenMoveBossToHealer / PositionRanged | A3/A4 |
| BigBadWolfRunAwayFromBossAction | A4 |
| WizardOfOzScorchStrawmanAction | A12 (state-gated) |
| Curator MarkAstralFlare / PositionBoss / SpreadRanged | A19/A1/A2 |
| ShadeOfAran RunFromArcaneExplosion / MaintainDistance | A4/A1 |
| ShadeOfAranStopMovingDuringFlameWreathAction | A10 |
| ShadeOfAranMarkConjuredElementalAction | A19 |
| Netherspite Block Red/Blue/GreenBeamAction | NICHE (beam intercept) |
| NetherspiteAvoidBeamAndVoidZoneAction | A4 |
| NetherspiteManageTimersAndTrackersAction | A25 |
| PrinceMalchezaar Enfeebled / AvoidInfernal / MainTankMovement | A9/A4/A14 |
| Nightbane GroundPosition / RotateRanged | A1/A2 |
| NightbaneCastFearWardOnMainTankAction | A21 |
| NightbaneControlPetAggressionAction | A22 |
| NightbaneFlightPhaseMovementAction | A8 |
| Nightbane/Netherspite/Attumen ManageTimers* | A25 |

### Triggers
T1 (FlameWreath, auras), T3 (phase: mounted/chase/banished/flying), T5 (Charred
Earth), T6 (ManaWarp explode, beam rotation), T7/T8 (engaged/threat), T11 (adds
spawned). Netherspite beam-active triggers = T10-ish (mechanic active).

---

## Magtheridon

### Actions
| Class | Shape |
|---|---|
| MainTankAttackFirstThreeChannelersAction | A12 |
| First/SecondAssistTankAttackNW/NEChannelerAction | A14 |
| MisdirectHellfireChannelers | A17/A15 |
| AssignDPSPriorityAction | A12 |
| WarlockCCBurningAbyssalAction | A18 |
| MainTankPositionBossAction | A1 |
| SpreadRangedAction | A7 |
| UseManticronCubeAction | A23 |
| ManageTimersAndAssignmentsAction | A25 |

### Triggers
Channeler-engaged = T4/T7; KillOrder = T6; BurningAbyssalSpawned = T11;
BlastNova incoming = T6; cube timer = T6.

---

## Gruul's Lair

### Actions
| Class | Shape |
|---|---|
| Maulgar MainTank/Mage/Moonkin/AssistTank attack <add> | A14 |
| MaulgarAssignDPSPriorityAction | A12 |
| MaulgarHealerFindSafePositionAction | A4 (healer escape) |
| MaulgarRunAwayFromWhirlwindAction | A4 |
| MaulgarBanishFelstalkerAction | A18 |
| MaulgarMisdirectOlmAndBlindeyeAction | A15/A17 |
| GruulTanksPositionBossAction | A1 |
| GruulSpreadRangedAction | A2 |
| GruulShatterSpreadAction | A7 |

### Triggers
Role checks (T4 ×5 for council tanks), KillOrder (T6), HealerInDanger (T4/T8),
Whirlwind channel (T3), Felstalker spawned (T11), IncomingShatter (T6).

---

## Blackwing Lair *(stub)*

| BwlOnyxiaScaleCloakAuraCheckAction | NICHE (equip swap) |
| BwlTurnOffSuppressionDeviceAction | A23 |
| BwlUseHourglassSandAction | A23 |
| BwlSuppressionDeviceTrigger | T7 |
| BwlAfflictionBronzeTrigger | T1 |

---

## Serpentshrine Cavern

### Actions (selected)
| Class | Shape |
|---|---|
| UnderbogColossusEscapeToxicPoolAction | A4 |
| GreyheartTidecallerMarkWaterElementalTotemAction | A19 |
| Hydross Frost/NatureTankPosition | A1/A12 |
| HydrossPrioritizeElementalAddsAction | A12 |
| HydrossFrostPhaseSpreadOutAction | A7 |
| LurkerRunAroundBehindBossAction | A3 |
| LurkerSpreadRangedInArcAction | A7 |
| LurkerTanksPickUpAddsAction | A15 |
| Leotheras TargetSpellbinders / DestroyInnerDemon | A13 |
| LeotherasRunAwayFromWhirlwindAction | A4 |
| Karathress MainTank/AssistTank position <add> | A14 |
| KarathressAssignDpsPriorityAction | A12 |
| Morogrim MoveBossToTankPos / Phase2RepositionRanged | A1 |
| LadyVashj MainTankPosition / SpreadRangedInArc | A1/A7 |
| LadyVashjSetGroundingTotemInMainTankGroupAction | A1 (place totem) |
| LadyVashjStaticChargeMoveAwayFromGroupAction | A7 |
| LadyVashjTankAttackAndMoveAwayStriderAction | A3 (kite) |
| LadyVashjTeleportToTaintedElementalAction | NICHE |
| LadyVashjLootTaintedCoreAction | A23 |
| LadyVashjPassTheTaintedCoreAction | NICHE (item relay) |
| LadyVashjDestroyTaintedCoreAction | A23 |
| LadyVashjAvoidToxicSporesAction | A4 |
| many *ManageTimer* / *Misdirect* | A25 / A17 |

### Triggers
T4 (roles/add assignment, heavy on Karathress/Vashj), T3 (phase), T5 (hazards/adds
near), T6 (spout/storm windows), T1/T2 (static charge, chaos blast stacks), T8
(aggro), T7 (pull). TaintedCore looted/unusable = item-state (niche).

---

## Tempest Keep

### Actions (selected)
| Class | Shape |
|---|---|
| CrimsonHandCenturionCastPolymorphAction | A18 |
| Alar Boss/MeleeDps MoveBetweenPlatforms / RangedMoveUnder / ReturnToCenter | A1 |
| AlarAssistTanksPickUpEmbersAction | A14/A15 |
| AlarRangedDpsPrioritizeEmbersAction | A12 |
| AlarJumpFromPlatformAction | A5 (jump) |
| AlarMoveAwayFromRebirthAction | A4 |
| AlarAvoidFlamePatchesAndDiveBombsAction | A4 |
| AlarSwapTanksOnBossAction | A16 |
| VoidReaverTanksPositionBossAction | A1 |
| VoidReaverUseAggroDumpAbilityAction | A20/A21 |
| VoidReaverSpreadRangedAction | A7 |
| VoidReaverAvoidArcaneOrbAction | A4/A5 |
| Solarian RangedLeaveSpaceForMelee / MoveAwayFromGroup | A7 |
| SolarianStackForAoeAction | A8 |
| SolarianTargetSolariumPriestsAction | A13/A14 |
| SolarianCastFearWardOnMainTankAction | A21 |
| Kael KiteThaladred | A3 (kite) |
| Kael MisdirectAdvisorsToTanks | A17 |
| Kael MainTank/WarlockTank/AssistTank Position <advisor> | A14 |
| Kael CastFearWard <tank> | A21 |
| Kael SpreadAndMoveAwayFromCapernian / SpreadOutInMidair | A7 |
| Kael AssignAdvisor/LegendaryWeapon DpsPriority | A12 |
| Kael MoveDevastationAway / AvoidFlameStrike | A3/A4 |
| KaelLootLegendaryWeaponsAction | NICHE |
| KaelUseLegendaryWeaponsAction | NICHE |
| KaelReequipGearAction | NICHE |
| Kael HandlePhoenixesAndEggs | A13/A14 |
| Kael BreakMindControl / BreakThroughShockBarrier | A20 (niche) |
| many *ManageTimer* / *Misdirect* | A25 / A17 |

### Triggers
T1 (casts/auras), T3 (phase: flying/platforms/gravity), T4 (roles, warlock-tank,
advisor assignments), T5 (adds/embers/weapons present), T6 (incoming orbs/quills,
pyroblast), T8 (threat/fixate). KaelThaladredFixated = T8 (fixate). Weapon
equipped/looted/lost = item-state (niche).

---

## Zul'Aman

### Actions
| Class | Shape |
|---|---|
| AmanishiMedicineManMarkWardAction | A19 |
| Akil'zon/Nalorakk/Janalai/Halazzi/Zuljin TanksPositionBoss | A1/A12 |
| *SpreadRanged(InCircle)Action (most bosses) | A2/A7 |
| AkilzonMoveToEyeOfTheStormAction | A1 (safe zone) |
| JanalaiAvoidFireBombsAction | A4 |
| JanalaiMarkAmanishiHatchersAction | A19 |
| HalazziFirstAssistTankAttackSpiritLynxAction | A13/A14 |
| *AssignDpsPriorityAction | A12 |
| HexLordMalacrass/Zuljin RunAwayFromWhirlwind / AvoidCyclones | A4 |
| HexLordMalacrassCastersStopAttackingAction | A20 |
| HexLordMalacrassMoveAwayFromFreezingTrapAction | A4 |
| *MisdirectBossToMainTankAction (all bosses) | A17 |
| *ManageTimerAction | A25 |

### Triggers
T7 (pull, all bosses), T4 (roles), T3 (form switches: troll/eagle/dragonhawk,
bear/etc.), T1 (form-specific casts/spell reflect), T5 (adds/bombs/traps/cyclones),
T6 (electrical storm windows, whirlwind channels), T8 (aggro).

---

## AQ20 *(stub)*

| Aq20UseCrystalAction | A23 |  | Aq20MoveToCrystalTrigger | T5/A1 |

Likely relies on generic (non-raid) strategies for everything else.
