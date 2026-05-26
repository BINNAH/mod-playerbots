# Grobbulus — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_grobbulus.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Grobbulus` |
| Boss entry | `15931` |
| Map | Naxxramas, `533` |
| Room center / kite orbit anchor | `3281.23, -3310.38` (z ≈ 293; derived from `GrobbulusRotateAction` + `GrobbulusMoveCenterAction`) |
| Pre-combat patrol anchor | `3178, -3305, 319` (passive patrol; casts `BOMBARD_SLIME` 28280 at `3129, -3313, 293` area) |
| Leash | standard BossAI evade |

## Adds

| Name | Entry | Source | Notes |
|------|-------|--------|-------|
| `Fallout Slime` | `16290` | Slime Spray hit target | Spawned at the position of each Slime Spray hit unit (`spell_grobbulus_slime_spray::HandleHit`). Immediately `SetInCombatWithZone`. No 10/25 count difference in script — one per Spray hit. |
| `Grobbulus Cloud` | `16363` | Poison Cloud cast | Not a "true add" — it is an invisible NPC placed at Grobbulus's feet every 15 s; grows its `COMBATREACH` from 2 y → ~17 y over 60 s (0.00025/ms). Its aura (`28158`) is the lingering hazard. Also has a `(1)` duplicate entry `29379` — confirm active difficulty uses `16363`. |

> **Chamber adds on engage:** `NPC_STICHED_GIANT` (entry `16025`) adds within 300 y of
> Grobbulus are pulled into combat with the first aggro target by `PullChamberAdds`. They
> are ambient guards in the Construct Quarter, not summoned by the boss itself.

## Spells

| Spell | ID | Timer / interval | Notes |
|-------|----|-----------------|-------|
| Poison Cloud | `28240` | First 15 s, repeat 15 s | Cast on self; spawns a `Grobbulus Cloud` NPC at boss position. Cloud grows to ~17 y radius over 60 s. This is the PRIMARY kite pressure — the tank must step away before/after each cast. |
| Mutating Injection | `28169` | First 20 s; repeat `6 000 + 120 × healthPct` ms (≈ 18 s at 100%, ≈ 6 s near death) | Random non-MT player. A dummy aura; on EXPIRE or ENEMY_SPELL removal it triggers Mutating Explosion (`28206`) at the victim's position, spawning a new cloud. **Injected player must run to open ground before the aura expires.** |
| Mutating Explosion | `28206` | Triggered by injection removal | The AoE detonation that actually places the cloud; handled in `spell_grobbulus_mutating_injection_aura::HandleRemove`. |
| Slime Spray | `28157` | First 10 s, repeat 20 s | Frontal cone on current victim. Each hit unit spawns one `Fallout Slime` (entry `16290`) at that unit's position. Emote fires before cast. Tank must face boss away from raid. |
| Poison Cloud Damage Aura | `28158` | Continuous on each Cloud NPC | Applied 1 s after spawn (delayed for visibility). This is the aura to detect on the ground-NPC, not on Grobbulus. |
| Berserk | `26662` | **10-man: 720 s (12 min); ★25-man: 540 s (9 min)** | Hard enrage. |
| Bombard Slime | `28280` | Every 5 s out of combat | Pre-combat patrol cast; targets a ground point around `3129, -3313, 293`. Irrelevant once engaged. |

> **Difficulty-id note:** Grobbulus scales enrage timer by *value* (`RAID_MODE(720s, 540s)`),
> **not** by separate per-difficulty spell rows — the same spell IDs fire on 10 and 25.
> The Poison Cloud and Mutating Injection IDs are safe to detect by id or aura NAME on the
> active 25-man path. The Grobbulus Cloud NPC (`16363` vs `29379`) may differ by difficulty;
> gate cloud-avoidance by aura NAME (`"poison cloud"` or matching spell `28158`) rather than
> by creature entry. If unsure, verify active-difficulty NPC entry against DB at runtime.

## Phase structure

Grobbulus is a **single-phase, no-transition fight** — there are no balcony phases,
threat resets, or movement scripted by the boss itself. The complexity is entirely
**positional** and **player-driven**:

- **Continuous:** Tank kites Grobbulus in a slow clockwise arc around the room perimeter
  to keep deposited Poison Clouds (placed every 15 s) behind the boss's path so they
  do not block future movement. The room center anchor is `3281.23, -3310.38`; the C++
  AI uses a radius-35 orbit of 8 waypoints.
- **Every 15 s:** Poison Cloud placed at boss's feet → tank must have already moved
  (or move immediately) to clear the spot for the next arc segment.
- **Every ~18–20 s (slows near death):** Mutating Injection on a random non-MT player →
  that player runs to the perimeter/open area, waits for the ~10–15 s aura to expire,
  then returns. At expiry the cloud is placed at their position.
