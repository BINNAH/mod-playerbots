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

## Existing C++ AI (still owns the in-combat fight)

A `json-raid` strategy now exists (`raid_strategies/thaddius.json`) and owns the
**engagement** (autonomous ramp climb + role-based left/right split — see implications
below). The C++ below still owns everything **in combat** (on-platform positioning,
Magnetic Pull swaps, the transition jump, polarity). Existing C++ AI does:

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

- **Engagement is now autonomous + role-split (JSON).** At `.rjson pull`, each bot
  paths up the ramp **on its own** via `move_to_target` (`detect:nearest`) toward its
  assigned add and engages it — no human leading required. The split: main tank +
  ranged + healers → Stalagg (left); off-tank + **melee DPS** → Feugen (right). **The
  melee rule MUST be `role:"melee,!tank,!healer"`, not `"melee"`** — `IsMelee` is just
  `!IsRanged`, so a melee tank reads as `melee` and a bare rule made the **main tank**
  match both `maintank`(→Stalagg) and `melee`(→Feugen), ping-ponging it through center
  (the up/down-to-center oscillation; only visible once the climb itself worked). The
  instant combat starts, the C++ `thaddius attack nearest pet` takes over on-platform
  positioning. **Healers are split across both adds** via `manual_engage`'s `split`
  field (`role:"healer", split:"1/2"`→Stalagg, `split:"2/2"`→Feugen) so the off-tank
  side isn't left unhealed; ranged DPS (`ranged,!healer`) go left, melee DPS
  (`melee,!tank,!healer`) right. The split counts bots only (the human isn't counted),
  and the in-combat C++ then holds each healer at its nearest add's ranged anchor, so
  the side assignment persists into the fight.

- **Tanks must not taunt off each other on the Magnetic Pull swap.** The C++
  lose-aggro rule (`thaddius phase pet lose aggro` → `taunt spell`) fires for *any*
  tank whose add isn't hitting it. After a swap, the MT (pulled toward Feugen, its
  target set to the nearest add by the C++) taunted Feugen off the OT. The boss's
  threat-swap already re-assigns the tanks to the opposite adds, so that taunt is
  redundant *and* steals. Fix (JSON, no upstream edit): a `suppress` using the
  `target_victim` trigger zeroes `taunt spell` whenever the bot's current target is
  already being tanked by another **bot tank** — so neither tank taunts off the other.
  The taunt still fires to recover an add from a DPS/healer (victim isn't a tank). See
  `thaddius.json` suppress + the `target_victim` shape.

- **The pull hands off to the C++ via the `move_to_target` reached-latch (not a
  combat gate).** Once a bot gets within `distance` of its add, `move_to_target`
  latches and yields every tick, so the proven C++ owns positioning. This fixed three
  things the logs exposed in sequence: (1) healers ran `move_to_target` ~400×/fight
  chasing their *moving* add and almost never healed (the +3 movement starved the heal
  rotation); (2) after a **Magnetic Pull** swap, a tank's pull rule (`target` = its
  *original* add) dragged it back instead of letting `thaddius attack nearest pet`
  tank the add it was thrown onto; (3) — the trap of the *first* fix — gating the
  hand-off on `IsInCombat()` made an off-tank that got flagged in combat early (taunt
  / Static Field AoE) while still on the floor hand off to the C++ *before climbing*;
  the C++ `MoveTo` can't climb, so it stalled at the bottom (the **Magicguyman** case,
  z≈295). Latching on *reached* keeps a bot climbing until it's actually up, then hands
  off. Re-arms only when out of combat AND far (wipe / fresh pull), so it survives
  Magnetic Pull (which legitimately throws a tank far from its named add — it then
  tanks the nearest add via C++). **Pull rules carry no `attack` action** — the C++
  `thaddius attack nearest pet` engages the nearest add on arrival and owns targeting
  (so swaps work); a fixed `attack` would re-introduce the drag-back. No `.rjson stop`
  required.

