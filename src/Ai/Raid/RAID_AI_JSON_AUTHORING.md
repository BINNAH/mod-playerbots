# Authoring JSON Raid Strategies

How to write boss tactics as **JSON** that the `json-raid` strategy loads and
**live-reloads** with no rebuild/restart. This is the experimental, opt-in layer
described in `RAID_AI_JSON_PLAN.md`. The hand-tuned C++ strategies are untouched;
JSON runs *instead of* them only while you toggle it on for a test.

**Goal:** express as much raid AI as possible in JSON, so future boss strategies
are authored as data, not C++. Reach for a generic shape first; drop to C++ only
for genuinely irreducible mechanics (phase clocks, multi-actor relays — see
"Hard limits").

See also: `RAID_AI_PATTERNS.md` (shape vocabulary) and `RAID_AI_INVENTORY.md`
(per-raid class→shape lookup).

> **Mental model:** a rule is `trigger → [actions]`. The **trigger** decides
> *who* (role) and *when* (boss engaged + optional phase). The **actions** decide
> *what* (move/attack), in priority order. Both trigger and actions are generic,
> parameterized shapes — or a by-name reference to existing C++.

---

## Where files live

- Drop `*.json` in the directory the worldserver reads at runtime. Default:
  **`<worldserver-cwd>/raid_strategies/`** (i.e. `server/raid_strategies/`).
- Override with `RaidJson.Dir = some/path/` in any loaded `.conf`.
- The loader merges **all** `*.json` in the directory into one rule set. Triggers
  self-gate, so loading every boss at once is fine.
- Standard JSON (no `//` comments). You can stash notes in an unused key like
  `"_comment"` — the loader ignores keys it doesn't use.

---

## Commands (GM / console)

| Command          | What it does                                                                 |
|------------------|------------------------------------------------------------------------------|
| `.rjson status`  | Source dir, file/rule/error counts, how many of your bots run it.            |
| `.rjson reload`  | Re-read every JSON file, rebuild the rule set, re-init your json-raid bots.   |
| `.rjson on`      | For your bots: strip their C++ instance strategy, add `json-raid` (A/B swap). |
| `.rjson off`     | For your bots: remove `json-raid`, restore the proper C++ instance strategy.  |

Tuning loop: `.rjson on` → pull → edit JSON → `.rjson reload` → re-pull. No build.

---

## Schema

```jsonc
{
  "name": "anubrekhan",          // label (diagnostics only)
  "boss": "anub'rekhan",         // default boss NAME for triggers/targets below
  "rules": [
    {
      "trigger": <TRIGGER>,         // exactly one
      "actions": [ <ACTION>, ... ]  // one or more, each with its own priority
    }
  ],
  "suppress": [ ... ]              // optional: data-driven multipliers (see "Suppress")
}
```

`priority` is the offset added to `ACTION_RAID`; **higher wins the tick**, defaults
to `1`. A rule may list several actions at different priorities — they fall through
in order (an action that returns "nothing to do" yields to the next-lower one the
same tick). Mirror the C++ convention: survival/phase movement high (`+3`), add
pickup / spread mid (`+2`), plain attack low (`+1`).

---

## Level 2 — generic shapes (the callable surface)

### Trigger shape `encounter_active` (→ `JsonEncounterActiveTrigger`)
The universal condition. Active while the boss is engaged, optionally narrowed by
role and phase. **Evaluated per bot**, so one rule fans out correctly.

| Field          | Type    | Default     | Meaning                                                        |
|----------------|---------|-------------|----------------------------------------------------------------|
| `boss`         | string  | file `boss` | Creature name to detect (`"find target"`, matched by name).    |
| `role`         | string  | (all)       | Comma-list, OR semantics (see role tokens below).              |
| `boss_aura`    | string  | (none)      | Phase gate: an aura **on the boss**, by name (e.g. `locust swarm`). |
| `aura_present` | bool    | true        | `true`: only when the aura is up. `false`: only when it's down. |
| `include_cast` | bool    | true        | Also treat the phase as active while the boss is **casting** a spell of the same name — so the raid reacts at cast-start, not after the aura lands. Set `false` to gate strictly on the aura. |

```json
{ "shape": "encounter_active", "role": "maintank", "boss_aura": "locust swarm", "aura_present": true }
```

**Role tokens:** `all`, `maintank`, `offtank`, `tank`, `notmaintank`, `nontank`,
`ranged`, `melee`, `healer`, `dps`. Combine with commas, e.g. `"ranged,healer"`.

