# Instructor Razuvious — boss notes

Reference facts for authoring a `json-raid` strategy. Sourced from the core script
`src/server/scripts/Northrend/Naxxramas/boss_razuvious.cpp`,
`mod-playerbots/src/Ai/Raid/Naxxramas/Action/RaidNaxxActions_Razuvious.cpp`, and
`creature_template` / `creature_template_spell` (world DB), verified 2026-05-26.
**Verify IDs against the live DB/source before trusting** (creature_template can
drift; see difficulty note below).

## Identity

| Thing | Value |
|-------|-------|
| Boss name | `Instructor Razuvious` |
| Boss entry (10-man) | `16061` |
| Boss entry (25-man) | `28357` |
| Boss entry Image | `37853` |
| Map | Naxxramas, `533` |
| Script | `boss_razuvious` |

> **Two boss entries.** The DB has `16061` (10-man) and `28357` (25-man) as
> separate `creature_template` rows. There are also `(1)` variants (`29940`,
> `29941`) — these appear to be heroic/secondary copies. For 25-man progression
> (difficulty 1), the active entry is `28357`.

## Adds — Death Knight Understudies

| Name | Entry (10-man) | Entry (25-man) | Count | Role |
|------|----------------|----------------|-------|------|
| `Death Knight Understudy` | `16803` | `29941` | 2 (10) / **4 (25)** | MC tank relay |

Understudies are summoned by `SpawnHelpers()` at fixed spawn points around the
room before combat starts. In 10-man, two spawn; in **25-man, four spawn**.
On engagement (`JustEngagedWith`), all summons enter combat via
`summons.DoZoneInCombat()`. The boss's `DamageTaken` hook credits damage dealt
by a controlled Understudy to player damage (for percent-health/kill-credit
accounting).

### Understudy abilities (from `creature_template_spell`, verified in DB)

| Spell ID | Name | Use |
|----------|------|-----|
| `29060` | Taunt | Forces Razuvious to attack this Understudy; triggers `SAY_TAUNTED` on Razuvious |
| `29061` | Bone Shield | Protective buff cast on the Understudy immediately before Taunt to absorb the incoming spike |
| `61696` | Blood Strike | Melee damage ability used while in melee range of Razuvious |

The C++ AI action (`RazuviousUseObedienceCrystalAction`) drives the controlled
Understudy through: move-to-target → cast Bone Shield (29061) → cast Taunt
(29060) → cast Blood Strike (61696) when in range. It also monitors aura
duration ("force obedience" / "mind control") and suppresses Taunt near
expiry to avoid a gap.

## Spells (Razuvious himself)

| Spell | ID | Timer | Notes |
|-------|----|-------|-------|
| Unbalancing Strike | `26613` | Every 20s | Heavy melee hit on the current tank; the primary reason normal players cannot tank Razuvious — it leaves a stacking armor debuff making follow-up hits lethal. |
| Disrupting Shout | `55543` | Every 15s | AoE silence + breaks active MC on Understudies. This is the core mechanic forcing the MC relay swap. |
| Jagged Knife | `55550` | Every 10s | Ranged throw at a random player within 45y. Physical damage. |
| Hopeless | `29125` | On death | Cast on self at death; kills remaining Understudies. |
| Taunt (from Understudy) | `29060` | — | Razuvious reacts to this with `SAY_TAUNTED` and turns on the new Understudy. |

> **Difficulty-id note:** Razuvious scales by **separate `creature_template`
> entry** (10-man `16061` vs 25-man `28357`), not by `RAID_MODE` modifier on a
> single script. Spell ids appear shared between difficulties based on the script
> enum — the same four spell IDs are referenced regardless of `Is25ManRaid()`.
> The only `Is25ManRaid()` branch in the script controls how many Understudies
> spawn. Detecting his abilities by id or aura NAME is safe on the 25-man path.
> (Keep gating phases by aura NAME for any aura you detect on other units — see
> memory: raid-debuff-difficulty-ids.)

## Phase structure

Razuvious has **no phase transitions** — he is a single-phase tank-and-spank
with a relay-tank mechanic. He does not move between phases, does not go
non-selectable, and does not have separate boss states. All combat is continuous:

- **MC relay loop:** Two (or four on 25-man) Understudies are available. A
  Priest Mind Controls one (or in 10-man uses a Spellclick orb — see below),
  positions it in melee, casts Bone Shield → Taunt → Blood Strike. Every 15s
  **Disrupting Shout** breaks all active MC. The next Priest must immediately
  MC a fresh Understudy and re-establish the tank before Razuvious turns on the
  raid. With four Understudies on 25-man, two Priests can alternate, giving more
  buffer time between Disrupting Shout and the next Taunt.
- **Unbalancing Strike:** Hits the current Understudy-tank every 20s; because
  Understudies are more durable than players (and armor-debuff stacks reset when
  the tank swaps), this is manageable.
