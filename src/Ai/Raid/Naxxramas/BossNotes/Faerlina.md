# Grand Widow Faerlina — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_faerlina.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (all four of Faerlina's spells have per-difficulty variants — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Grand Widow Faerlina` |
| Boss entry | `15953` |
| Map | Naxxramas, `533` |
| Room / spawn coords | Worshippers spawn ~`3344–3363, -3618–3621, 261.08`; Followers ~`3347–3360, -3617–3619, 261.0` |

## Adds (the core mechanic: sacrifice a Worshipper to cure Frenzy)

| Name | Entry | Difficulty | Count | Role |
|------|-------|------------|-------|------|
| `Naxxramas Worshipper` | `16506` | 10-man AND 25-man | 4 | **Sacrificed** (killed by Widow's Embrace) to cure Faerlina's Frenzy. Present on both difficulties. |
| `Naxxramas Follower`   | `16505` | **25-man only**   | 2 | Extra adds; do NOT cast Widow's Embrace — they just fight. |

All adds are summoned at pull (`SummonHelpers`) and `DoZoneInCombat()` 1.2s after
Faerlina is engaged, so they enter combat immediately. On 25-man: 4 Worshippers + 2 Followers = **6 adds total**.

The Worshippers cast `Widow's Embrace` (28732, 25-man: 54097) on Faerlina when killed
(this is cast TO Faerlina BY the dying Worshipper via `SpellHit`). On 25-man the
casting Worshipper **also dies** (`caster->KillSelf()`) as part of the script — the
sacrifice is real. This removes Frenzy and resets the Frenzy timer by 1 minute.

## Spells

| Spell | Base ID | 25-man ID | Notes |
|-------|---------|-----------|-------|
| Widow's Embrace | `28732` | `54097` | Cast BY a sacrificed Worshipper ON Faerlina. Removes Frenzy, delays next Frenzy by 1 min. Kills the casting Worshipper on 25-man. |
| Frenzy (Enrage) | `28798` | `54100` | Applied to Faerlina at 60–80s intervals. Causes massive damage increase — a wipe if not cured quickly. Blocked while Widow's Embrace is active. |
| Poison Bolt Volley | `28796` | `54098` | Frontal cone poison cast on the tank every 7–15s. Skipped while Widow's Embrace is active. |
| Rain of Fire | `28794` | `54099` | Ground AoE on a random target every 8–18s. The only currently-handled mechanic in `faerlina.json`. |

> **Difficulty-id note:** ALL four of Faerlina's spells have per-difficulty spell IDs
> (`spelldifficulty_dbc` rows confirmed in DB). The 10-man IDs are `28732/28798/28796/28794`;
> the 25-man IDs are `54097/54100/54098/54099`. The engine resolves these via
> `GetSpellIdForDifficulty` in `SpellHit`, meaning the aura that actually lands on Faerlina
> at 25-man is `54097` (Widow's Embrace) or `54100` (Frenzy), not the base IDs.
> **Always gate phase detection on aura NAME, not spell ID**, or your trigger will silently
> miss on 25-man — the user's active difficulty.

## Phase structure

Faerlina has no true phase transitions — it is a single continuous fight with a repeating
Frenzy cycle:

- **Baseline (0–60s+):** Tank Faerlina; dodge Rain of Fire (random target AoE); Poison Bolt
  Volley hits the tank every 7–15s. No special handling needed beyond avoiding the fire.
- **Frenzy (60–80s in, then every ~60s):** Faerlina gains Frenzy (`28798` / `54100`).
  Massive damage spike — tank will likely die without immediate intervention.
  **A Naxxramas Worshipper must be killed during Frenzy** so its Widow's Embrace cast
  lands on Faerlina, removing Frenzy. Frenzy is suppressed if Widow's Embrace is already
  active (the scheduler skips and retries in 30s).
- **Widow's Embrace window:** After a sacrifice, the next Poison Bolt Volley is also
  suppressed (good for the tank). The Frenzy timer resets by 1 minute. Repeat until all
  Worshippers are spent.
- **No more Worshippers:** Once all 4 Worshippers are dead (25-man: 4 sacrificeable),
  Frenzy can no longer be cured. This puts a soft execution timer on the fight — the
  raid must kill Faerlina before Frenzy stacks kill the tank.

## What `faerlina.json` does today vs. what is missing

**What it does (current stub):**
- While Faerlina is `encounter_active`, everyone runs `avoid aoe` (Rain of Fire dodge).
  That's it.

**What is NOT handled:**
- Frenzy detection — no trigger fires when Faerlina gains Frenzy / `54100`.
- Worshipper sacrifice — no logic directs a bot (or the off-tank) to kill a Worshipper
  during Frenzy. This is the central mechanic. Without it, the first Frenzy ~60–80s in
  will almost certainly wipe the raid.
- Follower management — the 2 Followers (25-man only) need to be tanked; currently no
  `tank_adds` or `assist` action is wired.
- Worshipper management before Frenzy — Worshippers should be kept alive until needed;
  DPS bots freely attacking them before Frenzy wastes the cure.

## Implications for bot AI / strategy authoring

- **The core unbuilt mechanic is Frenzy management.** A Naxxramas Worshipper must be
  killed during Frenzy so Widow's Embrace cures her. Until that is wired in `faerlina.json`
  (or companion C++ logic), the first Frenzy at ~60–80s will kill the tank.
- **Gate Frenzy detection by aura NAME, not spell ID.** On 25-man, the active aura is
  `54100`, not `28798`. Use `has aura "Frenzy"` (name-based) or equivalent so the trigger
  fires on both difficulties without per-difficulty branching. Same applies to Widow's
  Embrace (`"Widow's Embrace"`). (Memory: raid-debuff-difficulty-ids.)
- **`find target` won't see Faerlina during a Frenzy-kill sub-task.** The bot asked to
  kill a Worshipper must locate it via something other than boss threat; Worshippers
  enter combat with the zone at pull so they appear in the "attackers" list. A
  `tank_adds` or targeted `attack` action on `Naxxramas Worshipper` by name will work;
  `AI_VALUE2("find target","grand widow faerlina")` returns the boss, not the adds.
  (Memory: findtarget-is-threat-based.)
- **Designate exactly one bot as the Frenzy-kill executor.** A fixed bot slot (e.g.
  assist-tank #0) should be assigned the Worshipper-kill during Frenzy, not the full
  raid — otherwise DPS bots burn Worshippers prematurely and none remain for late Frenzies.
- **The human is never the required Frenzy-killer slot.** Bot-only assignment; human
  participation is optional. (Memory: human-never-required.)
- **Worshippers should be left alive pre-Frenzy.** Until the JSON layer can express
  "hold DPS on adds until condition X", the safest interim approach is to have the
  off-tank hold Worshippers alive and only convert to a kill order when Frenzy lands.
  Consider a Frenzy-triggered `attack` action on `Naxxramas Worshipper` at high priority
  for a single designated bot.
- **Followers (25-man only, 2 adds) need an off-tank.** They don't cast Widow's Embrace
  and can be killed freely. A `tank_adds` action on `Naxxramas Follower` (entry `16505`)
  should be wired to assist-tank #0 or #1 once add handling is built. They enter combat
  immediately at pull via `DoZoneInCombat`.
- **No C++ is strictly required** for the core loop if the JSON layer can express
  Frenzy-aura-triggered priority actions — this should be fully data-expressible given
  the existing `encounter_active` + aura-check trigger infrastructure.
