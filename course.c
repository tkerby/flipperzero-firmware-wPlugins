#include "course.h"
#include <math.h>

#define MOVER_PERIOD 120 // ticks for a full back-and-forth

// --- 1. Goblin Green ----------------------------------------------------
static const Rect h1_walls[] = {
    {58, 24, 4, 26},
};
static const Thing h1_things[] = {
    {ThingTree, 30, 18, 0, 0, 0},
    {ThingTree, 32, 54, 0, 0, 0},
    {ThingTree, 92, 20, 0, 0, 0},
    {ThingTree, 90, 52, 0, 0, 0},
};

// --- 2. Docking Bay 9 ----------------------------------------------------
static const Rect h2_walls[] = {
    {58, 10, 4, 32},
    {96, 30, 4, 33},
};
static const Thing h2_things[] = {
    {ThingConveyor, 20, 50, 70, 12, 0}, // pushes right along the bottom
    {ThingPod, 80, 20, 0, 0, 0},
    {ThingPod, 112, 44, 0, 0, 0},
};

// --- 3. Wizard's Spiral --------------------------------------------------
static const Rect h3_walls[] = {
    {30, 18, 62, 3}, // top
    {30, 18, 3, 24}, // left, open at the bottom
    {89, 18, 3, 34}, // right
    {45, 49, 47, 3}, // bottom, open at the left
    {48, 30, 3, 22}, // the hook: up from the bottom...
    {48, 30, 30, 3}, // ...and across, leaving the inner room open on the right
};
static const Thing h3_things[] = {
    {ThingTree, 14, 18, 0, 0, 0},
    {ThingTree, 110, 58, 0, 0, 0},
    {ThingSlime, 33, 22, 14, 10, 0},
};

// --- 4. Black Hole Bend --------------------------------------------------
static const Rect h4_walls[] = {
    {40, 10, 3, 16},
    {86, 46, 3, 17},
};
static const Thing h4_things[] = {
    {ThingWell, 64, 36, 24, 140, 0},
    {ThingPod, 20, 20, 0, 0, 0},
    {ThingPod, 108, 50, 0, 0, 0},
};

// --- 5. Dragon's Hoard ---------------------------------------------------
static const Rect h5_walls[] = {
    {100, 10, 3, 24},
};
static const Thing h5_things[] = {
    {ThingLava, 54, 10, 14, 19, 0}, // river, bridge at y 29..42
    {ThingLava, 54, 43, 14, 20, 0},
    {ThingMover, 76, 12, 8, 42, 1}, // the dragon patrols behind the bridge
    {ThingBumper, 110, 20, 3, 0, 0}, // the hoard
    {ThingBumper, 118, 30, 3, 0, 0},
    {ThingTree, 24, 16, 0, 0, 0},
    {ThingTree, 20, 56, 0, 0, 0},
};

// --- 6. Tractor Beam -----------------------------------------------------
static const Rect h6_walls[] = {
    {44, 24, 4, 20},
    {84, 10, 4, 24},
};
static const Thing h6_things[] = {
    {ThingWind, 0, -11, 0, 0, 0}, // pulls up
    {ThingBumper, 66, 50, 3, 0, 0},
    {ThingSlime, 96, 40, 30, 22, 0},
    {ThingPod, 24, 18, 0, 0, 0},
};

// --- 7. Faerie Ring ------------------------------------------------------
static const Thing h7_things[] = {
    {ThingBumper, 80, 36, 2, 0, 0},
    {ThingBumper, 75, 48, 2, 0, 0},
    {ThingBumper, 64, 53, 2, 0, 0},
    {ThingBumper, 53, 48, 2, 0, 0},
    {ThingBumper, 48, 36, 2, 0, 0},
    {ThingBumper, 53, 24, 2, 0, 0},
    {ThingBumper, 64, 19, 2, 0, 0},
    {ThingBumper, 75, 24, 2, 0, 0},
    {ThingTree, 18, 16, 0, 0, 0},
    {ThingTree, 110, 56, 0, 0, 0},
    {ThingTree, 108, 16, 0, 0, 0},
    {ThingSlime, 92, 26, 18, 20, 0},
};

