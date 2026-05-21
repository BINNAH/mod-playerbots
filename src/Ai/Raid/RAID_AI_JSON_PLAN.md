# Plan: JSON-Driven Raid Strategy (experimental, opt-in)

Handoff plan for a fresh context to implement. Read alongside `RAID_AI_PATTERNS.md`
(shape vocabulary + engine wiring) and `RAID_AI_INVENTORY.md` (per-raid class→shape
lookup) in this directory.

## Goal

Let boss tactics be defined in **JSON** that can be **reloaded live** (no rebuild,
no server restart), as a **completely separate, opt-in system** toggled by chat
commands — so we can A/B a JSON strategy against the existing hand-tuned C++ one.
Do NOT modify or remove the existing C++ raid strategies. This is additive and
reversible.

Motivation: the C++ rebuild+restart loop (~30-60s) is the main friction when
tuning boss tactics. There is no DLL hot-swap in AzerothCore (monolithic,
static-linked), so the win comes from moving *wiring* and *tunable params* to data.

Guiding principle (already settled): **Data wires and parameterizes. C++
implements behavior.** Irreducibly-complex fights (Heigan clock, Yogg, Lich King,
Netherspite, Kael weapons, Vashj relay, Mimiron) stay in C++.

## Starting boss: Anub'Rekhan (walking skeleton), then Four Horsemen (scale test)

**Anub'Rekhan** is reproducible almost entirely from generic shapes + JSON params,
so it proves the full pipeline on the simplest real case:
- Trigger `"anub'rekhan"` = encounter active (shape T7).
- Action `AnubrekhanPositionAction` = `RotateAroundTheCenterPointAction(ai,
  "anub'rekhan position", 3272.49f, -3476.27f, 45.0f, 16)` — shape A2, params:
  center (3272.49, -3476.27), radius 45, 16 segments.
- `AnubrekhanChooseTargetAction` (A12) exists but is NOT wired under the boss
  trigger in `RaidNaxxStrategy.cpp` — confirm whether it's needed.

**Four Horsemen** is the second target once the plumbing works: 5 triggers,
priority layering, role checks, hazard avoidance. Mostly Level-1 (its actions are
bespoke C++ that the JSON references by name).

## Two levels of "data-driven" (scope discipline)

- **Level 1 — data-driven WIRING:** JSON lists `trigger → [action+priority]`
  referencing existing registered C++ names. Live re-order / re-prioritize /
  enable-disable. No new behavior code.
- **Level 2 — data-driven PARAMS:** generic parameterized actions/triggers (one
  C++ class per catalog shape) that read numbers (coords, radius, aura, entry,
  stacks) from the JSON rule.

Anub needs only a thin slice of Level 2 (A2 orbit + T7 encounter, maybe A12). Build
the core Level-2 plumbing on Anub; reuse Level-1 (reference-by-name) for Four
Horsemen's bespoke actions.

---

## VERIFIED facts (checked by reading source)

- Engine wiring: `Raid<X>Strategy::InitTriggers(std::vector<TriggerNode*>&)` pushes
  `TriggerNode("<trigger name>", { NextAction("<action name>", ACTION_RAID + N) })`.
  `InitMultipliers(...)` pushes `Multiplier*`. (Read `Naxxramas/Strategy/RaidNaxxStrategy.cpp`.)
- Triggers implement `bool IsActive()`; actions implement `bool Execute(Event)`
  (return true = fired/consumed tick, false = fall through). Names are resolved to
  classes by per-raid Context factories (`Raid<X>ActionContext.h` /
  `Raid<X>TriggerContext.h`).
- Generic movement primitives exist in `src/Ai/Base/Actions/MovementActions.h`:
  `MoveInsideAction` (x,y,radius), `RotateAroundTheCenterPointAction`
  (x,y,radius,segments[,dir]), `MoveAwayFromCreatureAction` (entry,radius),
  `MoveAwayFromPlayerWithDebuffAction` (aura,dist), `FleeAction`, and
  `MovementAction::MoveFromGroup(dist)`, `MoveTo(map,x,y,z)`, `MoveAway(unit,dist)`.
- Anub'Rekhan specifics (above) read from `Naxxramas/Action/RaidNaxxActions.h` and
  `RaidNaxxStrategy.cpp`.

