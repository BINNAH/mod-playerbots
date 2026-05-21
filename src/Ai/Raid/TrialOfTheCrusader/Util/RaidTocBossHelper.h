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

    SPELL_FIRE_BOMB        = 66313,
    SPELL_FIRE_BOMB_AURA   = 66318,
    SPELL_SNOBOLLED        = 66406,
    SPELL_IMPALE           = 66331, // Gormok stacking bleed on the active tank
    SPELL_PARALYTIC_TOXIN  = 66823, // Acidmaw debuff: ramps to full paralysis (25N)
    SPELL_BURNING_BILE     = 66870, // Dreadscale debuff: neutralizes Paralytic Toxin
    SPELL_SURGE_OF_ADRENALINE = 68667, // Icehowl: granted the instant the charge starts (25N)
};

// Arena centre = Locs[LOC_CENTER] in core trial_of_the_crusader.h
constexpr float TOC_CENTER_X = 563.673f;
constexpr float TOC_CENTER_Y = 139.571f;
constexpr float TOC_CENTER_Z = 393.837f;

// Gormok the Impaler
constexpr float TOC_FIRE_BOMB_AVOID_RADIUS = 8.0f;
constexpr float TOC_FIRE_BOMB_FLEE_DISTANCE = 10.0f;
constexpr float TOC_SNOBOLD_SEARCH_RADIUS = 80.0f;
constexpr uint32 TOC_IMPALE_SWAP_STACKS = 3;   // don't bother swapping below this many stacks
constexpr uint32 TOC_IMPALE_SWAP_LEAD = 2;     // ...and only when the active tank leads me by this much
// Ranged/healers fan onto a ring — one unique, evenly spaced slot each — so
// Snobold Fire Bombs (and the worms' sprays/frontal effects) can't chain off a
// clump. Applies through Gormok and the worm phase.
constexpr float TOC_SPREAD_RADIUS = 18.0f;
constexpr float TOC_SPREAD_TOLERANCE = 4.0f;

// Acidmaw & Dreadscale. Slime Pools grow over their ~30s life, so react well
// before the visual edge and clear it with margin (it was the #1 phase-2 damage).
constexpr float TOC_SLIME_POOL_AVOID_RADIUS = 10.0f;
constexpr float TOC_SLIME_POOL_FLEE_DISTANCE = 14.0f;
constexpr float TOC_BILE_CURE_REACH = 4.0f;    // get this close to a Burning Bile carrier

// Icehowl: clear at least this far off the charge lane (Trample radius is 12y).
constexpr float TOC_ICEHOWL_CHARGE_CLEAR = 16.0f;

// 2*pi without pulling in <cmath>/M_PI at header scope.
constexpr float TOC_TWO_PI = 6.2831853071795862f;

#endif
