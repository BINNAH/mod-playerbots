# Raid Bot AI — Pattern Catalog

A vocabulary of the behavioral "shapes" that every raid encounter strategy in this
module is built from. The point: although there are several hundred boss-specific
Action and Trigger classes across 17 raids, they collapse to **~25 action shapes**
and **~12 trigger shapes**. Most boss classes are thin parameterizations of a small
set of generic base classes.

Read this first before authoring a new boss strategy. For the full per-class
"which existing class already does shape X" lookup, see `RAID_AI_INVENTORY.md`.

---

## Guiding principle

> **Data wires and parameterizes. C++ implements behavior.**

This is where the codebase already sits today, which is why it's a safe place to
formalize. A strategy is a list of `(trigger -> [action + priority])` rules plus
some tunable constants (coordinates, radii, spell IDs, stack thresholds). That
wiring + those constants are *data*. The behavior an action performs (how it
paths, how it picks a target) is *code*.

A future data-driven (JSON/DB) layer should externalize the wiring and the
constants — NOT try to express behavior. The handful of fights with irreducibly
complex behavior (see "Bespoke behaviors" below) stay in C++; only their knobs
get exposed.

---

## How a strategy is wired (the engine)

Verified against `Naxxramas/Strategy/RaidNaxxStrategy.cpp`. Every raid follows the
same shape:

```cpp
void Raid<X>Strategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("four horsemen void zone",
        { NextAction("four horsemen avoid void zone", ACTION_RAID + 4) }
    ));
    // ... one TriggerNode per rule; a node may list several NextActions
}

void Raid<X>Strategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FourHorsemenGenericMultiplier(botAI));
}
```

- **TriggerNode** = one rule: a trigger name -> a list of `NextAction(actionName, relevance)`.
- **Trigger** class implements `bool IsActive()` (the condition).
- **Action** class implements `bool Execute(Event)` — returns `true` if it fired
  (consuming the tick) or `false` to fall through to the next candidate.
- **Relevance** = `ACTION_RAID + offset`. Higher offset wins the tick. Observed
  Naxx convention:
  - `+4` survival movement (dodge void zone) — overrides everything
  - `+3` urgent repositioning (healer bleed-off marks)
  - `+2` opening defensive cooldowns
  - `+1` normal positioning / target selection
  - `+0` low-priority self-buffs (frost resist aura)
- **Multiplier** classes scale an action's relevance dynamically (e.g. suppress
  normal positioning while a survival mechanic is active).
- **Context factories** (`Raid<X>ActionContext.h` / `Raid<X>TriggerContext.h`) map
  the string names to class constructors. This is the registry that makes the
  string-keyed wiring work.

Crucially, many `NextAction` names already reference **generic, shared actions** —
`"avoid aoe"`, `"rear flank"`, `"shield wall"`, `"taunt spell"`, `"barkskin"`,
`"last stand"`. The string-keyed wiring is *already* data-shaped; it's just spelled
in C++ instead of JSON.

---

## The generic base-class library

Boss actions inherit from these. They're in `src/Ai/Base/Actions/`. The base
class + constructor args usually ARE the behavior; the boss subclass just supplies
the numbers.

| Base class | File | What it does | Key params |
|---|---|---|---|
| `MovementAction` | MovementActions.h | Base for all positioning; helpers like `MoveTo(map,x,y,z)`, `MoveFromGroup(dist)`, `MoveAway(unit,dist)` | — |
| `MoveInsideAction` | MovementActions.h:249 | Get within radius of a fixed point | x, y, radius |
| `RotateAroundTheCenterPointAction` | MovementActions.h:264 | Orbit a center point in N segments | x, y, radius, segments, [dir] |
| `MoveAwayFromCreatureAction` | MovementActions.h:303 | Flee a creature within radius | entry/guid, radius |
| `MoveAwayFromPlayerWithDebuffAction` | MovementActions.h:320 | Spread from allies carrying a debuff | aura, distance |
| `FleeAction` | MovementActions.h:85 | Generic flee | — |
| `AttackAction` | AttackAction.h | Base for target selection / attack | — |
| `BuffOnMainTankAction` | (GenericSpellActions) | Cast a named buff on the main tank | spell name |
| `HealPartyMemberAction` | (heal actions) | Heal lowest party member (retargetable) | spell, threshold |
| `Action` | Action.h | Generic non-movement/attack action (marking, pet control, state mgmt) | — |
| `Trigger` / `HasAuraTrigger` / `HasNoAuraTrigger` | Triggers/ | Condition base classes | aura name, count |