// --- 8. Reactor Core -----------------------------------------------------
static const Rect h8_walls[] = {
    {30, 10, 3, 53}, // sealed airlock: the only way out is the portal
    {70, 22, 3, 12},
};
static const Thing h8_things[] = {
    {ThingPortal, 16, 18, 1, 0, 0}, // exits right out of the mate
    {ThingPortal, 46, 36, 1, 0, 0},
    {ThingBumper, 88, 40, 4, 0, 0}, // the core
    {ThingLava, 98, 50, 28, 12, 0}, // coolant
    {ThingConveyor, 50, 12, 40, 8, 0}, // pushes right along the top
    {ThingMover, 100, 24, 6, 14, 1},
    {ThingPod, 60, 56, 0, 0, 0},
};

// --- 9. The Rift ---------------------------------------------------------
static const Rect h9_walls[] = {
    {42, 10, 3, 40}, // lane 1 | 2, gap at the bottom
    {84, 22, 3, 41}, // lane 2 | 3, gap at the top
};
static const Thing h9_things[] = {
    {ThingWell, 64, 36, 18, 120, 0},
    {ThingLava, 88, 32, 22, 8, 0},
    {ThingPortal, 24, 54, 1, 0, 0}, // shortcut...
    {ThingPortal, 98, 14, 1, 0, 0}, // ...that drops you above the lava
    {ThingMover, 50, 54, 6, 24, 0},
    {ThingTree, 14, 36, 0, 0, 0},
    {ThingPod, 114, 24, 0, 0, 0},
};

#define N(a) (sizeof(a) / sizeof((a)[0]))

const Hole holes[HOLE_COUNT] = {
    {"Goblin Green",
     "LOST",
     ThemeFantasy,
     2,
     12,
     36,
     112,
     36,
     h1_walls,
     N(h1_walls),
     h1_things,
     N(h1_things)},
    {"Docking Bay 9",
     "LOST",
     ThemeSciFi,
     3,
     12,
     20,
     112,
     20,
     h2_walls,
     N(h2_walls),
     h2_things,
     N(h2_things)},
    {"Wizard's Spiral",
     "LOST",
     ThemeFantasy,
     3,
     12,
     56,
     60,
     41,
     h3_walls,
     N(h3_walls),
     h3_things,
     N(h3_things)},
    {"Black Hole Bend",
     "SPAGHETTIFIED",
     ThemeSciFi,
     3,
     14,
     56,
     112,
     18,
     h4_walls,
     N(h4_walls),
     h4_things,
     N(h4_things)},
    {"Dragon's Hoard",
     "DRAGON FIRE",
     ThemeFantasy,
     4,
     12,
     36,
     114,
     52,
     h5_walls,
     N(h5_walls),
     h5_things,
     N(h5_things)},
    {"Tractor Beam",
     "LOST",
     ThemeSciFi,
     3,
     12,
     56,
     112,
     56,
     h6_walls,
     N(h6_walls),
     h6_things,
     N(h6_things)},
    {"Faerie Ring", "LOST", ThemeFantasy, 3, 12, 36, 64, 36, NULL, 0, h7_things, N(h7_things)},
    {"Reactor Core",
     "MELTDOWN",
     ThemeSciFi,
     4,
     12,
     56,
     112,
     18,
     h8_walls,
     N(h8_walls),
     h8_things,
     N(h8_things)},
    {"The Rift",
     "INTO THE VOID",
     ThemeSciFi,
     4,
     14,
     14,
     114,
     54,
     h9_walls,
     N(h9_walls),
     h9_things,
     N(h9_things)},
};

Rect mover_rect(const Thing* t, uint32_t tick) {
    float phase = (float)(tick % MOVER_PERIOD) / (float)MOVER_PERIOD;
    float s = 0.5f - 0.5f * cosf(phase * 6.2831853f);
    int16_t off = (int16_t)(s * t->b + 0.5f);
    Rect r = {t->x, t->y, t->a, t->a};
    if(t->c) {
        r.y += off;
    } else {
        r.x += off;
    }
    return r;
}

bool rect_contains(const Rect* r, float x, float y) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}
