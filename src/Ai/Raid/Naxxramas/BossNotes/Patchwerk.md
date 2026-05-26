# Patchwerk — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from
`src/server/scripts/Northrend/Naxxramas/boss_patchwerk.cpp` and
`creature_template` / `spelldifficulty_dbc` (world DB), verified 2026-05-26.
**Verify IDs against live DB/source before trusting** (creature_template can
drift; Hateful Strike has per-difficulty spell IDs — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Patchwerk` |
| Boss entry | `16028` |
| Map | Naxxramas, `533` |
| Encounter constant | `BOSS_PATCHWERK = 0` (naxxramas.h) |
| Gate GO | `181123` (GO_PATCHWERK_GATE) |

No fixed room-center coordinate is hardcoded in the script; Patchwerk stands
roughly at the far end of his slime-pipe corridor. No pathing AI — he is a
stationary melee boss with no adds and no phase transitions.

## Adds

None. Pure tank-and-spank. No add table needed.

## Spells

| Spell | ID (10-man) | ID (25-man) | Cast timing | Notes |
|-------|-------------|-------------|-------------|-------|
| Hateful Strike | `41926` | **`59192`** | Every 1s | Targets the highest-HP player within 5y of the boss who is NOT the current victim, among the top `RAID_MODE(1,2)` threat targets. Hits the current victim if no other melee target qualifies. Adds 500 flat threat on hit. |
| Frenzy | `28131` | `28131` (same) | When HP ≤ 5% | Soft enrage. Boss emotes before casting. One-shot, not repeating. |
| Berserk | `26662` | `26662` (same) | 6 min after pull | Hard enrage. Triggers SLIME_BOLT every 3s afterward. |
| Slime Bolt | `32309` | `32309` (same) | Every 3s after Berserk | Only fires post-Berserk. |

> **Difficulty-id note:** Hateful Strike **does** have per-difficulty spell
> rows in `spelldifficulty_dbc` — 10-man fires spell `41926`, 25-man fires
> spell `59192`. Frenzy (28131), Berserk (26662), and Slime Bolt (32309) have
> **no** difficulty rows; the same id fires on both modes. If you gate
> off-tank logic on a Hateful Strike aura, detect it by **aura NAME** (not
> spell id), or check both ids, or just position off-tanks in melee regardless
> — that is safer. (General rule: gate by aura NAME when in doubt; see memory:
> raid-debuff-difficulty-ids.)

## Phase structure

Single phase — no transitions, no balcony, no adds, no immunity windows.

- **Pull:** Patchwerk enters combat with the zone (`SetInCombatWithZone()`),
  schedules Hateful Strike for 1.5s, Berserk for 6 min, and the HP health-
  check every 1s.
- **Throughout:** tanks in melee, off-tanks stack in melee to soak Hateful
  Strike. Hateful Strike fires every 1 second (repeating with `events.Repeat(1s)`).
- **≤5% HP:** Frenzy (soft enrage) fires once.
- **6 min:** Berserk fires, then Slime Bolt every 3s — wipe timer.

No timers, no teleports, no adds to grab. The only coordination required is
**off-tanks standing in melee** to keep Hateful Strike off the main tank.

## JSON strategy — `patchwerk.json` (added 2026-05-26)

A `json-raid` strategy now exists (`server/raid_strategies/patchwerk.json` +
git copy under `mod-playerbots/data/raid_strategies/`). Two rules, both the
generic `encounter_active` trigger + `attack` shape:

- `role: tank` → `attack patchwerk` — all tanks glued to the boss in melee
  (build threat + reach melee), so the two highest-HP/highest-threat bodies are
  the only Hateful Strike-eligible targets. **This is the entire soak as data.**
- `role: dps` → `attack patchwerk` — melee close in; ranged attack from class
  cast range, naturally staying outside the 5y strike radius. Healers get no
  rule (class heal strategy runs untouched).

**Intentionally NOT ported** from the commented-out C++ below: `rear flank`
(melee-behind) and the 12–15y `ranged position` hold. There is no Level-2 shape
for "hold distance from boss", and `RearFlankAction` moves a bot to
half-melee-range gated on **angle only** — wiring it to any group containing
ranged would pull casters into the 5y kill zone. Neither is mechanically needed
on Patchwerk (no cleave, no parry-haste), so both were dropped.

## Existing (commented-out) C++ AI summary

All Patchwerk-specific C++ code in `RaidNaxxActions_Patchwerk.cpp`,
`RaidNaxxStrategy.cpp`, `RaidNaxxTriggers.cpp`, and `RaidNaxxTriggerContext.h`
is **entirely commented out**. There is no active bot AI specific to Patchwerk.

The commented-out code sketched three triggers and actions:
- `patchwerk tank` trigger → `tank face` action (main tank facing)
- `patchwerk ranged` trigger → `patchwerk ranged position` action (keep ranged
  12–15y from boss; the action class body is also fully commented out in
  `RaidNaxxActions_Patchwerk.cpp`)
- `patchwerk non-tank` trigger → `rear flank` action (non-tank melee flank)

None of these wires are registered in `RaidNaxxTriggerContext.h`. The fight
currently runs on **generic bot AI only** (auto-attack, default tank aggro,
no off-tank Hateful Strike soaking).

## Implications for bot AI / strategy authoring

- **Fully data-expressible.** There are no phase clocks, no positional puzzles
  (Heigan-style), no adds, no immunity windows. The entire encounter reduces
  to: off-tanks stand in melee. This is the definition of a `json-raid`
  candidate — no new C++ needed.

- **The core mechanic: Hateful Strike soaking.** On 25-man, Patchwerk scans the
  top-**2** threat players (excluding the victim) within 5y and hits the
  highest-HP one. Off-tanks must be (a) in melee range (≤5y) and (b) high
  enough on the threat list to appear in the top-2. Two dedicated off-tanks
  stacking on the boss in melee satisfies both conditions. Ranged DPS must stay
  **outside 5y** so they are never eligible targets — a critical constraint.

- **`find target` works normally here.** Unlike Noth (who goes non-selectable
  during balcony), Patchwerk is always in combat and always threateable.
  `AI_VALUE2("find target", "patchwerk")` is safe throughout the fight.
  (memory: find-target-is-threat-based)

- **Off-tank slot: bots only, human never required.** Assign assist-tank #0 and
  #1 to stand in melee. Do not require the human player to fill an off-tank
  role. (memory: human-never-required)

- **Ranged DPS positioning.** Keep ranged and healers > 5y from the boss. The
  commented-out `patchwerk ranged position` action tried 12–15y. A `json-raid`
  `stay_back` or equivalent is the data-layer way to express this.

- **Frenzy and Berserk are wipe timers, not reactable mechanics.** Frenzy at 5%
  HP is cosmetic (fight is almost over). Berserk at 6 min is a DPS check —
  nothing the bot AI can do about it except burn the boss. No action needed for
  either.

- **Hateful Strike aura detection:** If ever gating logic on whether a bot is
  struck by Hateful Strike, gate by aura NAME (both difficulty IDs share the
  name "Hateful Strike"), not by spell id `41926` alone (that misses 25-man id
  `59192`). (memory: raid-debuff-difficulty-ids)

- **No stutter risk.** Unlike ground-AoE fights (Heigan, Grobbulus), Patchwerk
  has no ground effects. The avoid-aoe stutter fix is irrelevant here.
  (memory: avoid-aoe-stutter-fix)

- **Strategy suggestion:** A minimal `json-raid` strategy for Patchwerk needs
  only: `tank` role attacks and holds aggro; `assist_tank` roles (two of them)
  stack in melee at the boss; `ranged`/`healer` roles stay > 10y. That's the
  entire encounter expressed as data.
