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
| `.rjson on`      | For your bots: strip their C++ instance strategy, add `json-raid` (A/B swap). Announces "type `.rjson pull` …" in party/raid. |
| `.rjson off`     | For your bots: remove `json-raid`, restore the proper C++ instance strategy. Also clears the engage flag.  |
| `.rjson pull`    | **"Call the pull."** Flags your bots engaged so the `manual_engage` rules fire (tanks run in + pull their assigned add) without waiting for combat. Announces in party/raid. |
| `.rjson stop`    | Clear the engage flag — re-arm before the next pull, or abort one. |

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
| `class`        | string  | (all)       | Comma-list of class names, AND-ed with `role`: `warrior, paladin, hunter, rogue, priest, deathknight` (or `dk`), `shaman, mage, warlock, druid`. Targets a class-specific job — e.g. `role:ranged, class:"mage,hunter"` for the Gluth chow kiters. |
| `detect`       | string  | `threat`    | How the boss is located. `threat` = the find-target / threat list. `nearest` = a **proximity scan** (`"nearest npcs"`, ~sight range), which is **threat-independent** — it fires for a bot that never threatens the boss (a kiter / off-tank who only ever touches the adds). Everyone who actually fights the boss can stay on the default. |
| `boss_aura`    | string  | (none)      | Phase gate: an aura **on the boss**, by name (e.g. `locust swarm`). |
| `aura_present` | bool    | true        | `true`: only when the aura is up. `false`: only when it's down. |
| `include_cast` | bool    | true        | Also treat the phase as active while the boss is **casting** a spell of the same name — so the raid reacts at cast-start, not after the aura lands. Set `false` to gate strictly on the aura. |
| `self_aura`    | string  | (none)      | Phase gate on an aura **on the bot itself** (e.g. `mutating injection`) — the self-debuff case `boss_aura` can't cover (Mutating Injection runner, Thaddius polarity, Festergut spore). Checked with `HasAura(name, bot)`. |
| `self_aura_present` | bool | true   | `true`: only while the bot carries `self_aura`. `false`: only while it does not (e.g. the "injection cleared, re-stack" default). |

```json
{ "shape": "encounter_active", "role": "maintank", "boss_aura": "locust swarm", "aura_present": true }
{ "shape": "encounter_active", "role": "ranged,healer", "self_aura": "mutating injection", "self_aura_present": true }
```

**Role tokens:** `all`, `maintank`, `offtank`, `offtank1`, `offtank2`, `offtank3`,
`tank`, `notmaintank`, `nontank`, `ranged`, `melee`, `healer`, `dps`. Combine with
commas, e.g. `"ranged,healer"`. `offtank1/2/3` are the 1st/2nd/3rd **assist tanks**
(by index) — use them to give one off-tank a different job than the others (e.g.
`offtank1` does a tank swap on the boss while `offtank2,offtank3` tank adds).

> **The predicates OVERLAP — comma is OR, prefix `!` to EXCLUDE (AND).** `IsMelee`
> is literally `!IsRanged`, so a melee **tank** *and* a melee-spec **healer** both
> read as `melee`; likewise a healer can read as `melee`. A bare `"melee"` rule
> therefore also catches the tanks — on Thaddius this made the **main tank** match
> both `maintank` (→ left add) and `melee` (→ right add) and oscillate through the
> center. Write **"melee DPS only" as `"melee,!tank,!healer"`**. Positive tokens are
> OR-ed (≥1 must match); each `!token` is a hard exclusion AND-ed in. A list of only
> exclusions (`"!tank"`) matches anyone tripping none of them. When you split a raid
> across two targets by role, make the role sets provably disjoint or one bot will be
> pulled two ways.

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

### Trigger shape `adds_near` (→ `JsonAddsNearTrigger`)
Fires while at least `count` living creatures matching `add` are within `range` of
the bot (`of: self`) or the boss (`of: boss`). Detection is a **proximity scan**
(`"nearest npcs"`), so it is **threat-independent** — it sees adds (and the boss)
a bot has no aggro on, the gate the threat-based `encounter_active` can't give
kiters / off-tanks. Use it to gate add-control actions (snares / AoE-threat) and
count-based switches (e.g. *"start kiting once ≥ 6 chow pile on me"* → a higher-
priority orbit rule whose trigger is `count: 6`, with the hold rule below it).

