# Gluth — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_gluth.cpp`, `RaidNaxxSpellIds.h`,
`RaidNaxxBossHelper.h`, `RaidNaxxActions_Gluth.cpp`, and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Gluth` |
| Boss entry | `15932` |
| Map | Naxxramas, `533` |
| Room center / leash anchor | `~3293.0, -3142.0` (CircleBoundary r=80, instance_naxxramas.cpp line 52) |
| Boss encounter index | `BOSS_GLUTH = 2` (naxxramas.h), used as instance boss-state id |

## Adds

| Name | Entry | When | Count 10 / 25 |
|------|-------|------|---------------|
| `Zombie Chow` | `16360` | Every 10s throughout the fight | **1 (10-man)** / **2 (25-man)** |

**10-man**: chow always spawn from the center gate (`zombiePos[0]` = `{3267.9, -3172.1, 297.42}`).  
**25-man**: each of the 2 chow per wave spawns from a random gate among all three
(`zombiePos[0..2]` = center, left `{3253.2, -3132.3}`, right `{3308.3, -3185.8}`).  
Chow spawn with `AttackStart(boss)` — they pathfind straight toward Gluth. They are
**not** `SetInCombatWithZone()`, so bots do not automatically have them on their
"attackers" list; the designated handlers must actively pull threat on them.  
If a chow reaches Gluth he eats it via `SPELL_CHOW_SEARCHER` (28404) AoE,
healing himself for **5% of maximum HP** per chow consumed.

## Spells

| Spell | ID | Difficulty | Cadence | Notes |
|-------|----|------------|---------|-------|
| Mortal Wound | `25646` (10) / `54378` **(25)** | Both | Every 10s | Stacking healing-reduction debuff on the current tank. Tank swap when ≥5 stacks. |
| Frenzy (Enrage) | `28371` | Both | Every 22s | Dispellable enrage. Triggered by `EVENT_ENRAGE` at 22s repeat. Hunters: Tranquilizing Shot. |
| Decimate | `28374` (10) / `54426` **(25)** | Both | **110s (10)** / **90s (25-man)** | AoE that drops ALL targets (players + zombies) to exactly 5% HP. Zombie Chow then sprint to Gluth at full speed. The raid must burst all chow before Gluth eats them. |
| Decimate (damage component) | `28375` | Both | — | Script-effect per-target damage to reach exactly 5%; fires from the main Decimate hit. Also used as `Decimate25Alt` in bot code. |
| Berserk | `26662` | Both | 6 min | Hard enrage, fight-ending. |
| Infected Wound | `29306` | Both | — | Gluth is immune to this (self-applied immunity in `Reset()`). Not cast at players. |
| Chow Searcher | `28404` | Both | Every 1s (eat-check) | AoE scan: eats any chow within 20-unit radius when Gluth's victim is a chow within melee range. |

> **Difficulty-id note:** Mortal Wound and Decimate **do have separate per-difficulty
> spell ids** — `25646`/`28374` on 10-man, `54378`/`54426` on 25-man. The bot code
> already handles this via `NaxxSpellIds::GetAnyAura(mt, {MortalWound10, MortalWound25})`
> (checks both) with a name-fallback `"mortal wound"` for safety. Always gate Mortal
> Wound detection by **aura NAME** in addition to id, or check both ids explicitly.
> Frenzy/Enrage (`28371`) and Berserk (`26662`) appear to be shared across difficulties
> (single id in boss_gluth.cpp enum, no RAID_MODE branching on them).

## Phase structure

Gluth is a **single-phase fight** with no transitions. Everything runs concurrently
on a repeating clock from the pull:

| t=0 | Pull. `SetInCombatWithZone()`. |
|-----|---|
| t=10s | First Mortal Wound (repeats every 10s). First Zombie Chow wave (repeats every 10s). Eat-check loop starts (1s tick). |
| t=22s | First Frenzy/Enrage (repeats every 22s). |
| t=90s **(25)** / t=110s (10) | First **Decimate** (repeats at same interval). All combatants → 5% HP; surviving chow sprint boss. |
| t=6 min | Berserk (fight-ending). |

No phase-flag, no transition events, no selectable phases. The fight is a DPS race
against the 6-minute Berserk punctuated by the Decimate windows.

## Implications for bot AI / strategy authoring

### Current C++ AI (no JSON strategy yet)

No JSON strategy yet. The existing C++ AI handles this entirely:

- **`GluthTrigger`** (`IsActive` = `UpdateBossAI() || GluthEngaged()`): fires the
  four Gluth actions for every bot from the pull, including kiters/off-tanks who
  never threaten Gluth (threat-independent via instance boss-state, same trick as
  FourHorsemen). The trigger is wired to actions at `ACTION_RAID`/`ACTION_RAID+1`/
  `ACTION_RAID+2` in `RaidNaxxStrategy.cpp`.
- **`GluthMainTankMortalWoundTrigger`**: fires on assist-tank #0 when the main tank
  has ≥5 stacks of Mortal Wound (by id `25646`/`54378` + name fallback); result is
  `taunt spell` at `ACTION_RAID+1`. Tank swap is implemented.
- **`GluthGenericMultiplier`**: suppresses `DpsAssist`, `TankAssist`, `Flee`, and
  `CombatFormationMove` / `Follow` actions for kiters and off-tanks (governed
  threat-independently from the pull), preventing them from charging Gluth or
  following the master instead of doing their jobs. For the main-tank and DPS it
  suppresses `DpsAssist` once threat is established, keeping DPS on Gluth.
- **`GluthChooseTargetAction`**: snaps all non-kiter/non-off-tank bots onto Gluth,
  but yields if a bot already has a low-HP (decimated) chow targeted during the
  burn window.
- **`GluthPositionAction`**: tanks go to fixed tank anchor; 25-man ranged/healers
  spread by fixed group-slot index into a fan cluster at `rangedClusterPos25 =
  {3300.0, -3140.0}` (SW of boss, away from chow gates), preventing the north-door
  escape and minimizing chow contact.
- **`GluthSlowdownAction`** (kiters + off-tanks only, 25-man):
  - *Kiters* (first 2 bot hunters + first 2 bot mages): orbit a SW kite ring
    (center `{3276.0, -3160.0}`, radius 20y) continuously, snaring/rooting chow
    with class-appropriate spells. Mages bank Frost Nova for the Decimate burn;
    hunters prioritize Concussive Shot on the "leak" (chow nearest Gluth).
    During the burn window (`InDecimateBurn`): pivot to AoE/root the pack in place.
  - *Off-tanks* (assist-tanks #0 and #1): hold at ±12y flanks of the ring center
    with AoE-threat spells; orbit when ≥6 chow are piled on them.
- **`GluthBurnAddsAction`**: during the Decimate burn window, all non-handler DPS
  bots pivot off the boss and target the nearest low-HP chow within engage range
  (35y ranged / 10y melee). Human and healers are excluded.
- **`InDecimateBurn` detection**: reactive, not clock-based — triggers when Gluth is
  currently casting Decimate **or** when any chow is at ≤10% HP. This is a
  **reactive signal**, not a positional phase clock; no hard-coded timer is needed.

### Hard-limit vs data-expressible

- **Tank swap (Mortal Wound):** data-expressible in principle — the trigger is
  stacks of an aura, the action is a taunt. Already handled in C++ and working.
  A JSON layer could wire `tank_swap` on `"mortal wound" stacks >= 5`, but the
  C++ implementation is solid and trusted.
- **Zombie Chow handling (kiting / AoE / off-tank hold):** **hard-limit** for the
  detailed per-class kite orbit behavior. The orbit movement math (bearing + KITE_LEAD
  step, ring radius, flank anchors, escape blink/disengage) and the per-class snare
  priority table (`CastZombieThreat`) cannot be expressed in current json-raid
  primitives. Leave in C++. `tank_adds` could cover a simplified "chase and AoE"
  but would miss the orbital kite logic entirely.
- **Decimate burn pivot:** **reactive, data-expressible in concept** — trigger on
  chow HP ≤ 10% or on Decimate aura, action = attack chow. The current C++
  `GluthBurnAddsAction` already does this cleanly. A JSON `encounter_active` +
  `attack` action on chow could approximate it for the DPS pivot, but the kiter
  AoE-root logic during the burn is still C++-only.
- **Decimate itself:** drops all players to 5% HP — this is a server-side mechanic,
  not a bot positioning problem. No bot action required to "survive" Decimate; bots
  just need to be alive and burst chow immediately after.
- **Frenzy (Enrage):** hunters already have Tranquilizing Shot in their generic
  rotation; no Gluth-specific wiring needed.
- **`find target` note:** Gluth stays in combat and on the threat list throughout —
  no phase where he goes non-selectable. `AI_VALUE2("find target", "gluth")` is
  reliable for the whole fight for any bot that has threatened him. Kiters and
  off-tanks intentionally skip this (they use `GluthNearby()` / instance boss-state
  instead, since they must never build threat on Gluth).
- **Human is never required:** `GluthBurnAddsAction` excludes healers and checks
  bot role only. The human is not assigned to kiter or off-tank slots (those are
  selected from the *bot* roster). All required slots (MT, 2 OTs, kiters) are
  covered by bots. (memory: human-never-required.)
- **Aura NAME fallback:** always pair spell-id checks with a name fallback
  (`"mortal wound"`) for robustness across difficulty ids, per established pattern.
