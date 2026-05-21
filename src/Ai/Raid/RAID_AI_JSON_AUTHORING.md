# Authoring JSON Raid Strategies

How to write boss tactics as **JSON** that the `json-raid` strategy loads and
**live-reloads** with no rebuild/restart. This is the experimental, opt-in layer
described in `RAID_AI_JSON_PLAN.md`. The hand-tuned C++ strategies are untouched;
JSON runs *instead of* them only while you toggle it on for a test.

See also: `RAID_AI_PATTERNS.md` (shape vocabulary + what must stay C++) and
`RAID_AI_INVENTORY.md` (per-raid class→shape lookup).

> **Mental model:** *Data wires and parameterizes. C++ implements behavior.*
> JSON can (1) wire existing C++ triggers/actions together with priorities
> (**Level 1**), and (2) parameterize a small set of generic shapes (**Level 2**).
> It cannot invent new behavior — irreducibly-complex fights stay in C++.

---

## Where files live

- Drop `*.json` in the directory the worldserver reads at runtime. Default:
  **`<worldserver-cwd>/raid_strategies/`** (i.e. `server/raid_strategies/` here).
- Override with `RaidJson.Dir = some/path/` in any loaded `.conf` (e.g.
  `playerbots.conf`). Trailing slash optional.
- Reference copies live in the module at
  `modules/mod-playerbots/data/raid_strategies/` — edit those for version
  control, but the **server-CWD copy is what loads**. `.rjson status` prints the
  exact absolute directory it read.
- One file per boss is the convention, but the loader merges **all** `*.json` in
  the directory into one rule set. Triggers self-gate (a rule for Anub only fires
  near Anub), so loading every boss at once is fine — same as the C++ strategy
  holding all of a raid's bosses.

---

## Commands (GM / console)

| Command          | What it does                                                                 |
|------------------|------------------------------------------------------------------------------|
| `.rjson status`  | Print source dir, file/rule/error counts, and how many of your bots run it.  |
| `.rjson reload`  | Re-read every JSON file, rebuild the rule set, re-init your json-raid bots.   |
| `.rjson on`      | For your bots: strip their C++ instance strategy, add `json-raid` (A/B swap). |
| `.rjson off`     | For your bots: remove `json-raid`, restore the proper C++ instance strategy.  |

Typical tuning loop: `.rjson on` → pull boss → edit JSON → `.rjson reload` →
re-pull → repeat. No build, no restart.

`on`/`off` act on the bots **you own** (your spawned playerbots). `reload`/`status`
also work from the server console, but only re-init bots when run in-game.

---

## Schema

```jsonc
{
  "name": "anubrekhan",          // free-form label (diagnostics only)
  "boss": "anub'rekhan",         // default boss NAME for encounter_active shapes
  "rules": [
    {
      "trigger": <TRIGGER>,      // exactly one trigger per rule
      "actions": [ <ACTION>, ... ]  // one or more, in priority order
    }
  ]
}
```

A `<TRIGGER>` is **either**:
- `{ "name": "<registered C++ trigger name>" }`  — **Level 1**, reuse existing, or
- `{ "shape": "<shape>", ...params }`            — **Level 2**, generic.

An `<ACTION>` is **either**:
- `{ "name": "<registered C++ action name>", "priority": N }`  — **Level 1**, or
- `{ "shape": "<shape>", "params": {...}, "priority": N }`      — **Level 2**.

`priority` is the offset added to `ACTION_RAID` (the engine base for raid tactics).
**Higher wins the tick.** It defaults to `1` if omitted. Mirror the offsets the
C++ strategy uses (see `RaidNaxxStrategy.cpp`): survival movement high (`+4`),
positioning low (`+1`).

---

## Level 2 — generic shapes (the callable surface)

