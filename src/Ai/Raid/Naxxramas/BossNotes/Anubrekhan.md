# Anub'Rekhan — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_anubrekhan.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Anub'Rekhan` |
| Boss entry | `15956` |
| Map | Naxxramas, `533` |
| Room center / kite orbit center | `3272.49, -3476.27, 287.07` (used in anubrekhan.json) |
| Enrage timer | 10 minutes (`SPELL_BERSERK` 26662) |

## Adds

| Name | Entry | Normal entry (1) | When spawned | Count 10 / 25 |
|------|-------|-----------------|--------------|---------------|
| `Crypt Guard` | `16573` | `29256` | Pre-pull (Reset) + each Locust Swarm | **2 pre-pull on 25-man** (positions [0]+[1]); 1 extra at 17.5s on 10-man (position [2]); 1 more at Locust Swarm +3s on all difficulties |
| `Corpse Scarab` | `16698` | `29267` | On Crypt Guard death (×10) OR on player kill (×5) | Burst from each dead Guard; also from any player killed by boss |

**Crypt Guard spawn positions** (from `cryptguardPositions[]` in script):

| Index | X | Y | Z | Facing |
|-------|---|---|---|--------|
| [0] | `3299.732` | `-3502.489` | `287.077` | `2.378` |
| [1] | `3299.086` | `-3450.929` | `287.077` | `3.999` |
| [2] | `3331.217` | `-3476.607` | `287.074` | `3.269` |

On **25-man**: Guards [0] and [1] spawn at Reset (pre-pull). Guard [2] spawns 3s after
each Locust Swarm cast (all difficulties). On **10-man**: no pre-pull Guards; Guard [2]
spawns at 17.5s into the fight, plus the Locust Swarm trigger.

Crypt Guards die → cast `SPELL_SUMMON_CORPSE_SCARABS_10` (28864) on themselves → burst
of 10 Corpse Scarabs. Player killed by boss → boss casts `SPELL_SUMMON_CORPSE_SCARABS_5`
(29105) on the corpse → burst of 5 Scarabs.

## Spells

| Spell | ID | Cast | Notes |
|-------|----|------|-------|
| Impale | `28783` | Every 15–20s from 15s | Random target. Deals massive damage + launches target into air. |
| Locust Swarm | `28785` | First at 70s, then every 90s–2min | Self-cast; boss kites the raid. Spawns another Crypt Guard 3s later. |
| Summon Corpse Scarabs (×10) | `28864` | On Crypt Guard death | 10 Scarabs burst from the corpse. |
| Summon Corpse Scarabs (×5) | `29105` | On player kill | 5 Scarabs burst from the player corpse. |
| Berserk | `26662` | 10 min | Hard enrage. |

> **Difficulty-id note:** Anub'Rekhan uses the same spell IDs on 10-man and 25-man.
> The differences are structural (Crypt Guard count at pull, not separate spell rows).
> Detecting Locust Swarm by NAME is still the safest phase gate — consistent with the
> general rule to prefer aura NAME over id for WotLK raid debuffs.

## Phase structure

This is a **single-phase fight** — no phase transition where Anub'Rekhan goes
non-attackable or non-selectable. The Locust Swarm cast is the only major state change:

- **Normal (kite off cooldown):** Tank holds boss in place. Off-tank grabs Crypt Guards.
  Ranged/healers spread for Impale. DPS priority: Crypt Guards → Corpse Scarabs → boss.
- **Locust Swarm active (~20s duration):** Boss kites himself around the room; main tank
  orbits to keep ahead of him. Everyone else stacks in the center (safe from the chasing
  swarm). A new Crypt Guard spawns 3s after Locust Swarm is cast.
- Repeats until 10-minute Berserk.

Because Anub'Rekhan is always selectable and always on the main tank's threat list,
`find target` and `encounter_active` both work normally throughout — there is no balcony
analog from Noth here.

## Current anubrekhan.json strategy summary

The live JSON at `server/raid_strategies/anubrekhan.json` implements:

- **Main tank, no swarm:** `attack` → `anub'rekhan` (priority 1)
- **Main tank, swarm active:** `orbit_point` around `3272.49, -3476.27` r=45 clockwise (priority 3)
- **Not-main-tank, swarm active:** `stack_point` at `3272.49, -3476.27` r=4 (priority 3)
- **Off-tank, no swarm:** `tank_adds` for `crypt guard` dragging to `anub'rekhan` position (priority 2)
- **Ranged + healers, no swarm:** `spread` r=8 (priority 2)
- **DPS, no swarm:** `attack` priority list `["crypt guard", "corpse scarab"]` with boss fallback `anub'rekhan` (priority 1)

**Notable gaps / improvement candidates:**

- Corpse Scarabs are named in the DPS `attack` rule but not in an explicit `tank_adds`
  rule for the off-tank — in practice the off-tank will only chase Crypt Guards. Scarabs
  are melee-only and die fast to AoE, so this is acceptable but intentional.
- No explicit rule covers the off-tank during Locust Swarm — it falls through to no
  matching rule and will likely default to generic melee behavior near the boss. A
  `stack_point` or `tank_adds` rule for the off-tank during swarm could be added if the
  off-tank is observed running after the kiting boss.
- The Locust Swarm detection uses `boss_aura: "locust swarm"` (name-based) which is
  correct per the difficulty-id gotcha.

## Implications for bot AI / strategy authoring

- **`find target` works normally here.** Anub'Rekhan never goes non-selectable or
  non-attackable. Unlike Noth's balcony phase, there is no point where the boss drops
  off everyone's threat list. `encounter_active` gating on the boss name is safe for
  the full fight duration. (memory: findtarget-is-threat-based)
- **Gate Locust Swarm phase by aura NAME, not ID.** The current JSON uses
  `boss_aura: "locust swarm"` — correct. Do not substitute spell ID 28785; aura IDs can
  differ per difficulty in WotLK even when the spell table shows one row. (memory: raid-debuff-difficulty-ids)
- **25-man pre-spawns two Crypt Guards at pull.** On 25-man (the active progression path),
  both Guards [0] and [1] are already alive and in combat before anyone attacks. The
  off-tank's `tank_adds` shape will immediately find two targets. The third Guard spawns
  3s into every Locust Swarm. Plan for up to 3 simultaneous Guards.
- **Scarab bursts are AoE events, not sustained adds.** When a Guard dies, 10 Scarabs
  erupt. They are short-lived melee mobs. The DPS `attack` rule names them so ranged
  bots drop AoE on the cluster rather than tunneling the boss — this is correct and the
  biggest reason to list them explicitly.
- **Player-kill Scarab trigger:** If any raid member dies to the boss (not Guards), 5
  extra Scarabs burst from the corpse. Bots do not need a special rule for this — the
  same DPS `attack` rule that names `"corpse scarab"` already covers them.
- **The human is never a required slot.** Main tank, off-tank, and all DPS/healer roles
  are filled by bots. The human can play any role or none. (memory: human-never-required)
- **No C++ required for this fight.** All mechanics (kite orbit, stack, spread, add
  priority) are expressible with existing `json-raid` shapes. The current JSON is
  functionally complete; the gaps noted above are polish items only.
