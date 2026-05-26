# Kel'Thuzad — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_kelthuzad.cpp`, the existing C++ AI at
`src/Ai/Raid/Naxxramas/Action/RaidNaxxActions_Kelthuzad.cpp`, and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; some debuffs have per-difficulty spell ids —
see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Kel'Thuzad` |
| Boss entry | `15990` (naxxramas.h `NPC_KELTHUZAD`; DB-confirmed) |
| Map | Naxxramas, `533` |
| Room center / bot anchor | `3716.19, -5106.58` (`KelthuzadBossHelper::center`) |
| Tank spot | `3709.19, -5104.86` (`KelthuzadBossHelper::tank_pos`) |
| Assist-tank spot | `3746.05, -5112.74` (`KelthuzadBossHelper::assist_tank_pos`) |
| Phase detect | P1 = boss has `UNIT_FLAG_NON_ATTACKABLE` set; P2 = flag clear |

## Adds

### Phase 1 — add waves (228 s until boss engages)

| Name | Entry | Role | Spawn cadence | Notes |
|------|-------|------|---------------|-------|
| `Soldier of the Frozen Wastes` | `16427` | DPS fodder | ~every 3.1 s | No threat modifier; stampedes to a player target. High-priority AoE kill. |
| `Unstoppable Abomination` | `16428` | Must-tank melee | ~every 18.5 s | Enrages at 35 % HP (`SPELL_FRENZY 28468`), Mortal Wound (`28467`) every 15 s; must be tanked, not AoE'd down. |
| `Soul Weaver` | `16429` | Caster add | every 30 s | Casts from range; priority kill for ranged DPS. |

Adds spawn at 7 pre-set portal/gate positions (`SpawnPool[0–6]`). At pull, `SpawnHelpers()` also seeds 6 alcove packs (8 Soldiers + 3 Abominations + 1 Weaver each) at `SummonGroups[6–11]`. Surviving adds despawn on `ACTION_SECOND_PHASE` when KT enters P2.

### Phase 3 — Guardians of Icecrown (triggered at 45 % boss HP)

| Name | Entry | Count | Mechanics |
|------|-------|-------|-----------|
| `Guardian of Icecrown` | `16441` | **2 (10-man) / 4 (25-man)** | `SPELL_BLOOD_TAP 28470` every 15 s; must be tanked by assist-tank; flee/despawn on KT death. |

Guardians spawn staggered from portals 1–4 every 5 s after the Lich King dialogue (`EVENT_P3_LICH_KING_SAY`).

## Spells

### Kel'Thuzad (P2+)

| Spell | ID | Phase | Notes |
|-------|----|-------|-------|
| Frost Bolt (single) | `28478` | P2 | Targets current victim; interruptible heavy frost nuke. Fires every 2–10 s. |
| Frost Bolt Volley | `28479` | P2 | AoE frost; all targets in range. Every 15–30 s. |
| Shadow Fissure | `27810` | P2 | Void-zone placed on a random player every 25 s. Move out; triggers `SPELL_VOID_BLAST 27812`. |
| **Frost Blast** | **`27808`** | P2 | Random target; **see critical note below.** Every 45 s. **25-man activates `RAID_MODE(false, true)` targeting check** — verify whether the same id fires on both sizes. |
| Detonate Mana | `27819` | P2 | Targets a random mana-user every 30 s; drains 10 % max mana and deals `27820` damage equal to 10× drained. Affects any mana-class (healer, caster). |
| Chains of Kel'Thuzad | `28410` | P2 | **25-man only** — mind-controls up to 3 random players for 20 s (excluding already-chained); fires every 90 s. Not cast in 10-man. |
| Berserk | `28498` | P2 | After 15 min total from pull. |
| KT Channel | `29423` | P1 | Channeled cast while KT is `NON_ATTACKABLE`; its presence/absence is the reliable P1↔P2 detector. |

### Minion spells (for completeness)

| Spell | ID | Caster | Notes |
|-------|----|--------|-------|
| Frenzy | `28468` | Unstoppable Abomination | Below 35 % HP. |
| Mortal Wound | `28467` | Unstoppable Abomination | Every 15 s. |
| Blood Tap | `28470` | Guardian of Icecrown | Every 15 s. |