Example of how thin the boss subclass is:

```cpp
// Maexxna's Hand of Sacrifice = BuffOnMainTankAction + one string.
class MaexxnaHandOfSacrificeOnMainTankAction : public BuffOnMainTankAction {
    MaexxnaHandOfSacrificeOnMainTankAction(PlayerbotAI* ai)
        : BuffOnMainTankAction(ai, "hand of sacrifice") {}
};

// Anub'Rekhan / Grobbulus / Gluth positioning = RotateAroundTheCenterPoint + coords.
AnubrekhanPositionAction(ai) : RotateAroundTheCenterPointAction(ai, "...", 3272.49f, -3476.27f, 45.0f, 16) {}
```

---

## ACTION SHAPES

Grouped by family. Each shape lists what it does, the typical parameters a data
layer would carry, the usual generic base, and a couple of example classes (full
list in `RAID_AI_INVENTORY.md`).

### Movement / positioning

| ID | Shape | What it does | Typical params | Base | Examples |
|---|---|---|---|---|---|
| **A1** | Move to point / zone | Get to fixed coords or within a radius | x, y, [z], radius | MoveInsideAction | ThorimArenaPositioning, AlarMoveBetweenPlatforms, MorogrimMoveToTankPos |
| **A2** | Orbit a center point | Circle a point in N slots | x, y, radius, segments | RotateAroundTheCenterPointAction | Anub'Rekhan, Grobbulus, Gluth, NightbaneRotateRanged |
| **A3** | Position relative to boss | Behind / flank / face-away / at-range from boss | distance, angle | MovementAction | GrobbulusGoBehind, OnyxiaMoveToSide, YoggLunaticGaze |
| **A4** | Flee a hazard source | Move out of a radius around an NPC / ground effect / aura source | source entry, radius | MoveAwayFromCreatureAction | FourHorsemenAvoidVoidZone, AvoidFlameTsunami, McMoveFromBaronGeddon |
| **A5** | Dodge a line / charge | Sidestep perpendicular to a charge, beam, cone, or projectile path (incl. jump-dodge) | — | MovementAction | IcehowlDodgeCharge, HodirBitingColdJump, KologarnEyebeam, VezaxShadowCrash |
| **A6** | Timed safe-zone dance | Predict the boss-script schedule and move to the deterministically-safe spot | phase timings, zone geometry | MovementAction | HeiganDance, OnyxiaMoveToSafeZone (deep breath), SindragosaBlisteringCold — **fixed-pattern case now data-driven: JSON `timed_safe_zone` shape** |
| **A7** | Spread / disperse | Increase spacing from allies — to a fixed distance, an assigned ring slot, or only when self carries a spread-debuff | distance, [slot index], [debuff] | MoveFromGroup / MoveAwayFromPlayerWithDebuff | XT002MoveAwayFromGroup, GormokSpread, FestergutSpore, OnyxiaSpreadOut |
| **A8** | Stack / converge | Move together onto the group or a point (inverse of spread) | target, radius | MovementAction | BqlPactOfDarkfallen, SolarianStackForAoe, NightbaneFlightStack |
| **A9** | Position by self buff/debuff | Move based on what aura the bot carries (polarity side, beacon, stacks) | aura, geometry | MovementAction | ThaddiusMovePolarity, SindragosaMysticBuffet, SartharionTank(drake) |
| **A10** | Hold position | Suppress movement entirely while a condition holds | aura/condition | MovementAction | ShadeOfAranStopDuringFlameWreath |
| **A11** | Return to valid floor | Recover from a pit / fall hazard (often Z-based) | safe coords, z-threshold | MovementAction | KologarnFallFromFloor, AuriayaFallFromFloor, EmalonFallFromFloor |

### Targeting / combat