- **Jagged Knife:** Random ranged target every 10s; healers/ranged need to
  anticipate burst.

### 10-man vs 25-man mechanical difference

| | 10-man | **25-man (active)** |
|-|--------|---------------------|
| Understudies | 2, controlled via Spellclick orb | **4, controlled via Priest Mind Control** |
| Tank role | Any tank bot clicks the orb | **Priest bots MC an Understudy** |
| Raid DPS window | Same boss, same hp scaling | Same |

On 10-man the Understudies have Spellclick (orb) interaction and the C++ AI
has bots click by spawn ID (`spawnId 128352` / `128353`). On **25-man the
action instead scans the attackers list for `"death knight understudy"` and
calls `botAI->CastSpell("mind control", target)`** — so 25-man relies entirely
on a bot Priest successfully landing and driving Mind Control.

## *** HARD LIMIT — stays in C++, not data-expressible ***

**This entire encounter is on the canonical hard-limits list and must remain
in C++.** The MC relay is a multi-actor, multi-step stateful sequence:

1. Identify a live uncharm'd Understudy from the attackers list.
2. Cast Mind Control on it (requires a Priest bot, line-of-sight, and a
   successful spell cast with potential resist).
3. Drive the charmed Understudy as a secondary unit: move it to melee range,
   then cast Bone Shield → Taunt → Blood Strike on a specific sequence and
   timing.
4. Monitor the MC aura duration; suppress Taunt when nearly expired to avoid
   wasted GCDs.
5. On Disrupting Shout (MC break), repeat from step 1 — rotate to the next
   Understudy.

No `json-raid` shape/action primitive covers "cast spell A on NPC B, then
drive NPC B as a secondary unit and fire its abilities." This is equivalent
in complexity to Yogg-Saron (Tentacle handling), Mimiron (phase switching),
and Lich King (Defile avoidance/soul phase) — all of which are hard-C++ by
design.

**Open question for the AI:** It is not confirmed whether mod-playerbots can
actually drive a bot's own charmed/Mind-Controlled unit through the action
system the way `RazuviousUseObedienceCrystalAction` attempts. The action calls
`bot->GetCharm()` and then directly calls `charm->GetMotionMaster()`,
`charm->GetAI()->AttackStart()`, and `charm->CastSpell()` — bypassing the
normal bot action loop entirely. Whether this works correctly in-game
(especially with the Disrupting Shout break/re-acquire loop) has not been
verified and should be tested before trusting the 25-man AI path.

## Implications for bot AI / strategy authoring

- **Hard limit; no JSON strategy.** No `json-raid` strategy exists for this
  encounter and none should be authored. The MC relay mechanic is inexpressible
  in data. The existing C++ AI in `RaidNaxxActions_Razuvious.cpp` is the
  permanent solution. Treat this like Four Horsemen, KT, or Ulduar final bosses.

- **Existing C++ AI summary:** Two triggers + one multiplier drive everything.
  - `RazuviousTankTrigger` fires for Priests (25-man) or tanks (10-man) when
    Razuvious is alive → calls `razuvious use obedience crystal` action.
  - `RazuviousNontankTrigger` fires for non-Priests (25-man) or non-tanks (10-man)
    → calls `razuvious target` action (attack Razuvious directly; tanks target
    the Understudy).
  - `InstructorRazuviousGenericMultiplier` sets `neglect threat = true` for the
    whole fight and gates out generic DPS-assist, tank-assist, and taunt actions
    so they don't interfere with the Understudy-targeting logic.

- **`find target` is threat-based.** `UpdateBossAI()` uses
  `AI_VALUE2("find target", "instructor razuvious")` — Razuvious must be on the
  bot's threat list for detection. If a bot has zero threat (e.g. a Priest who
  has only cast Mind Control, never dealt damage), the helper may return null and
  the trigger won't fire. (memory: findtarget-is-threat-based.)

- **Aura detection by NAME.** If any aura check is added (e.g. detecting
  Disrupting Shout on bots), use `botAI->GetAura("disrupting shout", unit)` —
  not spell id — in case per-difficulty aura IDs differ.
  (memory: raid-debuff-difficulty-ids.)

- **Human never required.** The Priest MC role must be filled by bot Priests.
  Skip `GET_PLAYERBOT_AI == null` members when assigning MC slots so the human
  player is never a required actor. (memory: human-never-required.)

- **25-man Priest dependency.** The 25-man path requires at least one (ideally
  two) Priest bots. If the raid comp has no Priest, the encounter cannot be
  completed as scripted — Understudies will never be controlled and Razuvious
  will turn on the raid immediately. This is a composition hard requirement, not
  something the AI can route around.

- **Disrupting Shout timing window.** 15s between casts means the Priest bot
  has a narrow re-MC window after each break. The action already handles this
  with duration checks, but real latency + bot action tick rate means the
  Understudy may drop tank for 1-2 GCDs per cycle. Healers need to cover the
  spike. Worth noting if tuning healer priorities for this room.
