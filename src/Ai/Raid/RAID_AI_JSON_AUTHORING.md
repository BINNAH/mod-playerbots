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
  ]
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

### Action shapes

| Shape             | Class                       | `params`                                  | Behavior |
|-------------------|-----------------------------|-------------------------------------------|----------|
| `orbit_point`     | `JsonOrbitPointAction`      | `x, y, radius, segments, clockwise`       | Continuously walk the ring around (x,y) — a real orbit/kite. |
| `stack_point`     | `JsonStackPointAction`      | `x, y, radius`                            | Move to (x,y) and stay within `radius` (tight raid stack). |
| `spread`          | `JsonSpreadAction`          | `radius`, `min_interval` (ms, default 3000) | Move away from the nearest **other ranged/healer** within `radius`. Ignores the melee/tank stack and repositions at most once per `min_interval` so casters aren't interrupted. Yields when clear. |
| `attack_target`   | `JsonAttackTargetAction`    | `target` (default file `boss`)            | Focus the named creature. Yields when already on it. |
| `attack_priority` | `JsonAttackPriorityAction`  | `adds` (string or array), `boss` (default file `boss`) | Focus the lowest-HP living add whose name is in `adds`; fall back to `boss` when none are up. "Kill adds first, then boss." |
| `tank_adds`       | `JsonTankAddsAction`        | `add`, `boss` (default file `boss`)       | Off-tank (assist-tank #0) gathers every living add named `add` and drags it onto the main tank / boss. attack → taunt → reposition. |

```json
{ "shape": "stack_point",     "params": { "x": 3272.49, "y": -3476.27, "radius": 4.0 }, "priority": 3 }
{ "shape": "spread",          "params": { "radius": 8.0, "min_interval": 3000 }, "priority": 2 }
{ "shape": "attack_target",   "params": { "target": "anub'rekhan" }, "priority": 1 }
{ "shape": "attack_priority", "params": { "adds": "crypt guard", "boss": "anub'rekhan" }, "priority": 1 }
{ "shape": "tank_adds",       "params": { "add": "crypt guard", "boss": "anub'rekhan" }, "priority": 2 }
```

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
      "actions": [ { "shape": "attack_target", "params": { "target": "anub'rekhan" }, "priority": 1 } ] },

    { "trigger": { "shape": "encounter_active", "role": "maintank",    "boss_aura": "locust swarm", "aura_present": true },
      "actions": [ { "shape": "orbit_point", "params": { "x": 3272.49, "y": -3476.27, "radius": 45.0, "segments": 16, "clockwise": true }, "priority": 3 } ] },

    { "trigger": { "shape": "encounter_active", "role": "notmaintank", "boss_aura": "locust swarm", "aura_present": true },
      "actions": [ { "shape": "stack_point", "params": { "x": 3272.49, "y": -3476.27, "radius": 4.0 }, "priority": 3 } ] },

    { "trigger": { "shape": "encounter_active", "role": "offtank",     "boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "tank_adds", "params": { "add": "crypt guard", "boss": "anub'rekhan" }, "priority": 2 } ] },

    { "trigger": { "shape": "encounter_active", "role": "ranged,healer","boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "spread", "params": { "radius": 8.0 }, "priority": 2 } ] },

    { "trigger": { "shape": "encounter_active", "role": "dps",         "boss_aura": "locust swarm", "aura_present": false },
      "actions": [ { "shape": "attack_priority", "params": { "adds": "crypt guard", "boss": "anub'rekhan" }, "priority": 1 } ] }
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

- Phase clocks / predicted timers (Heigan dance, Sapphiron flight, Thaddius swaps).
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