## DISCOVERY TASKS for the implementing context (confirm before/while building)

These I did NOT verify — find them first; they shape the implementation:

1. **Strategy registration & attachment.** How does a named strategy get created
   (StrategyContext factory) and attached to a bot? How do raid strategies get
   auto-applied in a raid map / for an encounter? Find where `RaidNaxxStrategy` is
   registered and how bots receive it (likely a strategy context + combat strategy
   list, or a map/area hook). The JSON strategy must register as a NEW named
   strategy (e.g. `"json-raid"` or `"raid-json-naxx"`) without disturbing the
   existing one.
2. **Chat/console command registration.** How does playerbots add commands? Note
   the precedent: mod-ollama-chat added `.ollama reload` to live-reload its conf
   (see project CLAUDE.md) — find that implementation as a template, but also check
   playerbots' own command handler (PlayerbotMgr / chat handler).
3. **JSON library.** Reuse whatever mod-ollama-chat uses to parse OpenAI API JSON
   (likely a vendored `nlohmann/json` single-header). Confirm and reuse — do NOT
   add a new dependency.
4. **Live re-init on reload.** After re-reading JSON, bots already running the
   strategy must rebuild their TriggerNodes. Find how to force a strategy re-init
   (re-add the strategy, or clear+rebuild its trigger list) for active bots.
5. **`ACTION_RAID` value/macro** location, and the relevance/priority semantics
   (confirm higher = wins tick) — used to map JSON `priority` → `ACTION_RAID + N`.
6. **A/B coexistence.** Decide how JSON Anub rules and C++ Anub rules avoid
   double-firing during the test (see Phase 2 options).

---

## Phased implementation plan

### Phase 1 — Core plumbing (build on Anub)
- New subfolder, e.g. `src/Ai/Raid/JsonStrategy/` (gitignored data dir for the JSON
  files themselves, or put JSON under `mod-playerbots/data/raid_strategies/`).
- `JsonRaidStrategy : public Strategy` whose `InitTriggers` reads parsed rules and
  pushes `TriggerNode(triggerName, { NextAction(actionName, ACTION_RAID + prio) })`.
  Source of truth = an in-memory rule set loaded from JSON (see Phase 2 reload).
- Generic Level-2 actions/triggers needed for Anub (register names in a JSON
  action/trigger context):
  - `JsonOrbitPointAction` (A2): wraps/derives `RotateAroundTheCenterPointAction`,
    params `{x, y, radius, segments, dir?}`.
  - `JsonEncounterActiveTrigger` (T7): active while a given boss entry is engaged/
    alive, param `{boss_entry}` (or reuse an existing encounter-active trigger).
  - Optional `JsonChooseTargetAction` (A12) if Anub needs it.
- Keep the generic class set MINIMAL — only what Anub needs. Add more shapes
  on-demand when Four Horsemen / later fights require them.

### Phase 2 — Chat commands + opt-in toggle + reload
- Commands (names illustrative): 
  - `.rjson reload` — re-read JSON files from disk, rebuild rule set, re-init
    active JSON-mode bots.
  - `.rjson on` / `.rjson off` — for your current bots/group: enable the JSON
    strategy and suppress the C++ one for the boss(es) under test (A/B).
  - `.rjson status` — show active mode + loaded rule counts.