| Field   | Type        | Default | Meaning                                                            |
|---------|-------------|---------|--------------------------------------------------------------------|
| `add`   | string\|int | **req** | Add name or entry id to count.                                     |
| `range` | float       | `0`     | Max distance from the reference point (`0` = anywhere in sight).   |
| `count` | int         | `1`     | Minimum number within range for the trigger to fire.               |
| `of`    | string      | `self`  | Reference point for `range`: `self` (the bot) or `boss`.           |
| `role`  | string      | (all)   | Optional role filter (same tokens as `encounter_active`) so only e.g. off-tanks react. |
| `class` | string      | (all)   | Optional class filter (same tokens as `encounter_active`), AND-ed with `role`. |
| `boss`  | string      | file `boss` | Boss name (required when `of: boss`).                          |

```json
{ "shape": "adds_near", "add": "zombie chow", "range": 9.0, "count": 6, "of": "self", "role": "offtank" }
```

### Trigger shape `target_hp_ahead` (→ `JsonTargetHpAheadTrigger`)
Fires while the bot's **current target** is at/below `below` HP% **and** at least
one **other** named creature is `margin` HP% (or more) **higher**. The
cross-target HP compare a single-target gate can't do: the **death-sync throttle**
for twin adds that must die together (Thaddius's Feugen + Stalagg). Pair it with a
`suppress` rule listing the **`@damage`** category token (below) to zero the bot's
damage casts while *its* add is too far ahead, so the other catches up — the data
form of `ThaddiusGenericMultiplier`'s `≤40% / ≥3%` clamp. The `others` are located
by a **proximity scan** (`"nearest npcs"`), so it is **threat-independent**: a bot
reads both adds' HP even though it only ever threatens the one it tanks/DPSes. The
bot's own current target is excluded from `others`, so naming both adds is fine.
Evaluated per bot (it reads *that* bot's target), so the bots on the **ahead**
(lower-HP) add throttle while the bots on the behind add keep hitting.

| Field    | Type        | Default | Meaning                                                                 |
|----------|-------------|---------|-------------------------------------------------------------------------|
| `others` | string\|int\|array | **req** | Creature name(s)/entry id(s) to compare the bot's current target against. |
| `margin` | float       | `0`     | Throttle only when an `other` is this many HP% **above** the bot's target. |
| `below`  | float       | `100`   | Only throttle once the bot's target is at/below this HP% (the execute-range gate; `100` = always). |

```json
{ "shape": "target_hp_ahead", "others": ["feugen", "stalagg"], "margin": 3, "below": 40 }
```

### Trigger shape `target_victim` (→ `JsonTargetVictimTrigger`)
Fires while the bot's **current target** is being hit by a **bot** group member whose
role matches `role` (default `tank`) — i.e. *"my target is already handled by a
\<role\>."* Built for **`suppress`**: zero `"taunt spell"` while a tank's add is
already on **another tank**, so two tanks never taunt off each other during a
Magnetic-Pull-style swap (the boss's threat-swap re-assigns them on its own). The
taunt still fires to grab an add back from a DPS/healer — that victim isn't a tank,
so this stays inactive. A human / non-bot victim reads as "not handled" (inactive),
so the taunt can still recover the add. Evaluated per bot (reads *that* bot's target).

| Field  | Type   | Default | Meaning                                                       |
|--------|--------|---------|---------------------------------------------------------------|
| `role` | string | `tank`  | Victim role to match (same tokens as `encounter_active`, incl. `!`). |

```json
"suppress": [
  { "trigger": { "shape": "target_victim", "role": "tank" }, "actions": ["taunt spell"] }
]
```

### Trigger shape `manual_engage` (→ `JsonManualEngageTrigger`)
The **"call the pull"** gate. Fires while the bot's owner has issued **`.rjson pull`**
(an in-memory engage flag), `role` matches, and (if `add` is set) that add is alive
within `range`. Lets the raid leader kick off the engage *on command* — e.g. main
tank on one add, off-tank on the other — instead of waiting for combat. `.rjson stop`
(or `.rjson off`) clears the flag.

> **Hand-off is in the `move_to_target` ACTION, not here.** The pull stays active in
> combat on purpose; `move_to_target` latches once a bot **reaches** its add and then
> yields, so the in-combat AI takes over (see that shape). Do **not** gate this
> trigger on `IsInCombat()` — a bot flagged in combat *early* (an off-tank taunting,
> or anyone caught by AoE while still on the ramp) would lose its climb action and
> hand off to a C++ action that can't climb, getting stuck at the bottom. Latching on
> *reached* keeps it climbing until it's actually up. So you don't need `.rjson stop`
> after a clean pull. Corollary: **give pull rules no `attack` action** (especially
> healers) — let `move_to_target` climb, then the in-combat AI engages the nearest add
> / parks ranged at the anchor; a fixed `attack` here just fights the combat AI (e.g.
> drags a tank back to its original add after a swap).

**Pairing with the climb (`move_to_target`).** For a low→high platform engage,
drive the climb with a higher-priority **`move_to_target`** action on the same rule:
it walks the bot **toward the live add's Unit**, which paths up the ramp on its own
(see that shape — a fixed `(x,y,z)` point does *not* climb; it dives into the hazard
below). The `attack` then fires on arrival. So the modern Thaddius pull is fully
autonomous (no human leading); see `thaddius.json`.

**`range` (legacy / assignment gate).** An earlier workaround for the climb gated
the engage on proximity so a bot stayed silent and just **followed you up the ramp**
until you led it within `range` of its `add`. `move_to_target` supersedes that for
the *climb* itself, but `range` is still useful as a pure **assignment gate** (only
break off to *this* add once near it) and it **auto-hands-off on a knockback/pull**:
yanked out of `range`, the bot falls through to the in-combat rules (e.g. the
Level-1 nearest-pet positioning), which retarget it.

| Field   | Type        | Default | Meaning                                                                 |
|---------|-------------|---------|-------------------------------------------------------------------------|
| `add`   | string\|int | (none)  | Only fire while this add is **alive** (proximity scan, threat-independent). The per-add assignment key; also stops the rule once the add dies. |
| `range` | float       | `0`     | With `add`: only fire within this many yards of it (`0` = any distance in sight). Set it so bots follow you up first and break off near their add. |
| `role`  | string      | (all)   | Role filter (same tokens as `encounter_active`, incl. `!` exclusions) — give MT vs OT different adds. |
| `split` | string      | (none)  | `"i/n"`: divide the **bots** matching `role` into `n` contiguous groups (by stable group order) and fire only for the `i`-th. Sends **one role to two targets** — e.g. healers `split:"1/2"` → left add, `split:"2/2"` → right — which a single role token can't. Humans aren't counted, so the bots divide evenly; dead bots still count (sides don't reshuffle on a death). |

```json
{ "trigger": { "shape": "manual_engage", "add": "stalagg", "role": "maintank", "range": 25.0 },
  "actions": [ { "shape": "attack", "params": { "targets": "stalagg", "detect": "nearest" }, "priority": 1 } ] }

{ "trigger": { "shape": "manual_engage", "role": "healer", "split": "2/2" },
  "actions": [ { "shape": "move_to_target", "params": { "target": "feugen", "detect": "nearest", "distance": 5.0 }, "priority": 3 } ] }
```

### Action shapes

| Shape             | Class                       | `params`                                  | Behavior |
|-------------------|-----------------------------|-------------------------------------------|----------|
| `orbit_point`     | `JsonOrbitPointAction`      | `x, y, radius, segments, clockwise`, `[interval]` | Walk the ring around (x,y). Default = a continuous orbit/kite. Optional **`interval`** (ms) makes it **stepped**: the bot parks on the current waypoint and advances one slot only every `interval` ms — a cadence-paced kite that holds threat between drags (e.g. Grobbulus's ~15s-per-Poison-Cloud rotation). `0`/absent = continuous (back-compat). |
| `stack_point`     | `JsonStackPointAction`      | `x, y, radius`, `hold` (bool), `z` (optional) | Move to (x,y) and stay within `radius` (tight raid stack). **`hold:true`** = once parked, *own the tick* (stand still) instead of yielding — stops a bot parked **off** the boss with no current job (e.g. a Gluth off-tank between chow waves) from falling through to generic combat and running at the boss. **`z`** = an explicit anchor height for an **elevated** spot (Thaddius add platforms): moves in 3D so the bot climbs instead of yielding on the floor below. Omit both for a plain ground stack. |
| `spread`          | `JsonSpreadAction`          | `radius`, `min_interval` (ms, default 3000) | Move away from the nearest **other ranged/healer** within `radius`. Ignores the melee/tank stack and repositions at most once per `min_interval` so casters aren't interrupted. Yields when clear. |
| `attack`          | `JsonAttackAction`          | `targets` (name/entry, string or array), `boss` (fallback, default file `boss`), `detect` (`threat`\|`nearest`), `select` (`lowest_hp`\|`nearest`), `sticky` (bool, default true), `max_hp_pct`, `max_range` | Pick one creature to attack — the lowest-HP / nearest match from `targets`, falling back to `boss` when none are alive. **`detect`** = `threat` scans the attacker/threat list (default); `nearest` scans nearby NPCs so **off-threat** objects are visible (Web Wrap cocoons, un-aggroed adds). **`select`** = `lowest_hp` (default when `detect:threat`) is "kill adds first"; `nearest` (default when `detect:nearest`) is closest-first. **`max_hp_pct`** / **`max_range`** (both default `0` = off) filter the candidate set to adds at/below that HP% and/or within that many yards — so `{targets:"zombie chow", max_hp_pct:10, max_range:35}` self-gates to the Decimate burn (chow are only candidates while at 5%, else it falls back to `boss`), no separate phase trigger needed. **`sticky`** (default `true`) keeps the bot on its current match until it dies — no cast-cancel thrash when two candidates' "best" flips tick-to-tick. **`sticky:false`** re-picks the best every tick: use it with `select:nearest` so a tank yanked to the other add by **Magnetic Pull** swaps to the now-nearest add instead of running back to its original target (Thaddius). Matches by name **or** entry id; yields when already on target. To just focus one creature (e.g. the boss), name it in `targets`. Merges the former `attack_target`, `attack_priority` (`threat`+`lowest_hp`) and `attack_nearest` (`nearest`+`nearest`). |
| `move_to_target`  | `JsonMoveToTargetAction`    | `target` (name/entry, string or array), `detect` (`nearest`\|`threat`, default **`nearest`**), `distance` (yd, yield threshold, default 0), `boss` (fallback, default file `boss`), `then` (swap-recovery set, string or array) | Path to a live named creature's **actual position** (the nearest match) via an **exact-waypoint** `MoveTo` — the generic "run to the add / boss" primitive, and the only reliable **elevated-platform climb**. The trap it sidesteps: a normal `MoveTo` to any `(x,y,z)` (fixed point *or* a live unit) routes through `SearchForBestPath`, which **discards your Z** and re-snaps the destination to the **shortest-path** surface — from the floor that's the hazard *under* the platform, so bots walk across it to the spot below the target and never climb (the Thaddius pull bug; confirmed in-game — bots stuck at z≈292 while the add sat at z≈312, then dragged it down once aggroed). `move_to_target` passes `exact_waypoint=true`, keeping the target's literal Z, so recast routes **up the ramp** to the platform poly (still a real navmesh path). `detect` defaults to **`nearest`** (a proximity scan) — unlike `attack` — because the use case is reaching something the bot doesn't threaten yet (the `.rjson pull`). **Reached-latch:** it *holds* the tick while climbing (so nothing drags it off the ramp), and the instant it gets within `distance` it **latches and yields every tick thereafter** — handing positioning to the in-combat AI even as the add moves (a healer's add walking, a tank thrown by Magnetic Pull). It re-arms only when the bot is **out of combat AND far** from the target (a wipe / fresh pull), so the latch survives the whole engagement. Latching on *reached* (not "in combat") matters: a bot flagged in combat early while still climbing keeps climbing instead of stalling. Emits a throttled `[RaidJson][move_to_target]` debug line per bot in `Playerbots.log` (`reached=1` = handed off; watch the bot's Z **rise toward the target's** during the climb). **`boss` fallback hazard:** when no named target is alive it falls back to `boss` (default = the **file boss**), and since the rule's trigger can stay active into later phases (e.g. `manual_engage` runs the whole encounter), that fallback makes the pull keep walking bots toward the *boss* after the add dies — on Thaddius this fought the polarity `stack_point` at equal relevance and produced a "run back and forth, can't hold the charge spot" bug. For a pure pull-to-add rule that should go **inert** once its add dies, set **`"boss": ""`** to disable the fallback (no target ⇒ yields). **`then` (Magnetic-Pull swap recovery):** the permanent in-combat latch is wrong for a tank that gets *flung off its platform* by a swap — the in-combat C++ `MoveTo` can't climb back, so it's stranded out of taunt range (Thaddius: seen at z≈338 above Stalagg) and its add runs loose. Set `then` to the add set the tank may be thrown across (e.g. `["stalagg","feugen"]`): after the **first** reach the rule re-paths to the **nearest** of `then` (the add it landed on, not its original), and the latch **re-arms in combat** when the bot is flung **> distance+40** away so it re-climbs via exact-waypoint, reaches, and hands back. The named `target` still drives the *first* climb so the initial role split holds; both reset on a real wipe (out of combat + far). Omit `then` (default) for healers/ranged/melee that never get pulled — they keep the permanent latch. |
| `tank_adds`       | `JsonTankAddsAction`        | `add`, `boss` (default file `boss`)       | Off-tank (assist-tank #0) gathers every living add named `add` and drags it onto the main tank / boss. attack → taunt → reposition. |
| `tank_swap`       | `JsonTankSwapAction`        | `aura` (debuff name), `stacks` (default 1), `boss` (default file `boss`), `detect` (`threat`\|`nearest`), `watch` (`victim`\|`maintank`) | **A16 stacking-debuff tank rotation** — a self-contained 2-tank swap. Wire it on **both** swap tanks (e.g. `maintank` and `offtank1`). For each: **if I'm the active tank** (the boss is on me) it holds the boss and *yields to my own rotation* so I tank + go ham; **if I'm the off tank** it **stands ready and does NOT attack the boss** (owns the tick, so no premature pull-aggro) until the watched tank carries `stacks`+ of `aura` (Mortal Wound, Crunch Armor, Gormok Impale, …), then it taunts and takes over. Taunt is `DoSpecificAction("taunt spell")` which **force-runs** the class taunt regardless of relevance, so a `suppress` of `"taunt spell"` (to stop a relieved tank auto-taunting back) doesn't block the swap. **`watch`** = `victim` (default): gate on the boss's **current victim** → symmetric ping-pong, each tank takes over when the other hits the cap (Gluth Mortal Wound, Kologarn, Festergut, AQ40). `maintank`: gate on the designated **main tank**, and only the **primary relief tank (assist #0)** acts, so one off-tank covers while the MT detoxes and the rest keep their own job. `detect:nearest` lets a swap tank that isn't currently on the boss still locate him. |
| `snare_area`      | `JsonSnareAreaAction`       | `spells` (array, ordered), `add` (name/entry), `target` (`self`\|`nearest`\|`leak`), `range`, `boss` (for `leak`) | Cast a control / AoE-threat spell on the adds while a **lower-priority** movement shape (`orbit_point` / `stack_point`) holds the path. Tries each spell in `spells` the bot **knows and has off cooldown**, in order, firing the first that lands — so one rule can list every class's option and each bot fires its own (the Four Horsemen "opening defensive" idiom; generic form of the C++ `CastZombieThreat`). **`target`** = `self` (cast on the bot for self/ground-centered AoE — Frost Nova, Consecration, D&D; requires ≥1 matching add within `range` so a cooldown isn't wasted), `nearest` (the nearest matching add within `range` of the bot), or `leak` (the add **nearest the boss** — the one about to reach it — within `range` of the boss). **Yields** when no add qualifies / nothing is castable, so it layers cleanly over the movement underneath. The Blink/Disengage escape is just this shape: `{spells:["Blink","Disengage"], target:"self"}` under an `adds_near {range:6, count:1}`. |
| `position_vs_boss`| `JsonPositionVsBossAction`  | `anchor` (`radial_out`\|`behind`\|`front`\|`left`\|`right`), `distance`, `angle_offset`, `only_if_closer`, `boss` | Move to a single spot defined **relative to the boss** (A3/A4). `radial_out` = straight out along the boss→bot bearing (back off wherever you stand); the facing modes = a bearing off the boss's **orientation** (`behind`=+π, `front`=0, `left`=+π/2, `right`=−π/2) plus `angle_offset` (extra radians). `only_if_closer:true` yields once already ≥ `distance` away (a "maintain range" check). Generic form of Grobbulus's move-away (`radial_out`+`only_if_closer`) and go-behind (`behind`+`angle_offset`); also Onyxia move-to-side, Yogg face-away, etc. |
| `timed_safe_zone` | `JsonTimedSafeZoneAction`   | `zones` (array of `[x,y]`), `pattern` (array of zone indices), `z`, `first_at` (ms), `interval` (ms), `hold` (bool), `cast_while_moving` (bool), `tolerance` (default 5.0) | **A6 eruption dance** as data. The room has fixed safe `zones`; on a deterministic clock one zone after another is the only safe spot. Predicts the current safe zone (`pattern[k]` where `k` counts eruptions from `first_at`/`interval`) and stands on it. The generic form of `HeiganDanceAction`. `hold:true` = own the tick even when parked (tight cadence, no casting); `hold:false` = yield once parked so rotations run between eruptions. `cast_while_moving:true` = while **en route**, yield the tick so the bot's rotation fires INSTANTS as it relocates (the engine refuses cast-time spells while moving, so only instants come out) — **requires a `suppress` rule** (below) zeroing the movement-hijackers, else they grab the yielded tick. Per-bot clock auto-anchors on first run and re-anchors after a long idle gap (the rule going dormant across the *other* phase), so gate each phase with its own rule + cadence. |

```json
{ "shape": "stack_point",     "params": { "x": 3272.49, "y": -3476.27, "radius": 4.0 }, "priority": 3 }
{ "shape": "spread",          "params": { "radius": 8.0, "min_interval": 3000 }, "priority": 2 }
{ "shape": "attack",          "params": { "targets": "anub'rekhan" }, "priority": 1 }
{ "shape": "attack",          "params": { "targets": "crypt guard", "boss": "anub'rekhan" }, "priority": 1 }
{ "shape": "attack",          "params": { "targets": 16486, "detect": "nearest", "boss": "maexxna" }, "priority": 2 }
{ "shape": "move_to_target",  "params": { "target": "stalagg", "detect": "nearest", "distance": 3.0 }, "priority": 3 }
{ "shape": "tank_adds",       "params": { "add": "crypt guard", "boss": "anub'rekhan" }, "priority": 2 }
{ "shape": "timed_safe_zone", "params": { "zones": [[2756.0,-3704.0],[2794.9,-3668.1]], "pattern": [3,2,1,0,1,2], "z": 276.54, "first_at": 7000, "interval": 4000, "hold": true }, "priority": 32 }
{ "shape": "orbit_point",     "params": { "x": 3281.23, "y": -3310.38, "radius": 35.0, "segments": 8, "clockwise": true, "interval": 15000 }, "priority": 3 }
{ "shape": "position_vs_boss","params": { "anchor": "radial_out", "distance": 18.0, "only_if_closer": true }, "priority": 2 }
{ "shape": "position_vs_boss","params": { "anchor": "behind", "distance": 24.0, "angle_offset": 0.3927 }, "priority": 2 }
{ "shape": "snare_area",      "params": { "spells": ["Frost Nova","Arcane Explosion","Frost Trap"], "add": "zombie chow", "target": "self", "range": 10.0 }, "priority": 2 }
{ "shape": "snare_area",      "params": { "spells": ["Concussive Shot","Cone of Cold"], "add": "zombie chow", "target": "leak", "boss": "gluth" }, "priority": 2 }
{ "shape": "tank_swap",       "params": { "aura": "mortal wound", "stacks": 3, "boss": "gluth", "detect": "nearest", "watch": "victim" }, "priority": 5 }
{ "shape": "tank_swap",       "params": { "aura": "crunch armor", "stacks": 3 }, "priority": 3 }
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

**Category token `@damage`** — instead of (or alongside) action names, list
`"@damage"` to zero **every non-healing damage cast** (the type filter
`dynamic_cast<CastSpellAction*> && !CastHealingSpellAction`, which a name list
can't express). This is the data form of the C++ DPS-clamp multipliers: pair it
with the `target_hp_ahead` trigger for a twin-add death-sync throttle (slow the
ahead add's DPS so the other catches up). Soft throttle — like the C++ original,
melee auto-attacks aren't suppressed, only casts.

**Grace window `after_ms`** (or `after_seconds`) — optional per suppress entry:
suppression activates only once the `trigger` has been **continuously active for
≥ N ms**. Leaves an opening window where the listed actions still fire normally,
then disables them for the rest of the phase. The grace timer resets whenever the
trigger goes inactive, so a multi-wave re-entry gets a fresh window. Typical use:
**Thaddius no-in-fight-taunts** — `{ trigger: "thaddius phase pet", after_ms: 5000,
actions: ["taunt", "taunt spell", "dark command", "hand of reckoning",
"righteous defense", "growl"] }`. Magnetic Pull cleanly transfers threat in the
boss script, so a re-taunt is unnecessary and a paladin's 30/40-yard taunt
(Hand of Reckoning / Righteous Defense) lets the MT steal back across platforms.
The 5s grace preserves the *initial* tank pickup at the pull, then disables every
class taunt for the rest of the add phase.

The primary use: let a movement shape **yield the tick for instant casts**
(`cast_while_moving`) without the eruption-dodge / reach / formation actions
grabbing the yielded tick and dragging the bot off its route. Pick the suppress
list deliberately — e.g. leave `"reach melee"` *un*-suppressed if melee still need
to close on a tanked boss during the phase. (This also lets a Level-1 port carry a
multiplier its C++ original relied on, like four_horsemen.)

> **Yielding shapes must suppress the C++ instance-strategy action that shares
> their trigger.** `.rjson on` strips the C++ instance strategy, but
> `PlayerbotAI::ApplyInstanceStrategies` **re-adds it on zone-in/reset** — so the
> C++ strategy is usually still loaded *alongside* json-raid. A shape that *holds*
> the tick masks this (it wins at its priority and the C++ action never runs). A
> shape that *yields* does not: the C++ action (e.g. `heigan dance` @ `ACTION_RAID+3`)
> catches the yielded tick and holds it, so the rotation never fires. Add that C++
> action's name to the `suppress` list. (Diagnose from `Playerbots.log`: the C++
> action's `- OK` count tracking your shape's `- FAILED`/yield count is the tell.)

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
  flight — dodge wherever the ice blocks land). The *movement* is bespoke. (Several
  special cases are now generic: a fixed-pattern timed safe zone like the **Heigan
  dance** is `timed_safe_zone`; a periodic *cast* you only need to pre-mitigate is
  `pre_cast_window`; and **Thaddius polarity** — once thought bespoke because it's
  "keyed on the bot's own debuff" — is `encounter_active` + `self_aura` driving a
  per-polarity `stack_point`, because the C++ already groups by *fixed* left/right
  anchors, not dynamic regroup.)
- **Vertical / jump transitions** (z-platform → floor drops via `JumpTo`, e.g.
  Thaddius's adds-platform → low-platform transition). No movement shape paths over
  a navmesh gap. Detection compounds it: a non-attackable boss with the adds dead
  has no threatener, so no `encounter_active` rule fires during the transition.
- Multi-actor relays / assignments (Four Horsemen corner rotation, Vashj/Kael,
  Yogg, Lich King, Mimiron, Razuvious mind-control).
- Anything reading boss script internals (channel state, `_currentSection`).
- **Cross-target / coordinated-kill HP comparisons** are *no longer* a hard limit:
  `target_hp_ahead` + the `@damage` suppress token express twin-add death-sync
  (Thaddius) as data.

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