- **Pull rules MUST set `boss:""` or they hijack the polarity phase (2026-05-27).**
  `manual_engage` stays active the whole encounter (the climb fix), and `move_to_target`
  defaults its no-target fallback to the *file boss* (`boss=thaddius`). So once both adds
  die, the pull rules don't go quiet — `detect:nearest` finds no add, falls back to
  Thaddius, and keeps walking every bot toward the boss through the **transition** and
  **polarity** phases. That fought the per-charge `stack_point` (both are ACTION_RAID+3,
  i.e. relevance 63) in an equal-priority tug-of-war: a bot parks on its charge spot,
  `stack_point` yields (hold=0), the pull moves it back toward Thaddius, repeat — the
  "**people get movement-hijacked and can't hold the pulse spot, run back and forth**"
  bug. Downstream symptoms in the log: `flee` firing (cross-charge damage from failed
  separation) and bots dropping combat → idle `apply oil` / `add all loot` / `clean
  quest log` mid-fight. **Fix (data-only, no rebuild):** every `move_to_target` pull rule
  carries `"boss": ""`, so when its add dies it finds no target and **yields** — the pull
  is purely a one-add climb and is gone before the first Polarity Shift. Generic dps/heal
  assist (not suppressed in `thaddius phase thaddius`) covers boss damage between shifts.
  Diagnosed from `Playerbots.log`: Maryjane (resto) pushed her pull `move_to_target→feugen`
  and her positive-ranged `json stack::3524.32,-2936.26` at the **same** relevance 63.

- **Magnetic Pull FLINGS tanks off their platform; the swap needs an exact-waypoint
  re-climb (`then`, 2026-05-27).** The boss script (`boss_thaddius.cpp:530-537`) cleanly
  *transfers threat* on a swap — MT loses Stalagg / becomes top on Feugen, OT vice-versa —
  AND `CastSpell(tank, SPELL_MAGNETIC_PULL)` physically yanks each tank toward the other
  add. The yank lands them **off the platform**: a DK off-tank (Gheed) was logged at
  **z≈338**, ~83y from his target, *above* Stalagg. The in-combat C++
  `ThaddiusAttackNearestPetAction` moves with `MoveTo(target,0,…)` (exact_waypoint=**false**)
  — the same Z-snap that can't climb cold — so the flung tank is **stranded out of taunt
  range**: in one swap window Gheed cast `dark command` (DK taunt) **0× OK / 54× IMPOSSIBLE**,
  landed **1** melee strike, and just looped `thaddius attack nearest pet` + `reach melee`
  (chasing). His add ran loose to whoever still had threat → the MT ended up holding **both**
  (the "Luucious has aggro on both, mob runs to the other platform" report). **Fix:** the
  two tank pull rules get `then:["stalagg","feugen"]`. After the first reach, `move_to_target`
  re-paths to the **nearest** of `then` (the add it was thrown ONTO) and the reached-latch
  **re-arms in combat** when flung > `distance`+40 away, so it **re-climbs via exact_waypoint**,
  reaches, and hands back to the C++ + the tank's own taunt (now in range). The named `target`
  still drives the *first* climb so the MT/OT split holds; both latches reset on a real wipe.
  Only the **tanks** carry `then` — healers/ranged/melee aren't pulled, so they keep the
  permanent latch. (C++ change in `JsonMoveToTargetAction` + loader — needs a rebuild.)

- **No in-fight taunts (after the first 5s, 2026-05-28).** Even with `then` recovering
  the swapped OT's position, paladin taunts have *range*: Hand of Reckoning is 30y to the
  add, **Righteous Defense is 40y to a friendly** and AoE-taunts that friendly's attackers
  — so the MT (Luucious), flung to Feugen, casts Righteous Defense on Gheed (the OT, now
  near Stalagg) and **taunts Stalagg right back across the platform** before Gheed can
  lock it. "MT has aggro on both, the add runs to the other platform" follows. Since the
  boss script *already* transfers threat on Magnetic Pull cleanly (no re-taunt needed),
  the surgical fix is to disable every class taunt during the pet phase. A new generic
  `after_ms` field on `suppress` entries delays activation until the trigger has been
  continuously active for N ms — `{ trigger:"thaddius phase pet", after_ms:5000, actions:
  ["taunt","taunt spell","dark command","hand of reckoning","righteous defense","growl"] }`
  leaves a 5s opening window for the *initial* tank pickup, then zeroes every taunt for
  the rest of the add phase. Earlier `target_victim role:"tank"` attempt was wrong twice:
  (a) it only named `"taunt spell"`, which paladin/DK strategies alias *away* from to
  their own class action — so it was inert against the actual cross-taunt, and (b) it
  also blocked the OT's needed re-taunt when an add slips. Replacing it. (C++ change in
  the suppress multiplier — needs a rebuild.)