- A/B coexistence options (pick one):
  - (a) `.rjson on` swaps the bot's raid strategy: remove `+raid-naxx`-equivalent,
    add `+json-raid`. Cleanest isolation.
  - (b) A global flag the C++ Naxx strategy checks to SKIP bosses that have a JSON
    override while the flag is on. More surgical but touches the C++ strategy (a
    tiny, reversible guard — acceptable since it only gates, doesn't remove).
- Follow the `.ollama reload` precedent for live data reload without restart.

### Phase 3 — Anub'Rekhan JSON + A/B test
- Author `anubrekhan.json` (schema below).
- In-game: pull Anub with C++ strategy (baseline), then `.rjson on` + `.rjson
  reload` and re-pull; confirm bots orbit (3272.49, -3476.27) at r=45, 16 segments
  identically. Tweak the JSON radius, `.rjson reload`, re-pull — confirm instant
  change with no rebuild. That round-trip is the success criterion.

### Phase 4 — LLM authoring guide ("strategy generator")
Write `RAID_AI_JSON_AUTHORING.md` (and later promote to a Claude Code skill
`SKILL.md` so `/generate-strategy <boss>` emits JSON). It must contain:
- The JSON schema (below), fully annotated.
- The shape → generic-action "API": for each registered Json* action/trigger, its
  name and required params. (This is the callable surface for JSON authors.)
- The Level-1 escape hatch: how to reference an existing bespoke C++ action by name
  (with the list of names from the context factories) for fights that need it.
- A fully worked example (annotated Anub JSON).
- How to load/reload/test (the Phase 2 commands).
- Hard limits: which behaviors CANNOT be expressed in JSON → must stay C++ (point
  at the "Bespoke behaviors" list in `RAID_AI_PATTERNS.md`).
- Pointer to `RAID_AI_PATTERNS.md` / `RAID_AI_INVENTORY.md` as the shape reference.

### Phase 5 — Four Horsemen (scale test)
- Mostly Level-1: JSON references existing FH C++ actions/triggers by name
  (`"four horsemen void zone"` → `"four horsemen avoid void zone"` @ +4, etc.),
  reproducing `RaidNaxxStrategy.cpp` lines ~149-190 as data.
- Add generic shapes only where they cleanly replace a thin wrapper.
- Validates priority layering + multi-rule fights work from JSON.

### Phase 6 (future) — Externalize boss-helper constants
- Move tunable magic numbers (e.g. `FourHorsemenBossHelper` positions, mark
  thresholds, timers) into a reloadable table so bespoke fights are tunable from
  data too, without rebuild.

---

## Proposed file layout
```
mod-playerbots/
├── src/Ai/Raid/JsonStrategy/
│   ├── JsonRaidStrategy.{h,cpp}        # data-driven Strategy
│   ├── JsonStrategyActions.{h,cpp}     # generic Json* actions (one per shape, as needed)
│   ├── JsonStrategyTriggers.{h,cpp}    # generic Json* triggers
│   ├── JsonStrategyLoader.{h,cpp}      # parse + hold rule set; reload
│   └── JsonStrategyCommands.{h,cpp}    # .rjson chat commands
└── data/raid_strategies/
    ├── anubrekhan.json
    └── four_horsemen.json   (Phase 5)
```

## JSON schema (draft) + Anub example

```jsonc
{
  "name": "anubrekhan",
  "boss_entry": 15956,            // confirm Anub'Rekhan creature entry
  "rules": [
    {
      "trigger": { "shape": "encounter_active" },     // T7 (generic), or "name": "anub'rekhan" to reuse C++ trigger
      "actions": [
        {
          "shape": "orbit_point",                     // A2 -> JsonOrbitPointAction
          "params": { "x": 3272.49, "y": -3476.27, "radius": 45.0, "segments": 16 },
          "priority": 1                                // -> ACTION_RAID + 1
        }
      ]
    }
  ]
}
```
- `trigger`/`action` may be EITHER `{"shape": ...,"params": ...}` (Level-2 generic)
  OR `{"name": "<existing C++ registered name>"}` (Level-1 reference). The loader
  resolves both to TriggerNode/NextAction.
- `priority` is the offset added to `ACTION_RAID`.

## Risks / gotchas
- **Double-firing in A/B:** if both C++ and JSON Anub rules are active, bots get
  conflicting positioning. Resolve via the Phase 2 toggle before testing.
- **Reload must re-init bots:** re-reading JSON is useless if running bots keep
  stale TriggerNodes — wire the re-init (discovery task 4).
- **Name resolution:** Level-1 references only work for names actually registered
  in the raid's Action/Trigger context — verify names against the context headers.
- **Coordinate frame:** confirm RotateAroundTheCenterPoint uses world XY at the
  encounter map; Anub's z is implicit (ground). Reuse the exact C++ values first.
- **Don't expand the generic shape set speculatively** — implement a shape's Json*
  class the first time a fight needs it, not before.

## Handoff
Point the implementing context at this file + `RAID_AI_PATTERNS.md` +
`RAID_AI_INVENTORY.md`. Start at Phase 1 with Anub'Rekhan. Build is the user's job
(do NOT auto-run cmake); the user compiles and tests in-game.
```