### Trigger shape `pre_cast_window` (→ `JsonPreCastWindowTrigger`)
Fires in the short window *before* a boss's **periodic** cast, so externals /
defensives can be pre-applied and carry through the hit — the case reactive
healing can't cover because the raid (healers included) is stunned, e.g.
Maexxna's 40s Web Spray. The boss exposes no readable timer, so this **predicts**
from a fixed cadence and **re-anchors** every time the cast is actually observed,
staying locked to the real rhythm instead of drifting. Evaluated per bot, with
its own predicted clock.

| Field          | Type        | Default      | Meaning                                                              |
|----------------|-------------|--------------|----------------------------------------------------------------------|
| `boss`         | string      | file `boss`  | Creature name to detect (engaged gate; resets the clock when gone).  |
| `spell`        | string\|int | (none)       | Anchor cast for re-anchoring — name or spell id. Observed as the boss mid-cast **or** its aura already on the bot (reliable for instant casts). Omit for a pure clock. |
| `interval`     | int (ms)    | **required** | Cadence between casts.                                               |
| `first_at`     | int (ms)    | `interval`   | Offset of the first cast from combat start.                          |
| `lead`         | int (ms)    | `4000`       | Open the window this long *before* the predicted cast.               |
| `tail`         | int (ms)    | `6000`       | Hold the window open this long *after* (covers a delayed cast).      |
| `require_aura` | string      | (none)       | Optional phase gate: only fire while this aura is on the boss (e.g. `frenzy`). |
| `role`         | string      | (all)        | Optional role filter (same comma-list tokens as `encounter_active`). |

```json
{ "shape": "pre_cast_window",
    "boss": "maexxna", "spell": 29484, "interval": 40000, "first_at": 40000,
    "lead": 4000, "tail": 6000, "require_aura": "frenzy", "role": "maintank,healer" }
```

> **Trigger fields go at the trigger top level — NOT in a `"params"` object.**
> Only *action* shapes nest tunables under `params`; trigger shapes
> (`encounter_active`, `pre_cast_window`) read their fields directly off the
> trigger object. Wrapping a trigger in `params` makes the loader miss every
> field — e.g. `interval` reads back as `0` → `pre_cast_window needs a non-zero
> 'interval' (ms)`.

Pair it with class-spell / external actions (by name) that self-gate on
knowability + cooldown, so listing several under one rule fires exactly the ones
the present bots can cast — same idiom as Four Horsemen's "opening defensive".

### Action shapes