These are the only parameterized shapes implemented so far. Add a new `Json*`
class **only when a fight needs it** (don't pre-build shapes).

### Trigger shape `encounter_active`  (→ `JsonEncounterActiveTrigger`)
Active while a creature with the given name is engaged/found nearby (same
`"find target"` lookup the C++ boss triggers use — match by **name**, not entry).

| Param  | Type   | Default        | Meaning                                  |
|--------|--------|----------------|------------------------------------------|
| `boss` | string | file `"boss"`  | Lower-case creature name to detect.      |

```json
{ "trigger": { "shape": "encounter_active" } }                  // uses file "boss"
{ "trigger": { "shape": "encounter_active", "boss": "gluth" } } // per-rule override
```

### Action shape `orbit_point`  (→ `JsonOrbitPointAction`)
Continuously walk the ring around `(x, y)` — a real orbit (the bot heads to the
next ring point each tick).

| Param       | Type  | Default | Meaning                                   |
|-------------|-------|---------|-------------------------------------------|
| `x`, `y`    | float | 0, 0    | Ring center (world XY on the boss's map). |
| `radius`    | float | 40      | Ring radius (yards).                      |
| `segments`  | int   | 16      | Number of waypoints on the ring.          |
| `clockwise` | bool  | true    | Direction of travel.                      |

```json
{ "shape": "orbit_point",
  "params": { "x": 3272.49, "y": -3476.27, "radius": 45.0, "segments": 16, "clockwise": true },
  "priority": 1 }
```

> **Not a clone of C++ Anub.** The bespoke `AnubrekhanPositionAction` only kites
> the *tank* around this ring during Locust Swarm and spreads ranged otherwise;
> `orbit_point` makes **every** bot under the trigger orbit while it's active.
> Same ring math, simpler behavior — good for proving the pipeline and for
> fights that genuinely just want everyone circling.

---

## Level 1 — reference existing C++ by name (the escape hatch)

For anything the shapes can't express, name an already-registered C++ trigger or
action. The JSON only re-wires and re-prioritizes them; the behavior is the
compiled C++.

**Where to find valid names** (the left-hand strings in each `creators[...] =`):
- Triggers: `Naxxramas/RaidNaxxTriggerContext.h`, and the equivalent
  `Raid<X>TriggerContext.h` per raid.
- Actions: `Naxxramas/RaidNaxxActionContext.h`, and `Raid<X>ActionContext.h`.
- Class spells (e.g. `"shield wall"`, `"barkskin"`) come from the class action
  contexts and are valid too.

A name that isn't registered silently resolves to nothing — verify against the
context headers. `four_horsemen.json` is a full Level-1 example (it reproduces
`RaidNaxxStrategy.cpp` lines ~149-190 as data, including the `+4 / +3 / +2 / +1`
priority layering).

---

## Worked example — `anubrekhan.json`

```json
{
  "name": "anubrekhan",
  "boss": "anub'rekhan",
  "rules": [
    {
      "trigger": { "shape": "encounter_active" },
      "actions": [
        { "shape": "orbit_point",
          "params": { "x": 3272.49, "y": -3476.27, "radius": 45.0, "segments": 16, "clockwise": true },
          "priority": 1 }
      ]
    }
  ]
}
```

Round-trip test (the success criterion): `.rjson on`, pull Anub, watch bots orbit
the ring. Edit `radius` to `30`, `.rjson reload`, re-pull — the orbit tightens
instantly, no rebuild. `.rjson off` restores the C++ strategy.

---

## Hard limits — what JSON CANNOT do

JSON wires and parameterizes; it does not implement logic. Anything below stays
in C++ (see the "Bespoke behaviors" list in `RAID_AI_PATTERNS.md`):

- Phase clocks / predicted timers (Heigan dance, Sapphiron flight, Thaddius
  polarity swaps).
- Stateful target selection or add-herding (Anub guards, Noth adds, Maexxna web
  wrap, Gluth zombies).
- Multi-actor relays / assignments (Four Horsemen corner rotation, Razuvious
  mind-control, Vashj/Kael mechanics, Yogg, Lich King, Mimiron).
- Anything reading boss script internals (`_currentSection`, channel state).

For these, write the behavior as a C++ action/trigger, register it in the raid's
context, then **reference it by name** from JSON (Level 1) if you want JSON to own
the wiring/priority.

---

## Adding a new Level-2 shape (when a fight needs it)

1. Add a `Json*` class in `JsonStrategy/JsonStrategyActions.{h,cpp}` (or
   `...Triggers`). Multiply-inherit `Qualified`; parse params in
   `Qualify(std::string)`; override `getName()` to return `"<base>::" + qualifier`.
2. Register its base name in `JsonStrategyContexts.h`
   (`creators["<base>"] = ...`).
3. Teach the loader (`JsonStrategyLoader.cpp`) to translate the new `"shape"` +
   params into `"<base>::<csv params>"`.
4. Document it in the Level-2 table above.
5. Re-run cmake **configure** if you added files (the module is glob-collected;
   a new file/dir needs a re-configure, not just a build).
```
