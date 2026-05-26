# Thaddius — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_thaddius.cpp`, `RaidNaxxActions_Thaddius.cpp`,
`RaidNaxxBossHelper.h`, `RaidNaxxSpellIds.h`, and `creature_template` (world DB),
verified 2026-05-26. **Verify IDs against the live DB/source before trusting**
(creature_template can drift; raid debuffs sometimes have per-difficulty spell ids —
see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Thaddius` |
| Boss entry | `15928` |
| Map | Naxxramas, `533` |
| Platform / boss ground anchor | ~`3512.19, -2928.58, 304.02` (low platform center; spawns at home position, drops to low platform after adds die) |
| Berserk | 6 minutes after Thaddius phase begins (`ScheduleEnrageTimer(SPELL_BERSERK, 6min)`) |

## Adds

| Name | Entry | Platform | Spells |
|------|-------|----------|--------|
| `Stalagg` | `15929` | Left (NW) — tank anchor `3436.14, -2919.98, 312.61` | Power Surge (`54529`) every 19s; Magnetic Pull (`28337`) every 20s (synchronized with Feugen) |
| `Feugen` | `15930` | Right (SE) — tank anchor `3522.94, -3002.60, 312.61` | Static Field (`28135`) every 3s; Magnetic Pull (`28337`) every 20s (synchronized with Stalagg) |
| `Tesla Coil` (NPC) | `16218` | Between adds and Thaddius | Passive channeler; kills itself when Thaddius activates |

**Death-sync mechanic (HARD LIMIT — C++ only):** Both adds must die within ~5 s of
each other. The script detects via `ACTION_SUMMON_DIED` signal: the first death sets
`summonTimer = 1` and after 5000 ms calls `ACTION_RESTORE` on all summons — any
surviving add is fully revived at full HP (or max-health restored if still alive).
No JSON shape can enforce kill-timing across two independently-tanked targets.

**Overload / Tesla Coil:** If either add is pulled more than **28 yards** from its
home position, the Tesla Coil link breaks, the coil enters overload mode, and
randomly shocks a player every 1.5 s (`SPELL_TESLA_SHOCK = 28099`). Keep adds on
their platforms.

**Magnetic Pull:** Every 20 s Stalagg's AI swaps the two tanks' threat values and
applies `SPELL_MAGNETIC_PULL` to both tanks — each tank is pulled to the opposite
add, briefly stunned (3 s via `pullTimer`), then released. Tanks need to quickly
re-establish position. The C++ AI already handles this via `ThaddiusPhasePetLoseAggroTrigger`
→ `taunt spell` (ACTION_RAID + 2) when a tank bot loses victim on its target.

## Spells (Thaddius phase)

| Spell | ID | Notes |
|-------|----|-------|
| Polarity Shift | `28089` | AoE, every 30s from 20s. Randomly assigns each player Positive or Negative Polarity. **See charge spells below.** |
| Positive Polarity (marker) | `28059` | Applied by Polarity Shift; 10-man version / base aura |
| Positive Charge (damage proc) | `28062` | **25-man** — damages nearby players with OPPOSITE polarity; grants stacking damage buff to same-polarity neighbors (`SPELL_POSITIVE_CHARGE_STACK = 29659`) |
| Positive Charge Stack | `29659` | Stacking buff; stacks = count of same-polarity players within range |
| Negative Polarity (marker) | `28084` | Applied by Polarity Shift; 10-man version / base aura |
| Negative Charge (damage proc) | `28085` | **25-man** — same mechanism; damages opposite-polarity neighbors; stacks `SPELL_NEGATIVE_CHARGE_STACK = 29660` |
| Negative Charge Stack | `29660` | Stacking buff |
| Chain Lightning | `28167` | Cast on current victim every 15s, starting 14s after activation; jumps to nearby players |
| Ball Lightning | `28299` | Fired at highest-threat target when NO player is in melee range; encourages melee to stay in close |
| Berserk | `27680` | 6 minutes after Thaddius phase starts |

> **Difficulty-id note:** Thaddius itself has **no `RAID_MODE` calls** in the boss
> script — all scaling comes from spell data. However, the Positive/Negative Charge
> damage procs have **separate 10-man and 25-man spell IDs**:
> - 10-man Positive Polarity marker: `28059` / 25-man Positive Charge proc: `28062`
> - 10-man Negative Polarity marker: `28084` / 25-man Negative Charge proc: `28085`
>
> The existing C++ AI detects polarity by checking both the 10-man and 25-man ids
> **plus** the name (`"positive charge"` / `"negative charge"`) as a fallback — the
> right approach. **Always gate polarity detection by aura NAME** (not just id) for
> robustness on the active 25-man path. (See memory: raid-debuff-difficulty-ids.)

## Phase structure

- **Add phase (Feugen + Stalagg):** Both adds must be tanked on their separate
  platforms (left for Stalagg, right for Feugen) and killed within ~5 s of each
  other. Tesla Coil links must be maintained (adds stay ≤28 y from home). Magnetic
  Pull swaps tanks every 20 s. Power Surge (Stalagg) and Static Field (Feugen)
  deal raid-wide damage if adds aren't controlled. Thaddius himself is `UNIT_FLAG_NON_ATTACKABLE`
  and rooted during this phase.

- **Transition:** After both adds die within the window, Thaddius waits ~12 s
  (reviveTimer → 12000 ms), Tesla Coils fire `SPELL_SHOCK_VISUAL` at Thaddius,
  then he activates (~750 ms + 1 s stagger). Players should move to the low platform
  center during transition (`IsPhaseTransition()` trigger → `thaddius move to platform`
  action, target `3512.19, -2928.58, z=304.02`).

- **Thaddius phase:** Tank-and-spank with two overlaid mechanics:
  - **Polarity Shift** every 30s: randomly marks every player Positive or Negative.
    Same-polarity groups must cluster together; opposite-polarity players within
    range deal heavy damage to each other and strip the stacking buff. Correct
    grouping = escalating damage buff.
  - **Chain Lightning** every 15s on victim; avoid chain-jumping by not standing
    in a dense mixed blob.
  - **Ball Lightning** when no one is in melee range — melee should stay close.
  - No position-based adds; berserk at 6 min.

## Existing C++ AI (no JSON strategy yet)

No JSON strategy yet; existing C++ AI does:

- **`ThaddiusBossHelper`** (in `RaidNaxxBossHelper.h`) — phase detection helper used
  by all Thaddius actions and triggers:
  - `IsPhasePet()` — true while either Feugen or Stalagg is alive (uses `find target`).
  - `IsPhaseTransition()` — true after both pets dead but Thaddius still has `UNIT_FLAG_NON_ATTACKABLE`.
  - `IsPhaseThaddius()` — true otherwise.
  - `GetNearestPet()` — returns whichever of Feugen/Stalagg is closer to the bot.
  - Platform anchor coordinates for tank and ranged per add.

- **`ThaddiusGenericMultiplier`** (registered in strategy) — gating multiplier:
  - Zeroes out `CombatFormationMoveAction` (no formation movement ever during Thaddius).
  - During pet phase: zeroes out `DpsAssist`, `TankAssist`, `CastDebuffOnAttacker`,
    `ReachPartyMemberToHeal`, `BuffOnMainTank` — forces bots to use the dedicated
    Thaddius actions instead of generic behavior.
  - During pet phase at ≤40% HP: zeroes out all non-healing `CastSpellAction`s on
    bots attacking the further-ahead add (i.e., damage is soft-throttled to keep
    the two adds roughly equal HP for the synchronized kill).

- **`ThaddiusAttackNearestPetAction`** (`thaddius attack nearest pet`) — fires on
  `thaddius phase pet` trigger (ACTION_RAID+1). Moves each bot to attack whichever
  add is nearer. If a tank bot has aggro, it moves to the pre-set tank anchor for
  that add; if ranged, to the ranged anchor. Effectively splits the raid across the
  two platforms.

- **`ThaddiusMoveToPlatformAction`** (`thaddius move to platform`) — fires on
  `thaddius phase transition`. Navigates bots from their add platforms up/down to
  the low central platform. Uses z-check + `JumpTo` to handle the vertical geometry.

- **`ThaddiusMovePolarityAction`** (`thaddius move polarity`) — fires on
  `thaddius phase thaddius` (ACTION_RAID+1). Checks the bot's own polarity aura
  (by id AND name fallback) and moves to one of 6 fixed spots:
  - Negative: left melee `(3508.29, -2920.12)` or left ranged `(3501.72, -2913.36)`
  - Positive: right melee `(3519.74, -2931.69)` or right ranged `(3524.32, -2936.26)`
  - No charge / MT without aggro: center melee/ranged `(3512.19/3504.68)`

- **`ThaddiusPhasePetLoseAggroTrigger`** — tank-specific; fires `taunt spell`
  (ACTION_RAID+2) when a tank bot is in pet phase but its current target's victim
  is not the bot — handles Magnetic Pull tank-swap re-aggro.

## Implications for bot AI / strategy authoring

- **Add death-sync is now DATA (was C++-only).** The "both adds must die within 5 s"
  requirement — monitor two independently-tanked targets' HP, throttle DPS on the
  ahead one — is expressed by the `target_hp_ahead` trigger (cross-target HP compare,
  located by proximity scan so it's threat-independent) + a `suppress` rule listing
  the `@damage` category token (zeros non-healing casts). That is the data form of
  `ThaddiusGenericMultiplier`'s `≤40% / ≥3%` clamp. See `raid_strategies/thaddius.json`.
  *(Built 2026-05-26; in-game tuning of `margin`/`below` still pending.)*

- **Polarity grouping is now DATA (was thought C++-only).** The pessimism here
  predated the `self_aura` trigger field. The C++ `ThaddiusMovePolarityAction` doesn't
  dynamically regroup — it sends negative bots to **fixed left** spots and positive
  bots to **fixed right** spots. That is exactly `encounter_active` (`self_aura`:
  `"negative charge"` / `"positive charge"`, gated by NAME for 10/25 safety) driving a
  per-polarity `stack_point` at those same six anchors. The 6 anchors are ported 1:1
  in `thaddius.json`. `HasAura(name)` is an exact-length match and all the polarity
  spell ids (28059/28062/28084/28085/29659/29660) are named `"Positive Charge"` /
  `"Negative Charge"`, so the name gate matches whichever one lands.

- **`find target` is threat-based; Thaddius is invisible until activation.**
  During the add phase, `find target "thaddius"` returns null (he has no threat list
  active before his phase begins). `ThaddiusBossHelper.UpdateBossAI()` uses
  `find target "thaddius"` and short-circuits if null — phase detection (`IsPhasePet`,
  `IsPhaseThaddius`) relies on Feugen/Stalagg's alive state rather than Thaddius's
  threat list. This pattern is correct. (See memory: find-target-is-threat-based.)

- **Aura detection must use NAME for 10/25 safety.** The polarity charge spells have
  different ids on 10-man vs 25-man. Always check both ids (`PositiveCharge10=28059`,
  `PositiveCharge25=28062`, etc.) **and** the name fallback (`"positive charge"`,
  `"negative charge"`). The existing C++ AI already does this correctly. Any future
  JSON encounter-active rules involving charges should gate by aura NAME.
  (See memory: raid-debuff-difficulty-ids.)

- **Human is never required for any mandatory slot.** Both add tanks are bots;
  polarity grouping positions are assigned per bot's own aura; no mechanic requires
  the human player's direct action. (See memory: human-never-required.)

- **Tesla Coil overload is passive damage, not a bot action.** Bots naturally stay
  on their platforms because `ThaddiusAttackNearestPetAction` moves them to fixed
  anchors — the 28 y leash from home should not be triggered in normal bot play.
  No special overload handler is needed.

- **The add phase splits the raid spatially** — bot tanks go to opposite platforms,
  non-tanks follow the nearer add. This is managed purely by the C++ action's
  anchor coordinates, not by formation or JSON shapes.

- **Transition geometry is complex.** The two add platforms are at z=312, the main
  Thaddius platform is at z=304. `ThaddiusMoveToPlatformAction` handles the jump
  navigation. No JSON movement shape can express vertical z-transitions or `JumpTo`
  calls — this remains C++ only.