| Shape             | Class                       | `params`                                  | Behavior |
|-------------------|-----------------------------|-------------------------------------------|----------|
| `orbit_point`     | `JsonOrbitPointAction`      | `x, y, radius, segments, clockwise`       | Continuously walk the ring around (x,y) — a real orbit/kite. |
| `stack_point`     | `JsonStackPointAction`      | `x, y, radius`                            | Move to (x,y) and stay within `radius` (tight raid stack). |
| `spread`          | `JsonSpreadAction`          | `radius`, `min_interval` (ms, default 3000) | Move away from the nearest **other ranged/healer** within `radius`. Ignores the melee/tank stack and repositions at most once per `min_interval` so casters aren't interrupted. Yields when clear. |
| `attack`          | `JsonAttackAction`          | `targets` (name/entry, string or array), `boss` (fallback, default file `boss`), `detect` (`threat`\|`nearest`), `select` (`lowest_hp`\|`nearest`) | Pick one creature to attack — the lowest-HP / nearest match from `targets`, falling back to `boss` when none are alive. **`detect`** = `threat` scans the attacker/threat list (default); `nearest` scans nearby NPCs so **off-threat** objects are visible (Web Wrap cocoons, un-aggroed adds). **`select`** = `lowest_hp` (default when `detect:threat`) is "kill adds first"; `nearest` (default when `detect:nearest`) is closest-first. Matches by name **or** entry id; sticks to its pick until it dies (no cast-cancel thrash); yields when already on target. To just focus one creature (e.g. the boss), name it in `targets`. Merges the former `attack_target`, `attack_priority` (`threat`+`lowest_hp`) and `attack_nearest` (`nearest`+`nearest`). |
| `tank_adds`       | `JsonTankAddsAction`        | `add`, `boss` (default file `boss`)       | Off-tank (assist-tank #0) gathers every living add named `add` and drags it onto the main tank / boss. attack → taunt → reposition. |
| `timed_safe_zone` | `JsonTimedSafeZoneAction`   | `zones` (array of `[x,y]`), `pattern` (array of zone indices), `z`, `first_at` (ms), `interval` (ms), `hold` (bool), `cast_while_moving` (bool), `tolerance` (default 5.0) | **A6 eruption dance** as data. The room has fixed safe `zones`; on a deterministic clock one zone after another is the only safe spot. Predicts the current safe zone (`pattern[k]` where `k` counts eruptions from `first_at`/`interval`) and stands on it. The generic form of `HeiganDanceAction`. `hold:true` = own the tick even when parked (tight cadence, no casting); `hold:false` = yield once parked so rotations run between eruptions. `cast_while_moving:true` = while **en route**, yield the tick so the bot's rotation fires INSTANTS as it relocates (the engine refuses cast-time spells while moving, so only instants come out) — **requires a `suppress` rule** (below) zeroing the movement-hijackers, else they grab the yielded tick. Per-bot clock auto-anchors on first run and re-anchors after a long idle gap (the rule going dormant across the *other* phase), so gate each phase with its own rule + cadence. |

```json
{ "shape": "stack_point",     "params": { "x": 3272.49, "y": -3476.27, "radius": 4.0 }, "priority": 3 }
{ "shape": "spread",          "params": { "radius": 8.0, "min_interval": 3000 }, "priority": 2 }
{ "shape": "attack",          "params": { "targets": "anub'rekhan" }, "priority": 1 }
{ "shape": "attack",          "params": { "targets": "crypt guard", "boss": "anub'rekhan" }, "priority": 1 }
{ "shape": "attack",          "params": { "targets": 16486, "detect": "nearest", "boss": "maexxna" }, "priority": 2 }
{ "shape": "tank_adds",       "params": { "add": "crypt guard", "boss": "anub'rekhan" }, "priority": 2 }
{ "shape": "timed_safe_zone", "params": { "zones": [[2756.0,-3704.0],[2794.9,-3668.1]], "pattern": [3,2,1,0,1,2], "z": 276.54, "first_at": 7000, "interval": 4000, "hold": true }, "priority": 32 }
```

> **Priority note for `timed_safe_zone` (and any movement shape that must beat
> avoid-aoe):** the generic `"avoid aoe"` action sits at `ACTION_EMERGENCY = 90`,
> above `ACTION_RAID` (60). To win the relevance race outright, wire survival
> movement at **`priority ≥ 31`** (offset on `ACTION_RAID`=60 → relevance ≥ 91 >
> 90). The loader does not clamp priority. A shape that holds the tick while it
> owns movement then beats everything below 91. When you instead want the shape to
> **yield** (e.g. `cast_while_moving`), priority alone isn't enough — yielding
> re-exposes the hijackers — so pair it with a `suppress` rule (below).

---

## Suppress — data-driven multipliers (the `InitMultipliers` analog)

The hand-tuned C++ strategies install **multipliers** that zero an action's
relevance during a phase (e.g. `HeiganDanceMultiplier` zeros `"avoid aoe"` so the
dance wins). `json-raid` exposes the same capability as data: a top-level
`suppress` array, sibling to `rules`.

```json
"suppress": [
  { "trigger": { "shape": "encounter_active", "boss": "heigan the unclean" },
    "actions": ["avoid aoe", "reach spell", "combat formation move", "flee"] }
]
```

Each entry: while its `trigger` (resolved exactly like a rule trigger — a `name`
or a shape) is active **for that bot**, every listed action **name** has its
relevance forced to 0, so it can't be selected. Names are matched against the
action's `getName()` — use the registered name (`"avoid aoe"`, `"reach spell"`,
`"reach melee"`, `"combat formation move"`, `"flee"`, …).

The primary use: let a movement shape **yield the tick for instant casts**
(`cast_while_moving`) without the eruption-dodge / reach / formation actions
grabbing the yielded tick and dragging the bot off its route. Pick the suppress
list deliberately — e.g. leave `"reach melee"` *un*-suppressed if melee still need
to close on a tanked boss during the phase. (This also lets a Level-1 port carry a
multiplier its C++ original relied on, like four_horsemen.)

---

## Level 1 — reference existing C++ by name (the escape hatch)

For anything the shapes can't express, name an already-registered C++ trigger or
action; JSON only re-wires/re-prioritizes it.