> **Difficulty-id note:** Most KT P2 spells (`28478`, `28479`, `27810`, `27819`) appear
> to use the same id on 10 and 25 — the script scales by value or simply fires the same
> spell regardless of raid size. **Frost Blast (`27808`)** has a `RAID_MODE(false, true)`
> filter on *target selection* (the spell only picks a target at all in 25-man according
> to the target-select call's filter arg), but the spell id itself is the same.
> **Chains of Kel'Thuzad (`28410`)** is explicitly scheduled only in `Is25ManRaid()`.
> Detect all KT abilities by **aura NAME** rather than id to stay safe across
> difficulties. (Memory: raid-debuff-difficulty-ids.)

## Phase structure

- **Phase 1 (0 – 228 s from pull):** KT is `NON_ATTACKABLE` and channelling (`29423`).
  He is on everyone's threat list (aggro range 50 y via `MoveInLineOfSight`) but cannot
  be attacked. Three add types funnel out of 6 alcove packs plus the central gate pool;
  the raid AoEs Soldiers and Weavers and tanks Abominations. **The whole raid must kill
  adds for 3 m 48 s before the boss engages.** Surviving adds despawn on P2 entry.
- **Phase 2 (boss engages):** KT becomes attackable and chases his victim. Full spell
  rotation begins: Frost Bolt spam → interrupt; Frost Bolt Volley; Shadow Fissure void
  zones (move out); Frost Blast flood-fill (see critical note); Detonate Mana on casters;
  Chains of Kel'Thuzad MC (25-man). Berserk at 15 min.
- **Phase 3 (45 % HP trigger):** Portals open; 2/4 Guardians of Icecrown spawn and must
  be picked up by the assist-tank. All P2 spells continue.

## Critical mechanic — Frost Blast flood-fill (spell 27808)

Frost Blast is **not** a standard AoE. The AC core re-casts spell `27808` from each
newly-frozen victim every 1 s (`SpellAuraEffects.cpp`), propagating to every unfrozen
player within 10 y of an already-frozen player. This is a **self-propagating flood-fill**:
one frozen player bridges the chain onto nearby players, who then bridge onto their
neighbours, and so on until the entire connected blob is frozen.

**Defensive geometry:** the raid must maintain isolated clusters where every cluster is
more than 10 y from every other cluster (including the main tank group, the ranged
ring, and other melee clusters). A single "bridge player" sitting between two clusters
propagates the freeze across both. The C++ AI implements a **4-point melee layout**:

- Main tank holds KT at `tank_pos`.
- Melee DPS are distributed across 3 clusters (left/behind/right of KT) at angles
  `+90°, 180°, -90°` from the tank-facing direction, at `boss_combat_reach + bot_combat_reach`
  radius (~11.5 y from KT). Clusters at 90° separation are ~16 y apart — safely beyond
  the 10 y Frost Blast hop.
- Ranged ring pushed to **26 y** (inner, 8 bots at 45° slots) and **34 y** (outer,
  offset 22.5° so no inner/outer pair lines up radially). This clears even a frozen
  melee cluster (max ~11.5 y from KT) by >13 y before the ranged ring begins.
- Shadow Fissure dodge: when any Shadow Fissure trigger unit appears within 10 y, bots
  flee 10 y in the direction away from room center (ranged) or toward center (melee).

(Project memory: kelthuzad-frost-blast-floodfill.)

## Existing C++ AI — what it does

No JSON strategy yet. The hand-tuned C++ AI (`RaidNaxxActions_Kelthuzad.cpp`,
`RaidNaxxMultipliers.cpp`, `RaidNaxxStrategy.cpp`) wires up three actions under trigger
`"kel'thuzad"` (fires whenever `KelthuzadBossHelper::UpdateBossAI()` returns true, i.e.
the bot's `find target "kel'thuzad"` is non-null):

- **`kel'thuzad control pet`** (`ACTION_RAID + 3`): Flips pets `REACT_PASSIVE` in P1
  (bot AI directs them to attack the chosen add target manually) and `REACT_AGGRESSIVE`
  in P2. Returns `false` so the engine chains to the next action on the same tick.
- **`kel'thuzad position`** (`ACTION_RAID + 2`): P1 — if no target, MoveInside to room
  center. P2 — main tank moves to `tank_pos` only while holding aggro; ranged spread to
  inner/outer rings; assist-tanks move to `assist_tank_pos` when holding a Guardian;
  melee DPS distribute to the 4-point cluster layout; Shadow Fissure flee override
  applies to all roles.
- **`kel'thuzad choose target`** (`ACTION_RAID + 1`): Assigns targets by role. Ranged
  DPS index ≤ 1 → Soldier > Weaver > Abomination > KT; ranged index > 1 → Weaver >
  Soldier > Abomination > KT. Assist-tank → Abomination > Guardian > KT. Main tank →
  Abomination > KT. All targets must be within 30 y of room center **and** within spell
  range of the bot (filters out alcove stragglers the bot cannot reach).

**`KelthuzadGenericMultiplier`** (always active while `UpdateBossAI()` is true):
- Zeros out `DpsAssistAction`, `TankAssistAction`, `CastDebuffSpellOnAttackerAction`,
  `FleeAction`, and `CombatFormationMoveAction` — prevents the generic assist/flee
  system from overriding the custom targeting and positioning.
- P1 additionally zeros totem placement, Shadowfiend, Raise Dead, Feign Death,
  Invisibility, Vanish, and `PetAttackAction` (pets are directed manually).
- P2 additionally zeros `CastBlizzardAction` and `CastFrostNovaAction` (would freeze
  adds in place and break melee positioning).

The MC (`Chains of Kel'Thuzad`) has no bot-side handler; mind-controlled bots are
briefly taken over by the server. No attempt is made to break the MC early.

## Implications for bot AI / strategy authoring

- **P1 "find target" caveat.** KT is on the threat list throughout P1 (he aggro'd the
  raid at pull), so `AI_VALUE2("find target", "kel'thuzad")` returns non-null in both
  phases — the trigger stays active. This is different from Noth's balcony phase (where
  Noth clears threat entirely). However, the boss is `NON_ATTACKABLE`, so the choose-
  target action must steer bots onto adds, not KT. (Memory: findtarget-is-threat-based.)
- **Frost Blast spacing is data-expressible in principle** (a spread shape with >10 y
  minimum inter-cluster gap), but the no-bridge-player constraint — that no lone player
  can sit between two clusters — is a layout invariant that is hard to enforce with
  static `spread` shapes. The C++ 4-point cluster system enforces it by construction
  (fixed angles off the tank, computed at runtime). Any JSON replacement would need to
  reproduce this or risk chain-propagation wipes. Treat as **hard-limit / C++ preferred**
  until a JSON spread parameterisation that handles the bridge constraint is designed.
- **Chains of Kel'Thuzad (MC) is 25-man only.** No bot-side counter exists. A mind-
  controlled bot attacks its former allies until the 20 s duration expires. Survivable
  but worth noting for healer/tank throughput calculations.
- **Guardian add-tanking is partly C++.** `KelthuzadChooseTargetAction` steers assist-
  tanks to Guardian targets and the position action moves them to `assist_tank_pos`.
  `KelthuzadChooseTargetAction` uses a "prefer Guardian currently hitting a non-assist-
  tank player" tiebreak to prioritise dangerous Guardians. A JSON `tank_adds` layer
  could supplement this but would need to target by name `"guardian of icecrown"` (entry
  `16441`).
- **Aura detection by NAME.** Chains of Kel'Thuzad fires only in 25-man; if gating on
  it use `HasAura("chains of kel'thuzad", target)` not the id — same guidance as all
  Naxx raid debuffs. (Memory: raid-debuff-difficulty-ids.)
- **Human never required.** Assign all mandatory positional slots (main tank, assist-
  tank, add clusters) from bot-only pools; skip `GET_PLAYERBOT_AI == null` members.
  (Memory: human-never-required.)
- **No JSON strategy file yet.** The existing C++ AI handles positioning, target
  priority, pet management, and the Frost Blast cluster geometry. This fight is a strong
  candidate to remain mostly C++ (the Frost Blast bridge constraint is non-trivial to
  replicate in data). The add-wave AoE targeting and Shadow Fissure dodge are simpler
  candidates for a future JSON overlay if desired.