| ID | Shape | What it does | Typical params | Base | Examples |
|---|---|---|---|---|---|
| **A12** | Choose / prioritize target | Pick attack target; fixed kill-order | priority list | AttackAction | FourHorsemenAttackInOrder, AssignDpsPriority (many) |
| **A13** | Attack add by entry | Switch to a specific spawned NPC | entry | AttackAction | MaexxnaAttackWebWrap, KillPowerSpark, GormokAttackSnobold |
| **A14** | Assigned add-tanking | Tank a specific add by role/index (council fights) | role/index -> entry | AttackAction | MaulgarCouncil, MagtheridonChannelers, KarathressAdds, KaelAdvisors |
| **A15** | Off-tank add herding | Taunt loose adds and drag them to the main tank | — | AttackAction | NothTankAdds, XT002OffTankPickupPummeler, LurkerTanksPickUpAdds |
| **A16** | Tank swap on threshold | Taunt the boss off the other tank at N debuff stacks | aura, stack count | AttackAction | GormokImpaleSwap, KologarnCrunchArmor, GluthMortalWound |
| **A17** | Threat redirect / misdirect | Use a threat-transfer assist (MD / Tricks) onto a tank | target tank | AttackAction | (many) MisdirectBossToMainTank |
| **A18** | Crowd-control an add | Banish / poly / sheep an assigned add | spell, entry | AttackAction | MaulgarBanishFelstalker, MagtheridonCCAbyssal, CenturionPolymorph |
| **A19** | Set raid marker | Place a raid icon for coordination / kill-order | icon, target | Action | EmalonMarkBoss, KologarnMarkDpsTarget, McGolemaggMarkBoss |
| **A20** | Suppress own action | Stop attacking / casting during a phase or condition | condition | Action | StopDpsUponPhaseChange, CastersStopAttacking |

### Support / utility

| ID | Shape | What it does | Typical params | Base | Examples |
|---|---|---|---|---|---|
| **A21** | Cast cooldown on self / tank | Pop a personal or tank-targeted defensive/buff | spell name, target | BuffOnMainTankAction / spell actions | shield wall, hand of sacrifice, fear ward, frost resist aura |
| **A22** | Set pet react state | Flip pet passive/aggressive (or collision) for a phase | state | Action | KelthuzadControlPet, NightbanePetCollision |
| **A23** | Use a world object | Move to and interact with a clickable: portal, cube, crystal, harpoon, device, loot | object entry/guid | MovementAction / Action | RazuviousCrystal, MagtheridonCube, SartharionPortal, RazorscaleHarpoon |
| **A24** | Vehicle operate | Board a vehicle, move/position it, use its abilities | seat, vehicle abilities | MovementAction / Action | FlameLeviathanVehicle, EoEDrake(fly+attack), IccGunshipCannon |
| **A25** | Encounter state manager | Per-tick bookkeeping action that updates the boss helper's phase/timer state (incl. movement/damage "cheats") | — | Action | *ManageTimersAndTrackers (most rich fights), Mimiron/VezaxCheat |

---

## TRIGGER SHAPES

| ID | Shape | What it detects | Typical params | Base | Examples |
|---|---|---|---|---|---|
| **T1** | Has / lacks aura | A specific buff/debuff on self or a unit | aura, [target] | HasAuraTrigger / HasNoAuraTrigger | MutatingInjection, FlameWreathActive, McLivingBomb |
| **T2** | Aura stacks ≥ N | A stacking debuff crossed a threshold | aura, count | Trigger | KologarnCrunchArmor, GormokImpale, ChaosBlastStacks |
| **T3** | Phase detection | Boss attackable/non-attackable, flying/ground, HP %, form, submerged | phase signal | Trigger | SapphironGround/Flight, MalygosPhase, NalorakkForm |
| **T4** | Role / assignment | Is this bot tank / healer / ranged / assist-index N / a given class-spec | role/index/class | Trigger | RazuviousTank, MaulgarMageTank, FourHorsemenAttractors |
| **T5** | In / near a hazard | Within radius of a hazard NPC or ground effect | entry/effect, radius | Trigger | FourHorsemenVoidZone, FlameTsunami, NightbaneCharredEarth |
| **T6** | Timed / predicted window | Opening N seconds; incoming-mechanic pre-cast window; rotation schedule | timing | Trigger | FourHorsemenOpeningDefensive, IcehowlCharge, AkilzonStormIncoming |
| **T7** | Encounter active | Boss engaged / in combat / pull detected | — | Trigger | AnubrekhanTrigger, AlarPullingBoss |
| **T8** | Threat / aggro | Lost aggro, who-has-aggro, fixation (untauntable threat on self) | — | Trigger | ThaddiusLoseAggro, KaelThaladredFixated |
| **T9** | Formation / proximity | Too close to an ally, or outside an assigned spread slot | distance/slot | Trigger | GormokSpread, (spread checks) |
| **T10** | Boss casting spell | A specific spell cast is in progress | spell ID | Trigger | OnyxiaDeepBreath, VoidReaverPounding, JanalaiFlameBreath |
| **T11** | Add / NPC present | A named add/NPC spawned or is alive | entry | Trigger | PowerSpark, CuratorAstralFlare, HalazziSpiritLynx |
| **T12** | Position-based | Bot at a coordinate region or Z below floor | coords / z | Trigger | EmalonFallFromFloor (Z<80) |