- **Every 20 s:** Slime Spray frontal cone → tank keeps boss faced away from raid.
  Each hit spawns a Fallout Slime add at the struck target's location.
- **9 min (25-man):** Berserk — enrage wipe if boss not dead.

## Existing C++ AI — no JSON strategy yet

No JSON strategy yet; existing C++ AI does:

- **`GrobbulusRotateAction`** (main tank only, has-aggro gate): `RotateAroundTheCenterPointAction`
  around `3281.23, -3310.38`, radius 35 y, 8 waypoints, clockwise. Fires when
  `GrobbulusCloudTrigger` is active (see below). This is the kite step.
- **`GrobbulusCloudTrigger`** (main tank only): activates when Grobbulus is casting
  Poison Cloud (`28240`) OR when 15 s have elapsed since the last cloud (`CloudRotationDelayMs = 15000`).
  The trigger polls `find target "grobbulus"` — gate goes silent if the tank loses
  threat (threat-based lookup).
- **`GrobbulusMoveAwayAction`** (melee with Mutating Injection): moves the bot to ≥18 y
  from boss when it has the `mutating injection` aura, regardless of direction. No
  cloud-avoidance path logic — it just opens distance.
- **`GrobbulusGoBehindAction`** (ranged with Mutating Injection): moves to 24 y behind
  the boss (orientation + π + π/8 offset) — runs injected ranged away from the raid
  stack by going around the back.
- **`GrobbulusMoveCenterAction`** (ranged, after injection removed): moves back toward
  `3281.23, -3310.38` within 5 y — re-stacks the ranged group after the cloud has been
  dropped.
- **`GrobbulusMultiplier`**: suppresses `AvoidAoeAction` for the main tank (tank must
  NOT flee Poison Cloud — it needs to keep boss position consistent) and suppresses
  `CombatFormationMoveAction` for all bots (no formation-follow during Grobbulus).
- **Triggers wired in `RaidNaxxTriggerContext.h`**: `mutating_injection_melee`,
  `mutating_injection_ranged`, `mutating_injection_removed`, `grobbulus_cloud`.
- **No tank-add / Fallout Slime handling** in the existing C++ AI — Slimes are
  treated as generic combat targets (they `SetInCombatWithZone` on spawn).

## Implications for bot AI / strategy authoring

- **Tank kite path is a PARTIAL HARD LIMIT.** The orbit mechanic (`GrobbulusRotateAction`,
  radius 35, 8 waypoints) cannot be reproduced in the current json-raid data layer —
  it requires a timed positional step every 15 s driven by `GrobbulusCloudTrigger`.
  The json layer can express `orbit_point` but has no trigger wiring to pace it by the
  cloud cast. Authoring a JSON strategy today would need to coexist with the C++ kite
  logic, not replace it. Treat kite-step as C++ territory.
- **Mutating Injection spread IS data-expressible in principle.** The move-away
  distance (18 y melee, 24 y ranged behind) and the return-to-center logic map onto
  `move_away`/`spread` + `attack` shapes — but the trigger must gate on the AURA NAME
  `"mutating injection"` (id `28169`), not on encounter state, because there is no
  phase clock. The existing C++ implementation already handles this correctly and is
  the path of least resistance.
- **Slime Spray Fallout Slime adds are data-expressible** with `tank_adds` on entry
  `16290`. They `SetInCombatWithZone` immediately so they appear in the "attackers"
  list; an assist-tank action can gather and cleave them. However, Slimes spawn at
  the RAID's position (Spray hits melee), so the off-tank pickup is straightforward —
  just list `fallout slime` in a `tank_adds` shape. Mark the slot bot-only (human
  never required).
- **`find target "grobbulus"` is threat-based.** All four triggers (melee/ranged
  injection, injection removed, cloud) gate on `find target "grobbulus"` returning
  non-null. If Grobbulus de-aggroes the bot (e.g. tank dies), all Grobbulus-specific
  triggers go silent. Design any JSON trigger to also gate by aura NAME rather than
  boss-presence alone when possible.
- **Aura detection: use NAME not base ID.** The Poison Cloud damage aura (`28158`) is
  on the Cloud NPC, not Grobbulus. Mutating Injection (`28169`) is on the player.
  Both are single-id (no per-difficulty splits confirmed), but per the standard rule
  use aura NAME in playerbot AI (see memory: raid-debuff-difficulty-ids).
- **No phase transitions to detect** — this is a pure single-phase fight. The only
  time-varying element is the injection re-cast timer slowing at low health (more
  overlap of cloud placement late in the fight). No phase clock required in JSON.
- **Berserk: ★25-man is 9 min (540 s), 10-man is 12 min (720 s).** DPS check is
  tighter on 25-man. Worth noting for any future encounter-timer annotation.
- **Human never required** in any mandatory role — tank, injection runner, add pickup
  are all coverable by bots. (See memory: human-never-required.)
