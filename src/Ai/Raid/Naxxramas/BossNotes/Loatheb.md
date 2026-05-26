# Loatheb — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_loatheb.cpp`, `creature_template` and
`spelldifficulty_dbc` (world DB), verified 2026-05-26. **Verify IDs against the
live DB/source before trusting** (creature_template can drift; several Loatheb
debuffs *do* have per-difficulty spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Loatheb` |
| Boss entry | `16011` |
| Dummy/test entry | `29718` (`Loatheb (1)`) |
| Map | Naxxramas, `533` |
| Leash | evades if >50y from home position (IsInRoom check in script) |

## Adds

| Name | Entry | When | Notes |
|------|-------|------|-------|
| `Spore` | `16286` | Every 35s from T+15s | Spawned by Summon Spore (29234); `SetInCombatWithZone()` on spawn; killing one fires `DATA_SPORE_KILLED` → grants Fungal Bloom crit buff to nearby players |
| `Spore (1)` | `30068` | (test/alt variant) | Not spawned in normal encounter script |

The `Spore` add at entry `16286` is the live encounter add. `23876` (also named
`Spore`) appears in the DB but is unrelated to Naxxramas.

## Spells

| Spell | ID | 10 / **25** id | Cast cadence | Notes |
|-------|----|----------------|--------------|-------|
| Necrotic Aura | `55593` | same id both | First at T+10s; repeats every **20s** | Disables healing for **17s**; `EVENT_NECROTIC_AURA_FADING` fires at 14s (warning emote), `_REMOVED` at 17s. **No per-difficulty variant** — single id safe to gate on. |
| Deathbloom | `29865` | 10-man `29865` / **25-man `55053`** | T+5s, every 30s | DoT on raid. **Per-difficulty split** — gate by NAME not id on 25-man. |
| Inevitable Doom | `29204` | 10-man `29204` / **25-man `55052`** | T+2min; first 5 casts every 30s, then every **15s** | Escalating raid-wide DoT (soft enrage ramp). **Per-difficulty split** — gate by NAME not id on 25-man. |
| Summon Spore | `29234` | same id both | T+15s, every 35s | Summons a Spore add (entry 16286). No per-difficulty variant. |
| Berserk | `26662` | same id both | T+12min | Hard enrage. No per-difficulty variant. |

> **Difficulty-id note:** Necrotic Aura (55593), Summon Spore (29234), and Berserk
> (26662) are **single-id** across 10 and 25-man — safe to gate by id or name.
> **Deathbloom and Inevitable Doom have per-difficulty spell rows** confirmed in
> `spelldifficulty_dbc`: 10-man uses 29865/29204, **25-man uses 55053/55052**.
> Any bot logic that gates on these by id must include both rows, or — safer —
> gate by aura **NAME** so it resolves correctly on the user's active 25-man path.

## Phase structure

Loatheb is a **single-phase tank-and-spank** fight. There are no phase transitions,
no movement requirements, and no positional wipes. The complexity is entirely in the
**healing rhythm** and **Spore kill priority**:

- **Necrotic Aura cycle (repeating, every 20s):**
  - T+0: Necrotic Aura applied — all healing disabled (~99% reduction).
  - T+14s: "Fading" warning emote fires (`EVENT_NECROTIC_AURA_FADING`).
  - T+17s: Aura removed — 3s healing **window** opens.
  - T+20s: Next Necrotic Aura applied — healing disabled again.
  - Effective burst-heal window per cycle: **~3s** (17s disabled, 3s open).

- **Spore spawns (every 35s from T+15s):** One Spore add spawns and enters combat.
  Melee near the Spore when it dies receive **Fungal Bloom** (+50% crit for 30s),
  dramatically increasing raid DPS. Killing spores fast is a DPS gain — the entire
  melee cluster should switch to the Spore immediately.

- **Inevitable Doom ramp (from T+2min):** First application at 2 minutes; first 5
  casts every 30s, then every 15s. This is the soft enrage — raid damage escalates
  until healers can't keep up. Kill speed is the answer, not mechanics.

- **Berserk at T+12min:** Hard enrage. Fight should be over well before this.

## Implications for bot AI / strategy authoring

No JSON strategy yet; existing C++ AI does the following:

- **`LoathebGenericMultiplier`** (always active while Loatheb is on the threat
  list): sets `neglect threat = true` (bots ignore threat, everyone stacks on boss);
  suppresses `DpsAssistAction`, `TankAssistAction`, `CastDebuffSpellOnAttackerAction`,
  `FleeAction`, and `CombatFormationMoveAction` (zeroed out) — bots stay planted and
  don't scatter. For healing spells specifically: checks whether Necrotic Aura is
  active on the bot via `NaxxSpellIds::NecroticAura10` (55593), with a fallback to
  aura name `"necrotic aura"`. If the aura has >1500 ms duration remaining, all
  `CastHealingSpellAction` calls return 0.0f — healers hold casts. If aura is absent
  or nearly expired (≤1500ms), healing is allowed — this models the burst-heal window.

- **`LoathebPositionAction`** (ACTION_RAID+1, fires on `"loatheb"` trigger): Tank
  (if has aggro on boss target) moves to `mainTankPos {2877.57, -3967.00}`; ranged
  move to `rangePos {2896.96, -3980.61}`. Melee have no special position — they stay
  on boss via the multiplier's formation-move suppression.

- **`LoathebChooseTargetAction`** (ACTION_RAID+1, same trigger): Scans `"attackers"`
  list for a unit named `"spore"`. If a Spore is within 1.0y of the bot, switches
  target to the Spore; otherwise targets Loatheb. **Caveat:** the 1.0y gate means
  a bot only switches to the Spore if it is already adjacent — bots not in melee
  range of the Spore will stay on Loatheb. This is intentional for ranged (ranged
  should stay on boss for sustained DPS; only melee in the pile get the crit buff).

- **`LoathebTrigger`**: fires whenever `helper.UpdateBossAI()` is true, i.e. while
  Loatheb is alive and on the bot's threat list (uses `"find target"` lookup). The
  trigger gates both position and target-choice actions.

**Hard limits (require C++ or are already covered by C++):**

- **Healing-window timing** is a hard C++ concern. The 3s window is too narrow
  and too clock-dependent to express reliably in data. The existing multiplier
  handles it correctly by reading the live aura duration — keep it in C++.
- **`neglect threat` global + formation-move suppression** must stay in the
  multiplier. Data expressions have no mechanism to zero out other actions
  or set engine-level flags.

**Data-expressible (candidates for json-raid if a JSON strategy is ever authored):**

- **Spore kill priority:** `"attack"` action targeting a Spore add by name is
  exactly what `LoathebChooseTargetAction` does. A json-raid `attack` shape on
  the Spore add, gated by `encounter_active`, could replicate or extend this,
  potentially without the 1.0y proximity gate (letting all melee switch on spawn).
- **Tank/ranged positioning:** the fixed `mainTankPos` / `rangePos` coordinates
  map cleanly to a `stack` or `attack` anchor shape in json-raid.
- **Overall stack:** the "everyone on boss" posture is already enforced by the
  multiplier's formation-move suppression; a json-raid `stack` could layer on top.

**Standard gotchas:**

- `"find target"` is threat-based — only sees Loatheb while this bot has threat on
  him. The trigger and all position/target actions go dark for bots that haven't
  landed a hit yet. (memory: find-target-is-threat-based.) The multiplier's initial
  `"find target"` lookup has the same blind spot; healers who never directly attack
  may not be gated by the multiplier until they pick up threat via a heal.
- Gate Deathbloom and Inevitable Doom detections by aura **NAME**, not id — the
  25-man spell ids (55053 / 55052) differ from the 10-man rows (see Difficulty-id
  note above). The existing C++ AI does not attempt to detect these; this matters
  only if a future strategy tries to react to them (e.g. defensive CDs on Doom ramp).
- **Human never required** — all bot roles (tank, healers, melee switching to
  Spore) must be fully covered by bots with no dependency on the human filling a
  required slot. (memory: human-never-required.)
- Loatheb is a contiguous single-phase fight — `encounter_active` keyed on Loatheb
  being alive stays true for the whole encounter with no blind-spot gaps (unlike
  Noth's balcony phase). The Spore is `SetInCombatWithZone()` on spawn, so it
  immediately enters the `"attackers"` list and is visible to the choose-target scan.
