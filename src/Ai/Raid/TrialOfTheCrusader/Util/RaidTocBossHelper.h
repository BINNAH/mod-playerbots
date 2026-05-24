#ifndef _PLAYERBOT_RAIDTOCBOSSHELPER_H
#define _PLAYERBOT_RAIDTOCBOSSHELPER_H

constexpr uint32 TOC_MAP_ID = 649;

enum TocIDs
{
    NPC_GORMOK_THE_IMPALER = 34796,
    NPC_SNOBOLD_VASSAL     = 34800,
    NPC_FIRE_BOMB          = 34854,
    NPC_DREADSCALE         = 34799,
    NPC_ACIDMAW            = 35144,
    NPC_ICEHOWL            = 34797,
    NPC_SLIME_POOL         = 35176,

    NPC_JARAXXUS           = 34780,
    NPC_LEGION_FLAME       = 34784, // persistent ground-fire trail dropped under the Legion Flame target

    SPELL_FIRE_BOMB        = 66313,
    SPELL_FIRE_BOMB_AURA   = 66318,
    SPELL_SNOBOLLED        = 66406,
    SPELL_IMPALE           = 66331, // Gormok stacking bleed on the active tank
    SPELL_PARALYTIC_TOXIN  = 66823, // Acidmaw debuff: ramps to full paralysis (25N)
    SPELL_BURNING_BILE     = 66870, // Dreadscale debuff: neutralizes Paralytic Toxin
    SPELL_SURGE_OF_ADRENALINE = 68667, // Icehowl: granted the instant the charge starts (25N)

    // Lord Jaraxxus. Nether Power is a self-buff stacking +20% spell damage that
    // a mage's Spellsteal strips one stack at a time (and grants the mage its own
    // copy — the core handles that in boss_lord_jaraxxus.cpp SpellHit). Detected by
    // NAME because it uses per-difficulty ids (66228 / 67106 / 67107 / 67108).
    SPELL_NETHER_POWER     = 66228,
    SPELL_SPELLSTEAL       = 30449, // mage Spellsteal (cast by name "spellsteal")
};

// Arena centre = Locs[LOC_CENTER] in core trial_of_the_crusader.h
constexpr float TOC_CENTER_X = 563.673f;
constexpr float TOC_CENTER_Y = 139.571f;
constexpr float TOC_CENTER_Z = 393.837f;

// Gormok the Impaler
constexpr float TOC_FIRE_BOMB_AVOID_RADIUS = 8.0f;
constexpr float TOC_FIRE_BOMB_FLEE_DISTANCE = 10.0f;
constexpr float TOC_SNOBOLD_SEARCH_RADIUS = 80.0f;
constexpr float TOC_SNOBBLED_REACH = 6.0f;     // snobbled player runs to within this of Gormok so all can hit the snobold
constexpr uint32 TOC_IMPALE_SWAP_STACKS = 3;   // off-tank taunts once the active tank hits this many stacks
                                               // (and only while the off-tank itself is at 0 stacks)
constexpr uint32 TOC_IMPALE_BOP_STACKS = 8;    // pally-tank emergency: BoP-self to wipe the bleed at this many

// Ranged/healers keep this far apart so Snobold Fire Bombs (and the worms'
// sprays) can't chain. Spacing-based: a bot only steps out when it's actually
// clumped, so spreading never fights fire-dodging or stops it from DPSing.
constexpr float TOC_SPREAD_MIN_DIST = 8.0f;    // clumped if a fellow spreader is closer than this
constexpr float TOC_SPREAD_PUSH_DIST = 11.0f;  // how far to step off when clumped

// Acidmaw & Dreadscale. Slime Pools grow over their ~30s life, so react well
// before the visual edge and clear it with margin (it was the #1 phase-2 damage).
constexpr float TOC_SLIME_POOL_AVOID_RADIUS = 10.0f;
constexpr float TOC_SLIME_POOL_FLEE_DISTANCE = 14.0f;
constexpr float TOC_BILE_CURE_REACH = 8.0f;    // toxin'd: only need to reach the 10y cure radius, not hug the carrier

// Burning Bile is a 10y fire AoE around the afflicted player. Ranged/healers who
// don't need curing keep clear of it (the #1 phase-2 damage source).
constexpr float TOC_BILE_AVOID_RADIUS = 11.0f; // treat a Burning Bile carrier this close as dangerous
constexpr float TOC_BILE_AVOID_FLEE = 13.0f;   // step out to here

// Worm tank facing: each worm spews a frontal cone (Molten/Acid Spew) at its
// tank. The tank stands on the far side of the worm from the raid so the cone
// points away. Reposition only when the backline is in the worm's front
// hemisphere (dot of facing vs raid-direction > this) — gives ~90deg of
// hysteresis so the tank doesn't micro-dance.
constexpr float TOC_WORM_FACE_DANGER_DOT = 0.0f;  // raid within 90deg of the worm's facing = dangerous
constexpr float TOC_WORM_FACE_MELEE_GAP = 3.0f;   // stand this far past the worm's combat reach (stay in melee)
constexpr float TOC_WORM_MELEE_RANGE = 8.0f;      // members within this of the worm count as "on it" (skipped for backline centroid)

// Icehowl: clear at least this far off the charge lane (Trample radius is 12y).
constexpr float TOC_ICEHOWL_CHARGE_CLEAR = 16.0f;

// Lord Jaraxxus. Legion Flame drops a trail of persistent ground-fire NPCs under
// its target; everyone steps out of any patch that gets close. React a touch
// before the visual edge and clear it with margin (matches the fire-bomb dodge).
constexpr float TOC_LEGION_FLAME_AVOID_RADIUS = 7.0f;
constexpr float TOC_LEGION_FLAME_FLEE_DISTANCE = 12.0f;

// 2*pi without pulling in <cmath>/M_PI at header scope.
constexpr float TOC_TWO_PI = 6.2831853071795862f;

#endif
