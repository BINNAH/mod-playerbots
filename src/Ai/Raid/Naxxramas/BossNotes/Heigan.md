# Heigan the Unclean — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_heigan.cpp`, the instance script
`instance_naxxramas.cpp` (eruption geometry), `naxxramas.h`, and the existing
playerbot C++ strategy (`Naxxramas/Action/RaidNaxxActions_Heigan.cpp` +
`Util/RaidNaxxBossHelper.h`). Verified 2026-05-26. **Verify IDs against the live
DB/source before trusting** (creature_template can drift; raid debuffs sometimes
have per-difficulty spell ids — see note below).

## TL;DR — the dance IS Level-2 (shaped 2026-05-26)

The eruption dance was *thought* bespoke, but it decomposes cleanly into data — a
set of safe-zone coords, a walk pattern over them, and a per-phase cadence — so it
became the generic **`timed_safe_zone`** shape (`JsonStrategy/JsonStrategyActions`,
the parameterized form of `HeiganDanceAction`). `heigan.json` now expresses the
dance as **two instances of that one shape** differing only in cadence (slow:
15s/10s; fast: 7s/4s), sharing the 4 zone centers + the `3,2,1,0,1,2` walk. Two
things stay **Level-1** (referenced C++ by name) on purpose:
- the **triggers** (`heigan fast dance` / `heigan slow dance ranged/platform`) —
  proven phase detection (`HeiganIsFastDancing`, boss auras) + exact role split;
- the **platform** action (`heigan platform`) for slow-phase tank+melee — its
  final-8s bleed-off onto the dance is genuinely Heigan-specific timing glue, not
  a generic shape.

So WHO/WHEN is proven C++; the dance *behavior* (the part once called bespoke) is
now reusable data. The docs (`RAID_AI_PATTERNS.md` A6, `RAID_AI_JSON_AUTHORING.md`
hard limits) were updated to match. A *reactive/random* safe zone (Sapphiron ice
blocks) still belongs in C++; a *fixed-pattern* one like Heigan does not.

**Cast-while-moving (added after first kill).** Both dance rules set
`cast_while_moving: true`: while *en route* the shape yields the tick so each bot's
own rotation fires INSTANTS as it relocates. Safe because the engine refuses
cast-time spells while moving (`PlayerbotAI::CanCastSpell`), so only instants come
out — no rooting into an eruption. Parked-fast still holds (`hold:true`, no cast),
fine since the fast phase is mostly spent moving and the boss isn't worth hitting.

It depends on **two phase-split `suppress` rules** (the data-driven
`HeiganDanceMultiplier`):
- **Fast** (`heigan fast dance`): zero `avoid aoe`, `reach spell`, `reach melee`,
  `combat formation move`, `flee`, **`heigan dance`**.
- **Slow** (`plague cloud` absent): same **minus `reach melee`** (platform melee
  must close on the tanked boss).

Two non-obvious entries, both learned from `Playerbots.log`:
1. **`heigan dance` MUST be suppressed.** `.rjson on` strips the C++ `naxx`
   strategy, but `ApplyInstanceStrategies` re-adds it on zone-in, so the C++
   `heigan dance` action (@ `ACTION_RAID+3`) is still in the queue. When json
   safezone yields, it catches the tick and *holds* → rotation never runs → no
   instants (the bug that "made it worse"). The holding version masked this by
   winning at 92; the yielding version doesn't.