(Niche trigger not promoted: **item/inventory state** — Kael legendary weapons
equipped/looted/lost, Vashj tainted core usable. One-boss mechanics.)

---

## Bespoke behaviors (leave in C++)

A small set of fights have behavior that is genuinely irreducible — expressing it
as data would mean building a scripting language with timers, loops, and state.
**Don't.** Keep these in C++; a data layer should only expose their tunable knobs.

- ~~**Heigan dance clock**~~ — *no longer bespoke:* the deterministic
  fixed-pattern eruption dance is now the generic JSON `timed_safe_zone` shape
  (zones + walk pattern + per-phase cadence as data). A *reactive/random* safe
  zone (Sapphiron ice blocks) still belongs here.
- **Yogg-Saron** — multi-room illusion navigation, brain link pair-spacing, portal in/out, sanity meter.
- **Lich King** — heroic-aware add management (Valkyr grabs, Defile, Necrotic Plague kite), multi-role winter positioning.
- **Netherspite (Kara)** — colored beam interception + rotation manager.
- **Kael'thas (TK)** — loot/equip/use legendary weapons; advisor phase choreography; fixate kiting.
- **Lady Vashj (SSC)** — tainted-core relay between players.
- **Mimiron** — multi-phase vehicle/aerial command logic.

Everything else is composition of A1–A25 + T1–T12.

---

## Coverage / completeness notes

- **Rich, fully-scripted:** Naxxramas, Ulduar, Icecrown, Karazhan, TempestKeep,
  SerpentshrineCavern, Magtheridon, Gruul's Lair, ZulAman.
- **Thin (a few mechanics):** ObsidianSanctum, VaultOfArchavon, Onyxia, MoltenCore,
  TrialOfTheCrusader, EyeOfEternity.
- **Stub / mostly generic strategy:** Aq20 (crystal only), BlackwingLair
  (device/cloak/hourglass only — 3 actions). These lean on the generic
  (non-raid-specific) strategies for trash and basic combat.

The WotLK progression target (Naxx -> Ulduar -> ToC -> ICC) is the most heavily
scripted, which is good — those are the fights you'll tune most.

---

## Implications for a data-driven layer (future)

When/if we build the JSON/DB layer, the schema falls out of this catalog:

1. **A strategy** = a list of rules. Each rule = `{ trigger, [{action, relevance}] }`.
   This is a 1:1 serialization of `InitTriggers`. No new primitives needed.
2. **An action reference** = `{ shape_or_name, params }`. For shapes that are pure
   parameterization (A1, A2, A4, A7, A21, A23 …) the params (coords, radius, spell,
   entry) are all that vary — these become fully data-driven and reloadable.
3. **Constants** (the boss-helper magic numbers) move to a reloadable table so
   tuning a fight needs no rebuild.
4. **Bespoke fights** keep their C++ action but are still *wired* and *tuned* from
   data.

Net: the "instant retry, no server reload" goal is reachable for the bulk of
tuning (wiring + constants + parameterized shapes), without rewriting the
hand-tuned behavioral code.
