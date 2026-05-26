# Noth the Plaguebringer — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_noth.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Noth the Plaguebringer` |
| Boss entry | `15954` |
| Map | Naxxramas, `533` |
| Room center / boss ground anchor | `2684.94, -3502.53, 261.31` (o `4.7`) |
| Leash | evades if >80y from `2684.8, -3502.5, 261.3` |

## Adds (the off-tank's job: gather → center → cleave)

| Name | Entry | When | Count 10 / 25 |
|------|-------|------|---------------|
| `Plagued Warrior`  | `16984` | Ground phase, every 30s | 2 / 3 |
| `Plagued Champion` | `16983` | Balcony waves 1 & 2 | wave1 2/4, wave2 1/2 |
| `Plagued Guardian` | `16981` | Balcony waves 2 & 3+ | wave2 1/2, wave3+ 2/4 |

Adds spawn at 5 fixed points ringing the room (`summoningPosition[5]` in the script,
~`2728,-3535` / `2725,-3514` / `2728,-3465` / `2704,-3459` / `2652,-3459`, all z≈263)
and are `SetInCombatWithZone()` on spawn, so they immediately enter the group's
combat/"attackers" list.

## Spells

| Spell | ID | Notes |
|-------|----|-------|
| Curse of the Plaguebringer | `29213` | Ground, every 25s. Max targets 3 (10) / **10 (25)**. Decurse or it triggers Wandering Plague AoE. |
| Cripple | `29212` | **25-man only**, fires with Blink. |
| Blink | `29208` | **25-man only**, every 30s from 26s. **Resets the threat list** (`DoResetThreatList`) — tanks must re-establish. |
| Summon Plagued Warriors | `29237` | Summon visual (the actual adds are spawned in code, not by this spell). |
| Teleport (to balcony) | `29216` | Enters balcony phase. |
| Teleport Back | `29231` | Returns to ground. |
| Berserk | `68378` | After the 3rd balcony return. |

> **Difficulty-id note:** Noth scales by *value* (`RAID_MODE`/`SPELLVALUE_MAX_TARGETS`),
> **not** by separate per-difficulty spell rows — the same spell ids fire on 10 and 25.
> So detecting his abilities by id or name is safe on the user's active 25-man path.
> (This is *not* universally true for Naxx debuffs — keep gating phases by aura NAME.)

## Phase structure

- **Ground (110s):** tank-and-spank. Curse every 25s; Warrior wave every 30s. On 25-man,
  Blink every 30s wipes threat → re-aggro needed. → teleports to balcony.
- **Balcony (70s):** Noth goes `REACT_PASSIVE` + `UNIT_FLAG_NOT_SELECTABLE` and stops
  attacking — **he is non-attackable and OFF EVERYONE'S THREAT.** Only the summoned adds
  remain in combat. Champion/Guardian waves every 30s. → teleports back to ground.
- Repeats; after the **3rd** balcony return Noth Berserks.

## Implications for bot AI / strategy authoring

- **Balcony phase breaks `find target` detection.** Because Noth clears threat and goes
  non-selectable, `AI_VALUE2("find target", "noth the plaguebringer")` returns null for
  the whole raid during balcony. Any `encounter_active` rule gated on **Noth** goes
  silent exactly when the adds matter. Detect the fight via the **adds / the group
  "attackers" list** instead — e.g. the generic `"has attackers"` trigger (fires on
  `attacker count >= 1`, the same list `tank_adds` reads), which stays true in both
  phases. (See memory: findtarget-is-threat-based, raids-25man.)
- **Off-tank tactic:** assist-tank #0 gathers all three Plagued add types and stacks them
  for a raid cleave. `tank_adds` already self-detects from "attackers" (sees loose adds),
  self-gates to assist-tank #0, and does attack → taunt → reposition. It matches **one**
  name per action, so list all three add types (one action each). It currently drags to
  the main tank's live position; a fixed **center** anchor (`2684.94, -3502.53, 261.31`)
  is more predictable in the balcony phase (the MT isn't tanking anything then).
- **The human is never the off-tank slot** — bots cover it. (memory: human-never-required.)
- Nothing here needs a positional phase clock, so no C++ is required; this is fully
  data-expressible with `has attackers` + `tank_adds`.
