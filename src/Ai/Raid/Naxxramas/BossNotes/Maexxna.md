# Maexxna — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_maexxna.cpp` and `creature_template`
(world DB), verified 2026-05-26. **Verify IDs against the live DB/source before
trusting** (creature_template can drift; raid debuffs sometimes have per-difficulty
spell ids — see note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Maexxna` |
| Boss entry | `15952` |
| Map | Naxxramas, `533` |
| Boss spawn anchor | `3511.38, -3921.58, 299.51` (o `1.92`) |
| Leash | evades if >100y from `3486.6, -3890.6, 291.8` |

## Adds

| Name | Entry | When | Count 10 / 25 |
|------|-------|------|---------------|
| `Web Wrap` (cocoon) | `16486` | Every 40s (first 20s), 2s after knockback | **1 / 2** |
| `Maexxna Spiderling` | `17055` | Every 40s (first 30s) | 8 / 8 (same count) |

**Web Wrap mechanics:** Maexxna selects 1 (10-man) or 2 (25-man) random non-tank,
non-already-webbed players, knocks them back to one of 7 fixed wall positions
(`PosWrap[0..6]`, all z≈320 along the east-north wall), applies a 5s pacify-silence
(`28618`), then 2s later casts the stun (`28622`) and summons the cocoon NPC (`28627`).
The tank (`GetVictim()`) is always excluded from Web Wrap targeting. Killing the
cocoon NPC frees the wrapped player (removes `28622` + `28627` auras). If the
wrapped player dies the cocoon self-destructs via `52512`.

**Spiderlings:** 8 summoned at Maexxna's feet each wave; `SetInCombatWithZone()` on
spawn so they immediately attack random targets.

## Spells

| Spell | ID | Cadence | Notes |
|-------|----|---------|-------|
| Web Wrap knockback | — | 20s first, 40s repeat | Code path (no direct spell id for the knockback itself) |
| Web Wrap Pacify-Silence | `28618` | On Web Wrap cast | 5s, hits the chosen targets |
| Web Wrap Stun | `28622` | 2s after knockback | Applied by the victim on themselves via the cocoon NPC sequence |
| Web Wrap Summon (cocoon) | `28627` | On 2nd tick of periodic aura | Creates entry `16486` |
| Web Wrap Kill Webs | `52512` | On victim death | Cocoon NPC self-destructs |
| Web Spray | `29484` | 40s first, 40s repeat | **Raid-wide stun.** 25-man: hits all. Healers stunned too. |
| Poison Shock | `28741` | 10s first, 10s repeat | Tank only (CastSpell on victim) |
| Necrotic Poison | `54121` | 5s first, 30s repeat | Tank only; healing debuff |
| Frenzy | `54123` | Once, at <30% HP | Cast via 1s health-check loop; loop breaks on fire, so it fires once and stays |

> **Difficulty-id note:** Maexxna uses the **same spell IDs on both 10-man and
> 25-man.** The only `RAID_MODE` difference is the Web Wrap target count (1 vs 2).
> All spell IDs are safe to gate by id or name on the user's active 25-man path.
> (This is *not* universally true for Naxx debuffs — keep gating phases by aura NAME
> as a habit.)

## Phase structure

Maexxna is a **single-phase tank-and-spank** with two periodic hazard cycles running
throughout. There is no balcony phase, no threat reset, no movement phase.

- **T+0:** Engage. Necrotic Poison (5s), Poison Shock (10s) begin.
- **T+20s:** First Web Wrap — 1 or 2 non-tank players knocked to walls, stunned 2s
  later; DPS must kill their cocoon(s) to free them.
- **T+30s:** First Spiderling wave (8). Subsequent waves every 40s.
- **T+40s:** First Web Spray — **raid-wide stun**. Also first Web Wrap repeat at T+60s,
  then both align at T+120s (both events fire on the same tick at ~2 min). 
- **<30% HP:** Frenzy applies, permanently increasing damage/attack speed. After this
  point Web Spray is lethal without pre-cast defensive cooldowns on the tank.

## Implications for bot AI / strategy authoring

- **Current `maexxna.json` summary:**
  1. Non-tanks (role `nontank`) apply `rear flank` + `avoid aoe` (replaces the C++ MaexxnaTrigger's non-tank gate).
  2. DPS use `attack` with `detect:nearest` targeting entry `16486` at priority +2 over default attacker selection — this swaps them off Maexxna onto the cocoon immediately when one spawns, then they fall back to the boss when it dies.
  3. A `pre_cast_window` trigger watches for Web Spray (`29484`) on a 40s interval with a 4s lead and 6s tail, gated to `maintank,healer` role and `require_aura:frenzy` — fires tank externals and defensives (Hand of Sacrifice, Guardian Spirit, Shield Wall, Icebound Fortitude, Survival Instincts, Divine Protection) only after Frenzy is up.

- **Why `detect:nearest` for the cocoon is non-negotiable:** The Web Wrap cocoon
  (`16486`) spawns at a wall position and is never on Maexxna's threat list. The
  default `find target` value in playerbots only sees mobs this bot threatens, so it
  reads null for the cocoon entirely. `attack` with `detect:nearest` does a nearby-NPC
  scan by entry, bypassing the threat list. This is the defining reason that detection
  mode exists in the json-raid attack shape.

- **Web Spray stuns healers — reactive healing fails:** Web Spray (`29484`) hits the
  full raid including every healer simultaneously. A healer who tries to cast *after*
  the stun lands cannot. The only viable pattern is `pre_cast_window` with enough lead
  time to get a cast off before the stun. The current 4s lead / 6s tail window is
  appropriate. Without `require_aura:frenzy`, the same defensives would fire every 40s
  from T+40s onward; the `require_aura` gate reserves them for the genuinely lethal
  Frenzied Sprays.

- **Web Wrap timing overlap at T+120s:** Both the 40s Web Wrap and 40s Web Spray
  events can fire on the same server tick at the ~2-minute mark. At <30% HP, this
  means the tank is getting a Frenzied Spray stun at the same time 2 players are
  being knocked to walls. The `pre_cast_window` fires before this window; healing bots
  already stunned cannot compensate reactively. This is the most dangerous recurring
  moment in the fight — no additional strategy action needed, but worth knowing if
  tuning healer cooldown actions.

- **Spiderlings are threat-visible (zone combat):** Spiderlings call `SetInCombatWithZone()`
  and `AttackStart(random)` on spawn, so they enter every bot's threat/attacker list
  immediately. Generic `find target` and `tank_adds` work on them without any
  `detect:nearest` workaround. The current JSON does not assign a bot to tank
  Spiderlings — they pile onto whoever they choose. This is a known gap; adding a
  `tank_adds` rule for entry `17055` or by name would let an off-tank gather them.

- **Poison Shock / Necrotic Poison — tank healing load:** Necrotic Poison (`54121`)
  is a healing-reduction debuff on the tank every 30s. Poison Shock (`28741`) hits
  every 10s. Combined with Frenzy melee, tank healing demand spikes sharply sub-30%.
  Healer bot actions from the `pre_cast_window` help, but dispel/cleanse for
  Necrotic Poison is not currently in the JSON — a gap if tank healers aren't keeping
  up.

- **Aura detection by name:** Although Maexxna's spell IDs are identical on 10/25,
  the general habit of gating by aura name (`require_aura:frenzy`, not by id `54123`)
  is correct and consistent with Naxx conventions. Keep it.

- **The human is never a required slot** — all Web Wrap target handling, cocoon DPS,
  and tank cooldown coverage must be fully covered by bots. (memory: human-never-required.)

- **No `find target` issues during combat:** Unlike Noth's balcony phase, Maexxna
  never goes non-selectable or clears threat. She is always on every bot's threat list
  while alive. `encounter_active` on `boss:maexxna` is reliable throughout the fight.
