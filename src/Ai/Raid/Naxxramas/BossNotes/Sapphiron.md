# Sapphiron — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_sapphiron.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Sapphiron` |
| Boss entry (10-man) | `15989` |
| Boss entry (25-man) | `29991` |
| Blizzard NPC (10-man) | `16474` |
| Blizzard NPC (25-man) | `30000` |
| Map | Naxxramas, `533` |
| Boss spawn / home / flight hover point | `3522.39, -5236.78, 137.71` (o `4.50`) — guid `133932` |
| Ground-phase tank anchor (C++ helper) | `3518.64, -5252.45` |
| Room center (C++ helper) | `3517.31, -5253.74` |
| Leash | evades if `>100y` from `3523.5, -5235.3, 137.6` |

> **Active progression is 25-man (difficulty 1) — prioritize/mark 25-man values.**

## Adds

No permanent adds. The **Blizzard NPC** (`16474` / **`30000`** on 25-man) is a
temporary creature summoned once per ground-phase timer cycle (~8 s / **~6.5 s**);
it moves randomly within 40y of its spawn point and deals frost damage while alive
(~16 s). It is not a proper "add" — it has no threat list and cannot be taunted.

## Spells

| Spell | ID | Phase | Notes |
|-------|----|-------|-------|
| Frost Aura | `28531` | Ground (always active) | Passive raid-wide frost DoT; ticks entire fight. Frost resist gear mitigates. |
| Cleave | `19983` | Ground, every 10 s | Front-arc physical; tank must face boss away from raid. |
| Tail Sweep | `55697` | Ground, every 10 s | Rear-arc physical; melee must NOT stand directly behind boss. |
| Life Drain | `28542` | Ground, every 24 s | Hits 2 random targets (10-man) / **5 random targets (25-man)**. Healer throughput check. |
| Summon Blizzard | `28560` | Ground, every 8 s / **6.5 s** | Spawns Blizzard NPC on a random player within 40y. NPC moves randomly ~16 s. |
| Berserk | `26662` | 15 min enrage | Soft enrage. |
| IceBolt (cast trigger) | `28526` | **Flight only** | Fired at each icebolt target; triggers `28522` (10-man aura) on the victim. |
| IceBolt (freeze aura) | `28522` / `28526` | **Flight only** | Encases the target in an ice block (also spawns GO `181247`). Bot AI tracks via aura name "icebolt". 2 targets (10-man) / **3 targets (25-man)**. |
| Frost Missile ("Frost Breath") | `30101` | **Flight** — after all icebolts land | Cast at self; visual + start of the "deep breath" sequence. |
| Frost Explosion | `28524` | **Flight** — 8.5 s after Frost Missile | AoE burst on every player NOT in LoS of an ice block. The lethal mechanic. |
| Chill (10-man) | `28547` | Flight / see note | Frost damage-over-time from Blizzard NPC or chill ground hazard. |
| Chill (25-man) | `55699` | Flight / see note | **25-man version.** Per-difficulty IDs — gate by aura NAME "chill". |

> **Difficulty-id note:** Sapphiron uses `RAID_MODE` for *count* differences
> (Life Drain max targets, icebolt count, Blizzard repeat timer), **not** for
> separate per-difficulty spell rows on most abilities. However, **Icebolt** and
> **Chill** have confirmed per-difficulty ids in the bot AI's `RaidNaxxSpellIds.h`
> (`Icebolt10=28522`, `Icebolt25=28526`; `Chill10=28547`, `Chill25=55699`).
> Always gate detection of these by **aura NAME** ("icebolt", "chill") so the
> correct id is found on both 10 and **25-man (the active path)**.

## Phase structure

- **Ground phase (repeating):** Tank-and-spank baseline. Frost Aura ticks the
  whole raid; keep a paladin on Frost Resistance Aura. Cleave/Tail Sweep every
  10 s — tank faces boss away from raid, melee stay on side flanks only. Life
  Drain hits 5 random players on 25-man every 24 s. A Blizzard NPC spawns every
  ~6.5 s and roams — entire raid must move away from it. At 45 s intervals
  (first flight at 45 s, then repeating, skipped below 11% HP) Sapphiron
  transitions to the Flight phase.
  - **Pre-liftoff window:** Boss goes `REACT_PASSIVE`, stops attacking, and glides
    to its home/hover point. This is ~1–2 s of passive ground time before actual
    liftoff. Non-tanks should already be spreading to the outer ring so icebolt
    targets land spread out.

- **Flight phase (per cycle):**
  1. Boss lifts off, stops melee.
  2. **IceBolt x2 (10-man) / x3 (25-man):** each target is frozen into an Ice
     Block game object at their current position. Targets are chosen randomly from
     the threat list, excluding already-frozen players. Icebolt travel time is
     distance-based (boss computes `distance/13.0` seconds per bolt).
  3. **Frost Missile → Frost Explosion (8.5 s later):** Frost Explosion is an AoE
     centered on the boss that hits every player who does **not** have LoS to an
     Ice Block (`IsInBetween` + within 10y of a block). Players in LoS behind a
     block survive; everyone else takes massive frost damage. The LoS check
     (`IsValidExplosionTarget`) is applied as a target filter in a SpellScript.
  4. **Land:** Ice block auras are removed, GOs despawn, boss resumes REACT_AGGRESSIVE.