- Triggers: see `Naxxramas/RaidNaxxTriggerContext.h` (and `Raid<X>TriggerContext.h`).
- Actions: see `Naxxramas/RaidNaxxActionContext.h` (and `Raid<X>ActionContext.h`).
- Class spells (`"shield wall"`, `"barkskin"`, …) from the class action contexts.

`four_horsemen.json` is a full Level-1 example (reproduces `RaidNaxxStrategy.cpp`
lines ~149-190 as data, including the +4/+3/+2/+1 priority layering). Use Level-1
when behavior is genuinely bespoke; otherwise prefer a generic shape.

---

## Worked example — `anubrekhan.json`

Six rules, all role + phase gated. Outside Locust Swarm: main tank holds the boss,
off-tank tanks Crypt Guards on the boss, ranged/healers spread for Impale, DPS hit
the boss. During Locust Swarm: the tank kites the ring while everyone else stacks
tightly in the center.

```json
{
  "name": "anubrekhan",
  "boss": "anub'rekhan",
  "rules": [
    { "trigger": { "shape": "encounter_active", "role": "maintank",    "boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "attack", "params": { "targets": "anub'rekhan" }, "priority": 1 } ] },

    { "trigger": { "shape": "encounter_active", "role": "maintank",    "boss_aura": "locust swarm", "aura_present": true },
      "actions": [ { "shape": "orbit_point", "params": { "x": 3272.49, "y": -3476.27, "radius": 45.0, "segments": 16, "clockwise": true }, "priority": 3 } ] },

    { "trigger": { "shape": "encounter_active", "role": "notmaintank", "boss_aura": "locust swarm", "aura_present": true },
      "actions": [ { "shape": "stack_point", "params": { "x": 3272.49, "y": -3476.27, "radius": 4.0 }, "priority": 3 } ] },

    { "trigger": { "shape": "encounter_active", "role": "offtank",     "boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "tank_adds", "params": { "add": "crypt guard", "boss": "anub'rekhan" }, "priority": 2 } ] },

    { "trigger": { "shape": "encounter_active", "role": "ranged,healer","boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "spread", "params": { "radius": 8.0 }, "priority": 2 } ] },

    { "trigger": { "shape": "encounter_active", "role": "dps",         "boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "attack", "params": { "targets": "crypt guard", "boss": "anub'rekhan" }, "priority": 1 } ] }
  ]
}
```

Why it composes: a ranged DPS matches both the `dps` rule (attack @1) and the
`ranged,healer` rule (spread @2) — spread wins when stacked, otherwise it yields and
the bot attacks. During Locust Swarm the `aura_present:false` rules go quiet and the
two `aura_present:true` rules take over.

---

## Hard limits — what stays C++

Shapes wire and parameterize; they don't implement novel logic. Keep these in C++
and reference them by name (Level 1):

- Positional phase clocks where the safe spot is **reactive or random** (Sapphiron
  flight — dodge wherever the ice blocks land; Thaddius polarity swaps — keyed on
  the bot's own debuff). The *movement* is bespoke. (Two special cases are now
  generic: a fixed-pattern timed safe zone like the **Heigan dance** is
  `timed_safe_zone`; a periodic *cast* you only need to pre-mitigate is
  `pre_cast_window`.)
- Multi-actor relays / assignments (Four Horsemen corner rotation, Vashj/Kael,
  Yogg, Lich King, Mimiron, Razuvious mind-control).
- Anything reading boss script internals (channel state, `_currentSection`).

If you find yourself wanting a new *kind* of behavior repeatedly, that's a signal
to add a new generic shape (below) rather than a one-off C++ action.

---

## Adding a new Level-2 shape

1. Add a `Json*` class in `JsonStrategy/JsonStrategyActions.{h,cpp}` (or
   `...Triggers`). Multiply-inherit `Qualified`; parse params in
   `Qualify(std::string)`; override `getName()` → `"<base>::" + qualifier`. Wrap an
   existing C++ primitive where possible (e.g. `MoveInsideAction`, `AttackAction`).
2. Register its base name in `JsonStrategyContexts.h`.
3. Teach `JsonStrategyLoader.cpp` to translate the new `shape` + `params` into
   `"<base>::<encoded params>"` (CSV for numbers, `key=val|key=val` for names).
4. Document it in the shape table above.
5. Re-run cmake **configure** if you added files (module is glob-collected).
```
