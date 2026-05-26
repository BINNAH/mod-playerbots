# The Four Horsemen — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_four_horsemen.cpp`,
`src/server/scripts/Northrend/Naxxramas/naxxramas.h`, and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; debuff stacks are per-difficulty — see note
below).

## Identity

| Horseman | Entry | Combat role | Corner (final waypoint coords) |
|----------|-------|-------------|-------------------------------|
| `Thane Korth'azz` | `16064` | Melee (physical + Meteor) | SW ~`2542, -3015, 241` |
| `Lady Blaumeux` | `16065` | Ranged/caster (stationary after corner) | NW ~`2469, -2948, 241` |
| `Baron Rivendare` | `30549` | Melee (physical + Unholy Shadow) | SE ~`2584, -2972, 241` |
| `Sir Zeliek` | `16063` | Ranged/caster (stationary after corner) | NE ~`2518, -2897, 241` |

Map: Naxxramas, `533`. All four share a single BOSS_HORSEMAN encounter slot — the
chest spawns when all four are dead (`IsBossDone`).

Note on "Baron Rivendare" name collision: entries 10440, 28445, 28910, 29109 are
other creatures with the same name (non-Horsemen). The Horseman is specifically
**entry `30549`** (`NPC_BARON_RIVENDARE` in naxxramas.h).

## Adds

None. The Four Horsemen encounter has no summoned add waves. All four Horsemen
themselves are the only hostile units. The human is never a required slot — bots
cover all tank/attacker assignments.

## Spells

### Marks (shared mechanic — one per Horseman, applied to their current tank)

| Spell | ID | Caster | Interval |
|-------|----|--------|----------|
| Mark of Korth'azz | `28832` | Thane Korth'azz | every **12s** |
| Mark of Blaumeux | `28833` | Lady Blaumeux | every **15s** |
| Mark of Rivendare | `28834` | Baron Rivendare | every **12s** |
| Mark of Zeliek | `28835` | Sir Zeliek | every **15s** |
| Mark Damage (trigger) | `28836` | (applied per stack) | on each stack application |

First Mark cast fires at **24s** into combat for all four. Marks are stacking
debuffs — damage escalates sharply per stack: 0 → 500 → 1500 → 4000 → 12500 →
20000 hp per application. Stack 7+ deals 20000 + 1000 per additional stack. This
is the core swap driver: tanks must rotate off a Horseman before Marks become
lethal.

### Per-Horseman combat spells

| Spell | ID | Caster | Type | Notes |
|-------|----|--------|------|-------|
| Meteor | `28884` | Thane Korth'azz | Primary (melee only) | Every 15s; AoE fire |
| Unholy Shadow | `28882` | Baron Rivendare | Primary (melee only) | Every 15s |
| Shadow Bolt | `57374` | Lady Blaumeux | Primary (ranged) | Every 15s while target in 45y; falls back to Unyielding Pain if out of range |
| Void Zone | `28863` | Lady Blaumeux | Secondary (ranged) | Every 15s; persistent ground AoE, same role as Heigan floor |
| Unyielding Pain | `57381` | Lady Blaumeux | Punish | Cast when no valid target in 45y |
| Holy Wrath | `28883` | Sir Zeliek | Secondary (ranged) | Every 15s; chain-lightning style |
| Holy Bolt | `57376` | Sir Zeliek | Primary (ranged) | Every 15s while target in 45y |
| Condemnation | `57377` | Sir Zeliek | Punish | Cast when no valid target in 45y |
| Berserk | `26662` | All four | — | 10 min enrage |