- Flight repeats every 45 s of ground time (delayed by 35 s while flight events
  run). No hard phase cap — fight ends at 0% HP. Enrage at 15 min.

## Existing C++ AI — no JSON strategy yet

**No JSON strategy yet; existing C++ AI does:**

- **`SapphironGroundPositionAction`** — triggers via `sapphiron_ground` (boss is
  on ground). Tank is moved continuously to `mainTankPos` (`3518.64, -5252.45`).
  Non-tanks: if `IsPreAirPhase()`, back out to a per-bot spread ring position
  (`PreAirSpreadPos`). If a Chill aura is ticking, move to a safe position away
  from it (`FindPosToAvoidChill`). Melee check `FindMeleePosToAvoidCleaveAndTail`
  to stay on side flanks out of both front and rear cones. Ranged/healers slot
  into an NW arc (angle `0.85π + 0.02π * group_slot_index`) at 35y / 30y.

- **`SapphironFlightPositionAction`** — triggers via `sapphiron_flight` (boss is
  flying). Frozen (icebolt-encased) bots yield the tick (they are the blocks).
  Once the full set of blocks has formed (`ReadyToHideBehindIceblock`), each bot
  computes a GUID-stable assignment to the nearest ice block and moves behind it
  (`GetIceblockHidePos`). Before blocks are ready (still forming), bots dodge
  Chill and hold the spread ring.

- **`SapphironGenericMultiplier`** — during flight phase **and** the pre-liftoff
  passive window, zeroes out every movement action except the Sapphiron-specific
  ones. Specifically suppresses: all `MovementAction` subtypes (follow, reach
  melee/spell, flee), `DpsAssistAction`, `TankAssistAction`,
  `CastDebuffSpellOnAttackerAction`, and `CastDeathGripAction` (always). This
  prevents generic chase/follow logic from dragging bots away from their ice
  block cover during the Frost Explosion window.

- **`sapphiron_frost_resistance_trigger`** — first-alive paladin switches to Frost
  Resistance Aura for the duration of the encounter.

## Implications for bot AI / strategy authoring

- **PHASE CLOCK = HARD LIMIT (stays C++) — flight/ice block/LoS is not
  data-expressible.** The Frost Explosion mechanic requires every non-frozen bot
  to be positioned *behind a specific dynamically-placed ice block* (a game
  object spawned at a randomly-targeted player's position at the time of the hit).
  The block positions are not known until flight begins, change each cycle, and the
  LoS shelter assignment must be GUID-stable to avoid bots chasing each other
  (`GetIceblockHidePos` uses sorted-GUID block assignment + group-slot sub-offset).
  No generic `shape` (orbit/stack/spread/attack) can express "stand behind the
  nearest dynamically-spawned ice block". This is equivalent in complexity to
  Heigan's dance or Thaddius's charge-side positioning — C++ is the only option.

- **The Frost Explosion LoS filter is in a SpellScript** — `spell_sapphiron_frost_explosion`
  filters targets by `IsValidExplosionTarget`, which walks the live `blockList`.
  Bots don't need to "cast" anything; they only need to be on the correct side of
  a block at the moment the explosion fires (~9.5 s after liftoff). Timing the
  hide movement correctly (wait for all blocks, then park) is why the C++ action
  has the `ReadyToHideBehindIceblock` guard.

- **IceBolt targets are selected from the *threat list*, not raid roster.** Bot AI
  uses `HasAura("icebolt", unit)` (name-based) to detect encased players — works
  across per-difficulty ids (`28522` / `28526`). The human is never required to
  be a block target or to hide behind a specific block; bots cover all LoS slots.
  (memory: human-never-required, find-target-is-threat-based.)

- **Melee positioning during ground phase:** Cleave (front arc) and Tail Sweep
  (rear arc) both repeat every 10 s. Melee bots must stay on side flanks only.
  The C++ helper `FindMeleePosToAvoidCleaveAndTail` handles this; any future JSON
  `melee_position` shape would need equivalent side-flank logic, which the current
  shape primitives don't support — keep in C++.

- **Blizzard NPCs move randomly** — bots use `FindPosToAvoidChill` to dodge the
  Chill aura when it lands on them. This is reactive (aura-triggered movement),
  not predictive; a JSON `spread` shape can't express it reliably. Keep in C++.

- **Chill has per-difficulty spell ids** — always detect by aura NAME "chill",
  never by id, on the 25-man active path. (Same rule applies to Icebolt.)

- **`find target` stays valid throughout the fight** — unlike Noth's balcony phase,
  Sapphiron never goes `REACT_PASSIVE` + `UNIT_FLAG_NOT_SELECTABLE` during the
  ground phase, and during flight the boss is still on the threat list (just not
  melee-attackable). The `sapphiron_ground` / `sapphiron_flight` triggers detect
  phase via `IsPhaseGround()` / `IsPhaseFlight()` (== `IsFlying()` on the boss
  unit), not by threat presence.

- **Frost Resistance Aura is automatic** — the shared
  `sapphiron_frost_resistance_trigger` subsystem (gated to alive paladins who know
  the spell) switches the first paladin to Frost Resistance Aura. No JSON action
  needed; this is already wired in C++.