- **Polarity-phase "flailing" is sub-stack_point movement competitors (2026-05-28).**
  Stack_point is rel 63 (priority +3) and *yields once parked* within 3y so the bot's
  rotation runs — but every other movement action in the queue is < 63, so the *yield*
  was being grabbed and the bot shoved out of the radius, re-engaging stack_point in a
  bounce loop. Confirmed in the log: per polarity attempt, melee bots ran **`set behind`
  50–125× each** (re-anchoring behind the boss instead of staying on the side polarity
  anchor) — Talason 125, Chadw 111, Magicguyman 69, Mech 53 — and aggregate **`flee` 83×**
  (cross-charge damage flees the bot OUT of its same-polarity stack, prolonging the damage
  instead of escaping it), **`tank face` 87×**. **Fix (data-only):** the
  `thaddius phase thaddius` suppress now zeros `set behind`, `tank face`, `reach melee`,
  `reach spell`, `autopilot move`, `flee` (in addition to the pre-existing `follow` /
  `combat formation move`). `dps assist` (rel 50) is *kept* so bots still target Thaddius
  between Polarity Shifts, and `avoid aoe` is kept so they dodge Ball Lightning ground
  marks. Once parked, the only thing left in the queue is the rotation casts — which
  don't move the bot. The polarity stack should now hold cleanly. Anchor positions are
  geometrically fine (left↔right melee ≈16y, left↔right ranged ≈32y, both > the 10y
  cross-charge damage radius); the bug was never the anchors, it was the yield window.

- **The low→high ramp climb needs an EXACT-waypoint `MoveTo` (preserve the
  destination z).** This bit us repeatedly, and *neither* a fixed-point `stack_point`
  *nor* a "move toward the live add" approach fixes it on its own — both still route
  through the buggy path. Root cause: `MovementAction::MoveTo(mapId,x,y,z,…)` with the
  default (non-exact, path-generating) flags runs `SearchForBestPath`
  (`MovementActions.cpp`), which **discards your z** and re-snaps the destination to
  whichever nearby surface yields the **shortest** navmesh path. From the floor, the
  slime *directly under* the platform is a far shorter path than the ramp, so it wins
  — the z is thrown away. **Confirmed from `[RaidJson][move_to_target]` logs (2026-05-27):**
  at pull start the adds were correctly up at **z≈312.1** while bots staged at
  **z≈295.6** ~85 y away; the bots then walked across the slime to the adds' x,y but
  **stayed at z≈292**, eventually aggroed, and the adds **ran down** to them. (An
  earlier note here claimed "a direct MoveTo to the platform TOP just works" — that
  was the **misdiagnosis**; the in-combat C++ only *looks* like it climbs because by
  then the bots are already at z≈312, so the floor is no longer the shortest path.)
  **Fix:** the `move_to_target` shape calls `MoveTo(mapId, add.x, add.y, add.z,
  false,false,false, exact_waypoint=TRUE, …)` — exact_waypoint skips the snap and
  keeps the add's literal z, so recast routes **up the ramp** to the platform poly
  (generatePath stays on → a real navmesh path, no straight-line clip). Only the
  **vertical jump** (the z312→z304 transition drop) still needs C++ `JumpTo`.
  `move_to_target` logs a throttled `[RaidJson][move_to_target]` line per bot — watch
  the bot's z **rise toward ~312** during the climb (the success tell).

- **The add phase splits the raid spatially in combat** — bot tanks hold opposite
  platforms, non-tanks hold the nearer add's anchor. This is managed by the C++
  action's anchor coordinates; the JSON engage rules above just deposit each bot on
  the correct platform first.

- **Transition geometry is complex.** The two add platforms are at z=312, the main
  Thaddius platform is at z=304. `ThaddiusMoveToPlatformAction` handles the jump
  navigation. No JSON movement shape can express vertical z-transitions or `JumpTo`
  calls — this remains C++ only.