2. **`reach melee` only in fast.** Heigan is teleported up in the fast phase but
   stays targetable, so a melee bot freed by the yield would chase him off the
   dance floor. Suppress it there; allow it in slow so platform melee can close.

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Heigan the Unclean` |
| Boss entry | `NPC_HEIGAN = 15936` |
| Map | Naxxramas, `533` (Plague Quarter) |
| Boss home / eruption fan origin | `HeiganPos = (2796, -3707)` (z ≈ 274.9) |
| Eruption sections | `4` (`HeiganEruptSectionCount`); wedges fan out from `HeiganPos` |

## Spells

| Spell | ID | Phase | Notes |
|-------|----|-------|-------|
| Spell Disruption | `29310` | Slow | Self-cast every ~10–15s. Interrupts/pushes back casters near him — keep ranged out of his melee bubble. |
| Decrepit Fever | `29998` | Slow | Every ~22–25s. Disease: damage + reduced max HP. Cleanse/decurse. |
| Plague Cloud | `29350` | **Fast** | Channeled the whole ~45s fast phase; room-wide raid damage. Its **aura on the boss is one of the fast-phase tells.** |
| Teleport Self | `30211` | Fast (entry) | Applied when he jumps to the upper platform at fast-phase start; **the other fast-phase tell.** |
| Eruption | `29371` (`Eruption10`) | both | Floor tiles erupt one **section** at a time, walking back and forth. Cast by the eruption GOs, not the boss. Per-difficulty id (the constant is named `Eruption10`) — **don't gate on this id.** |

## Phase structure (from `StartFightPhase`)

The fight alternates **slow → fast → slow → …** until he dies. Section index resets
to `3` on every phase entry, so both phases share the same walk pattern.

- **Slow dance (90s):** Heigan tanked on the ground platform, `REACT_AGGRESSIVE`,
  attackable.
  - Spell Disruption ~10–15s, Decrepit Fever ~22–25s.
  - **Eruption: first at +15s, repeats every 10s.** Section walks `3→2→1→0→1→2→3…`.
  - At +90s → fast dance.
- **Fast dance (~45s):** he `CastSpell(TeleportSelf)` to the upper platform,
  `AttackStop()` + `REACT_PASSIVE` → **non-attackable and OFF EVERYONE'S THREAT**,
  then channels Plague Cloud.
  - **Eruption: first at +7s, repeats every 4s** — same section walk, much tighter
    cadence (little slack to reposition; healers/casters can't afford a cast-time
    spell mid-hop).
  - At +45s → back to slow dance.

> **Difficulty-id note:** the fast-phase tells used by the bot AI — Plague Cloud
> (`29350`) and Teleport Self (`30211`) — fire on both 10 and 25; the C++ detection
> (`HeiganIsFastDancing`) works on the user's active **25-man** path. Eruption
> (`29371`) is per-difficulty, but nothing detects it by id (the dance is predicted
> from the *clock*, not the eruption aura), so it doesn't matter. Keep gating phases
> by **aura NAME** as a habit anyway. (See memory: raid-debuff-difficulty-ids.)

## The dance — safe sections & geometry

`instance_naxxramas.cpp::GetEruptionSection` maps a world (x,y) to a section by the
slope from `HeiganPos`. The bot AI mirrors the boss-script schedule with a pure
clock: a triangle wave `kPattern = {3, 2, 1, 0, 1, 2}` (period 6) gives the safe
section for the K-th eruption, and `ComputeSafeSectionAt(phase_start, now, fast)`
picks K from the per-phase first-delay + interval. Safe-spot centers
(`RaidNaxxActions_Heigan.cpp`):

| Section | x | y | z |
|---------|------|------|------|
| 0 (far SW strip) | `2756.0` | `-3704.0` | `276.54` |
| 1 | `2762.3` | `-3684.6` | `276.54` |
| 2 | `2775.5` | `-3674.4` | `276.54` |
| 3 (nearest spawn corner) | `2794.9` | `-3668.1` | `276.54` |

| Other anchor | x | y | z |
|--------------|------|------|------|
| Slow-phase melee/tank platform | `2795.52` | `-3705.23` | `274.88` (tol 3.0) |

The platform sits ~5.4y off `HeiganPos`, clear of the eruption wedges and outside
the Spell Disruption bubble — so the tank holds Heigan there through the whole slow
phase with no dancing, and melee stack on the tank.

## How the C++ strategy plays it (what the JSON reproduces)

Three rules in `RaidNaxxStrategy.cpp` + one multiplier:

| Trigger (C++ name) | Who | Action | C++ relevance |
|--------------------|-----|--------|----------------|
| `heigan fast dance` | everyone (fast phase) | `heigan dance` | `ACTION_RAID + 3` (63) |
| `heigan slow dance platform` | tank + melee (slow) | `heigan platform` | `ACTION_RAID + 2` (62) |
| `heigan slow dance ranged` | ranged (slow) | `heigan dance` | `ACTION_RAID + 2` (62) |

- **`heigan dance`**: predict safe section, move there. *Holds the tick* (returns
  true) while en route and during the whole fast phase; in the **slow** phase, once
  on the safe wedge it returns false so DPS/heal rotations run between eruptions.
- **`heigan platform`**: tank/melee park on the platform (returns false once parked
  so they melee/threat normally). In the final 8s of the slow phase non-tanks bleed
  off early onto the slow dance so the fast cadence doesn't catch them sprinting.
- **`HeiganDanceMultiplier`**: zeroes the competing movement — `CombatFormationMove`,
  `CastDisengage`, `CastBlinkBack`, `Flee`, **`AvoidAoe`** — and suppresses
  cast-time heals during the fast phase. This is what lets the low-relevance dance
  win; **see the JSON gotcha below.**
- `heigan follow master` action exists/registered but is **not wired** into the C++
  strategy (bots predict the dance themselves, which is better than copying the
  human). Available as a Level-1 fallback if ever wanted.

## JSON port — the multiplier gotcha (the key finding)

`JsonRaidStrategy` only implements `InitTriggers` — **it carries no multipliers.**
So a naïve Level-1 port that wires `heigan dance` at the C++ relevance (`+3` = 63)
is **broken**: generic `"avoid aoe"` is registered at `ACTION_EMERGENCY = 90`
(`Base/Strategy/CombatStrategy.cpp`), so when an eruption lands near a bot, avoid-aoe
(90) beats the dance (63) and the bot flees to a random, possibly-erupting wedge
instead of the predicted-safe one — exactly the case `HeiganDanceMultiplier` zeroes
out in C++.

**Fix (pure JSON, no build):** wire the dance/platform actions at a **priority high
enough to outrank `avoid aoe`** — the loader does not clamp `priority`, and it's an
offset on `ACTION_RAID` (60), so `priority: 31` → relevance 91 > 90. Because the
dance/platform actions already *hold the tick* (return true) while they need to own
movement and *return false* once safely parked in the slow phase, winning the
relevance race is sufficient to replace the multiplier — no per-action suppression
list needed. Heal suppression in the fast phase falls out for free (the dance holds
every fast-phase tick, so no cast ever starts). `heigan.json` uses `+32` for fast
dance and `+31` for the two slow-phase rules.

> This is the one place the JSON deviates from C++: priority-based suppression of
> *everything* ≤90 (during the windows the dance holds the tick) rather than the
> multiplier's targeted five-action list. It's equivalent-or-safer for Heigan (you
> want bots glued to the dance), but if some genuinely-critical emergency action
> ever needs to punch through mid-dance, that's the trade-off to revisit — or add
> multiplier support to `JsonRaidStrategy` (a framework change + build).

## Implications for bot AI / strategy authoring

- **The dance is now a reusable Level-2 shape** (`timed_safe_zone`, pattern A6).
  Geometry + walk pattern + cadence are data; the same shape should drive any other
  fixed-pattern timed safe zone. The only Heigan-specific bits left in C++ are the
  platform bleed-off glue and the proven phase/role triggers (both Level-1 here).
- **The human is never required.** Every bot predicts and dances independently;
  no rule depends on the human filling a slot. (memory: human-never-required.)
- **Fast phase breaks `find target` on the boss** (he goes non-selectable / off
  threat, like Noth's balcony) — but the C++ dance trigger detects fast-dancing by
  the **TeleportSelf / Plague Cloud auras on the boss** (and in-flight casts of
  either), not by threat, so detection survives the phase. This is handled inside
  the referenced C++ trigger; the JSON just wires it.