Melee Horsemen (Korth'azz, Rivendare): use `DoMeleeAttackIfReady()` plus a
periodic primary spell. They actively chase their victim.

Ranged Horsemen (Blaumeux, Zeliek): go `MoveIdle()` at their corner (stationary
permanently after the opening walk). They continuously scan for the nearest player
within 300y and attack at spell range. If the victim leaves 45y they cast their
Punish spell instead of the primary.

### Opening movement

On engage all four go `REACT_PASSIVE` and walk a fixed 3-waypoint path to their
designated corner. They become aggressive and enter combat only when the final
waypoint is reached (`MOVE_PHASE_FINISHED`). During this walk they are not
attacking. Bot tanks must wait until movement finishes before engaging.

> **Difficulty-id note:** All four Marks use the same spell IDs on 10-man and
> 25-man (no per-difficulty rows). Scaling is in damage values only (hard-coded in
> the aura script stack table). Detect Marks by **aura NAME**, not by id, to be
> safe across difficulties. The Mark damage trigger (`28836`) does not need
> detection — it fires automatically from the aura script.

## Phase structure

No traditional phase transitions. The encounter is effectively a single continuous
phase with four simultaneous actors:

1. **Opening walk (~10–15s):** All four Horsemen walk to their corners. They are
   passive. The raid should hold DPS and let tanks position.
2. **Active combat:** All four Horsemen fight simultaneously in their corners. Marks
   begin at 24s and repeat on each Horseman's interval (12s or 15s).
3. **Corner rotation (ongoing, Mark-driven):** When a tank's Mark stack reaches a
   dangerous threshold (~3–4 stacks), that tank rotates clockwise to the next
   Horseman; an unloaded tank takes over. The "front pair" (Korth'azz + Rivendare,
   melee) and "back pair" (Blaumeux + Zeliek, ranged) are typically handled by two
   sub-groups: two tanks rotating the front, two tanks rotating the back. The back
   pair requires ranged or healers to stand within their 45y range and take Marks
   so the Punish spells don't wipe the raid.
4. **Enrage:** Berserk at 10 minutes.

There are no phase-change events, no special transitions, no adds, and no
script-gated triggers beyond the Marks and the enrage timer.

## Current strategy file

`server/raid_strategies/four_horsemen.json` — a **Level-1 C++ reproduction**. It
does not use generic data shapes. It references C++-implemented named actions:

- `"four horsemen void zone"` / `"four horsemen avoid void zone"` — Blaumeux Void
  Zone avoidance
- `"four horsemen healer high mark"` / `"four horsemen healer bleed off mark"` —
  healer repositioning when their own Mark stack is high
- `"four horsemen attractors"` / `"four horsemen attract alternatively"` — the
  tank-rotation attractor system (corner assignment / swap trigger)
- `"four horsemen except attractors"` / `"four horsemen attack in order"` — DPS
  attack sequencing
- `"four horsemen opening defensive"` — class-specific cooldowns (Shield Wall, Last
  Stand, Icebound Fortitude, Vampiric Blood, Survival Instincts, Barkskin, Divine
  Protection, Shamanistic Rage) for the opening phase

The human has not yet analyzed this encounter's C++ implementation in depth. This
note is primarily a facts dump to enable future analysis.

## Implications for bot AI / strategy authoring

- **This stays C++ / Level-1 — it is the canonical example of a "hard limit"
  mechanic.** Corner rotation is a multi-actor relay assignment (tank A rotates to
  Horseman B when Mark stacks exceed threshold; tank B rotates from B to C; etc.).
  This requires shared state, per-bot Mark-stack tracking, and a rotation assignment
  algorithm across four simultaneous targets. Generic data shapes (`orbit`,
  `stack`, `spread`) cannot express this — the C++ named actions are the only
  viable implementation.

- **Gate by aura NAME, not spell ID.** `"Mark of Korth'azz"`, `"Mark of Blaumeux"`,
  `"Mark of Rivendare"`, `"Mark of Zeliek"` are the safe detection strings. The
  IDs (28832–28835) are the same on 10 and 25, but NAME-based detection is the
  established defensive pattern (see memory: raid-debuff-difficulty-ids).

- **`find target` is threat-based — all four Horsemen are separate threat lists.**
  A bot only "sees" a Horseman it has threatened. Ranged Horsemen (Blaumeux, Zeliek)
  do not melee-chase; they sit in their corners. A bot that has never attacked a
  corner Horseman will not have it in its `"find target"` results. The C++ action
  `"four horsemen attack in order"` must handle initial aggro establishment across
  all four, not just the nearest threat.

- **`find target` during the opening walk.** While the Horsemen are walking to
  their corners (`REACT_PASSIVE`), they are not attackable in the normal sense.
  Any bot action gated on `find target` for a specific Horseman may return null
  during this window. The C++ named action `"four horsemen opening defensive"` is
  the correct hook for the opening phase rather than a generic encounter_active
  trigger on a single Horseman.

- **Human is never a required slot.** All tank rotation slots and back-pair attractor
  positions must be covered by bots. (memory: human-never-required.)

- **Void Zone positioning.** Blaumeux's Void Zone (`28863`) is a persistent ground
  AoE (same design as Heigan ground). Bots must move out. The C++ `"four horsemen
  avoid void zone"` action handles this. If ever refactored to data, note that the
  avoid-AoE stutter fix (ACTION_EMERGENCY+1 short-circuit) is required for melee
  bots. (memory: avoid-aoe-stutter-fix.)

- **Ranged Horsemen Punish spells.** If no player is within 45y of Blaumeux or
  Zeliek, they cast Unyielding Pain / Condemnation (AoE punish). In a 25-man raid
  the back pair (Blaumeux NW corner, Zeliek NE corner) need at least one tank or
  ranged player within 45y at all times. Bot assignments must guarantee this.

- **No per-difficulty scaling differences in spell IDs.** 10-man and 25-man use
  identical spell IDs; only Mark damage values differ (hard-coded in the aura
  script). No ID branching needed in bot strategy code.
