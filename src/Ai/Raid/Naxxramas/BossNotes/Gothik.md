# Gothik the Harvester — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_gothik.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; check per-difficulty spell ids if adding
debuff detection).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Gothik the Harvester` |
| Boss entry | `16060` |
| Map | Naxxramas, `533` |
| Boss start position (platform) | `2642.14, -3387.0, 285.49` (o `6.27`) |
| Gate Y-coordinate | `POS_Y_GATE = -3360.78` — everything south of this is LIVING side, north is DEAD side |
| Living-side spawn row | 6 positions along `y ≈ -3428 / -3431`, `x = 2669/2692/2714` |
| Dead-side spawn points | 5 positions `y ≈ -3304 to -3348`, `x ≈ 2664–2733` |

## Adds

> **25-man counts shown where different from 10-man.**

### Living side (spawn at south row; attack players on LIVING side only until gate opens)

| Name | Entry | DB-verified name | Spell / ability | Count 10 / **25** |
|------|-------|-----------------|-----------------|-------------------|
| Unrelenting Trainee | `16124` | `Unrelenting Trainee` | Death Plague `55604` every 4–7s | 2 / **3** per wave |
| Unrelenting Death Knight | `16125` | `Unrelenting Death Knight` | Shadow Mark `27825` every 5–7s | 1 / **2** per wave |
| Unrelenting Rider | `16126` | `Unrelenting Rider` | Shadow Bolt Volley `27831` every 5s (hits non-Shadow-Marked targets only) | 1 / **1** per wave |

### Dead side (spawn at north cluster when their living counterpart dies; via trigger chain)

| Name | Entry | DB-verified name | Spell / ability |
|------|-------|-----------------|-----------------|
| Spectral Trainee | `16127` | `Spectral Trainee` | Arcane Explosion `27989` every 2.5s (range ≤20y) |
| Spectral Death Knight | `16148` | `Spectral Death Knight` | Whirlwind `56408` every 4–6s (melee) |
| Spectral Horse | `16149` | `Spectral Horse` | Stomp `27993` every 4–9s (melee) |
| Spectral Rider | `16150` | `Spectral Rider` | Drain Life `27994` every 8–12s; Unholy Frenzy `55648` every 15–17s |

**How the dead-side spawn chain works:** when a living add dies it casts a trigger
spell (e.g. `SPELL_ANCHOR_1_TRAINEE = 27892`). An invisible NPC Trigger (`16137`,
`Naxxramas Military Sub-Boss Trigger`) picks this up, relays it to a random dead-side
skull-pile trigger, which then summons the spectral counterpart via `DoSummon`.
The summoned creature is immediately registered in Gothik's own summon list.

## Boss spells (Phase 2 / after descent)

| Spell | ID | Notes |
|-------|----|-------|
| Harvest Soul | `28679` | Gothik's signature self-cast. Fires every 15s (with ±random start 5–15s). Steals life from players; buffs Gothik. |
| Shadow Bolt | `29317` | Primary nuke, cast every ~1s on his current victim. |
| Teleport to Dead side | `28025` | `SPELL_TELEPORT_DEAD` — Gothik jumps to dead side during phase-2 side-swaps. |
| Teleport to Live side | `28026` | `SPELL_TELEPORT_LIVE` — Gothik jumps to living side; also used at wave-sequence end to descend. |

> **Difficulty-id note:** Gothik scales add **count** via `Is25ManRaid()` checks in code
> (not by separate per-difficulty spell rows). The boss's own spells (`28679`, `29317`)
> fire identically on 10 and 25. Shadow Bolt Volley (`27831`) on the Unrelenting Rider
> is filtered by the `SPELL_SHADOW_MARK` aura — detect it by spell **name** rather than
> id if you need aura gating, as a precaution. No evidence of per-difficulty id split
> in this script, but verify against `spell_dbc` if adding aura-name logic.

## Phase structure

### Phase 1 — Wave sequence (Gothik on platform, immune to damage)

Gothik stands on a **raised platform** to the west (`z ≈ 285`), inaccessible to
players. He is `REACT_PASSIVE`, `UNIT_FLAG_DISABLE_MOVE`, and `DamageTaken` forces
all incoming damage to 0. He is **completely non-attackable** for the entire wave
sequence.

- First add spawns at **30s** after pull. Subsequent spawns follow the `gothikWaves[24]`
  table — a fixed sequence of 23 entries (the 24th is the terminator `{0,0}`).
- Gaps between waves range from 0 ms (instant double-spawn) to 29s.
- Add breakdown across the full sequence (approximate totals):
  **Trainees** (~10–12 waves, 2 or 3 each), **Death Knights** (~6 waves, 1 or 2 each),
  **Riders** (~4 waves, 1 each). On **25-man**, extra Trainees and a second DK per wave
  significantly raise pressure.
- A **2-minute safety check** (`EVENT_CHECK_PLAYERS`) verifies at least one player is
  alive on each side (`CheckGroupSplitted`). If the group fails to split, the gate
  opens early and all adds go free-for-all.

### Phase 1 gate rule: LIVING side must be covered

The core script's `JustSummoned` targets living-side adds at living-side players
**only** (checks `IN_LIVE_SIDE(summon) == IN_LIVE_SIDE(player)`). If no living-side
player exists, adds attack anyone. The **group must maintain a living-side presence**
for the entire wave phase or loses positional control.

### Phase 2 — Gothik descends (gate opens)

When the `gothikWaves` sequence ends (all 23 entries processed), the terminator
`{0,0}` fires:
1. `secondPhase = true`
2. Gothik talks (`SAY_PHASE_TWO`) and emotes (`EMOTE_PHASE_TWO`).
3. He casts `SPELL_TELEPORT_LIVE` (descends to living side).
4. Becomes `REACT_AGGRESSIVE`, moves freely, `ImmuneToPC = false`.
5. **Gate opens automatically** (no timed trigger needed — `OpenGate()` is called
   when `CheckGroupSplitted` fails, OR when Gothik drops below **30% HP** in phase 2,
   OR by the safety-check event).
6. Schedules: Shadow Bolt every 1s, Harvest Soul at 5–15s then every 15s, Teleport
   every 20s (bounces between sides while > 30% HP), health-check every 1s.

**Phase 2 side-swaps:** Gothik teleports between living and dead sides every 20s
until his HP drops below 30%. When he teleports he briefly pauses attack (`REACT_PASSIVE`
for 2s then `REACT_AGGRESSIVE`). If his current victim is on the wrong side after a
swap he finds a same-side target or opens the gate.

**Gate at 30% HP:** `EVENT_CHECK_HEALTH` fires every 1s in phase 2. At < 30% it
calls `OpenGate()` (if not already open) and cancels further teleports → from here
it is a straightforward tank-and-spank on the now-merged room.

## Implications for bot AI / strategy authoring

- **HARD LIMIT — side-split is C++ territory, not data-expressible.**
  The core mechanic — placing some bots on the living side and some on the dead side,
  then holding those positions for ~3–4 minutes of waves — requires coordinate-based
  positional assignment and per-tick enforcement. No JSON rule or generic shape can
  express "bot X stays south of y = -3360.78 while bot Y stays north of it" for the
  duration of the wave phase. This is the same class of hard-limit as Heigan dance.
  Any Gothik bot AI must be C++.

- **Why `GothikGenericMultiplier` is disabled.**
  The multiplier class is fully commented out in both `RaidNaxxMultipliers.h` (lines
  99–106) and `RaidNaxxMultipliers.cpp` (lines 323–346), and the registration line in
  `RaidNaxxStrategy.cpp` (line 263) is also commented out. The stub body attempted
  to: (a) get Gothik via `AI_VALUE2("find target", "gothik the harvester")` — which
  returns null in phase 1 because Gothik has no threat list — and (b) read Gothik's
  `EventMap` phase mask to suppress `FollowAction` and `AttackAction` (boss targeting)
  during phase 1. The implementation was broken on two counts: the boss is unreachable
  via `find target` for the entire wave phase (same problem as Noth's balcony), and
  the variable name `boss_botAI` in the event-map line is a typo (should be
  `boss_ai`). No triggers exist for Gothik; `RaidNaxxTriggers.cpp` and
  `RaidNaxxTriggerContext.h` have no Gothik entries. The action file
  `RaidNaxxActions_Gothik.cpp` contains only a comment placeholder. In short:
  **the C++ AI is a stub — it was scaffolded but never implemented**, and the
  multiplier was commented out rather than fixed.

- **No JSON strategy yet; C++ AI exists but its multiplier is disabled** — does
  nothing in-game. `RaidNaxxActions_Gothik.cpp` is empty. The full encounter AI must
  be written from scratch in C++.

- **`find target` cannot see Gothik during phase 1** — he has no threat list (`REACT_PASSIVE`,
  `UNIT_FLAG_DISABLE_MOVE`, damage blocked). Any AI code that needs to detect the
  fight is in phase 1 must gate on the **add NPCs** (Unrelenting/Spectral entries) via
  the "has attackers" / attackers list, not on Gothik's name. (Same pattern as Noth
  balcony; see memory: findtarget-is-threat-based.)

- **Two separate combat zones during the wave phase.** Living-side bots fight
  Unrelenting adds (melee/physical threat). Dead-side bots fight Spectral adds
  (AoE arcane from Trainee, magic from Rider). Assignment must be by fixed key
  (GUID or group-slot), never by live position — bots chasing each other across
  the gate line breaks the split. (See memory: multibot-positioning-fixed-keys.)

- **Living-side add targeting.** Unrelenting adds are `SetInCombatWithZone()` on
  spawn and start attacking a random same-side player. They enter living-side bots'
  threat lists immediately. `tank_adds` should be usable for living-side bots.
  Dead-side bots will not see living-side adds in their "find target" / attackers
  list (different side, no threat generated cross-gate).

- **Shadow Bolt Volley from Unrelenting Rider hits non-Shadow-Marked players only.**
  The `spell_gothik_shadow_bolt_volley` SpellScript filters targets to those
  **without** `SPELL_SHADOW_MARK (27825)`. Living-side bots near the Rider will be
  hit; dead-side bots are shielded by the gate.

- **Dead-side Spectral Trainee AoE is frequent and close-range (≤20y, every 2.5s).**
  Dead-side melee bots will take heavy Arcane Explosion damage. Dead-side healers
  need awareness that the Trainee threat is arcane, not physical. No avoid-aoe
  escape needed (AoE is instantaneous, not a ground effect) but healing demand
  is high.

- **Gate opens → full room merge.** Once the gate opens (end of waves, 30% HP, or
  safety check), all surviving adds become free-for-all, Gothik is attackable, and
  the fight becomes standard. Phase-2 Gothik side-swaps every 20s until 30% — the
  main tank must maintain aggro through brief REACT_PASSIVE windows.

- **Harvest Soul is a periodic self-buff/drain.** Fires every 15s in phase 2. Treat
  as a periodic threat; dispel if possible, otherwise expect sustained healing
  demand on the main tank.

- **The human is never a required slot** — bots must be able to cover both the living
  side tank/DPS and the dead side entirely. Assign human to either side as optional.
  (See memory: human-never-required.)

- **25-man wave pressure is substantially higher.** Phase 1 spawns an extra Trainee
  per trainee wave and a second Death Knight per DK wave. Living-side bots need an
  off-tank or at minimum a dedicated add-kite role to avoid being overwhelmed before
  the gate opens.
