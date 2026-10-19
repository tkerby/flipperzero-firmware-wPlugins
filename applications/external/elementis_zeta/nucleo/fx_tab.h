#pragma once
// Battle effects, the tables shared by the player (battle_fx.c), the baker (tools/fxbake/fxbake.c) and the host proof
// (tools/host_battle/fxcheck.c).  Contract 10.4.  No furi.h (host-compilable); `static` data: include it in ONE file per program.
#include <stdint.h>
#include "battle_fx.h"

// ---- durations (ms) of the effect kinds: the scene's timeline (AT_*) and the baked animations both follow these.
static const uint16_t FX_MS[FX_COUNT] = {
    [FX_NONE] = 0,      [FX_STAR] = 720,    [FX_STARS3] = 780, [FX_FLAME] = 900, [FX_DROPS] = 900, [FX_LEAVES] = 900,
    [FX_BUBBLES] = 860, [FX_BOLT] = 780,    [FX_RINGS] = 780,  [FX_ROCKS] = 900, [FX_SLASH] = 720, [FX_SWARM] = 840,
    [FX_UP] = 900,      [FX_DOWN] = 900,    [FX_ZZZ] = 900,    [FX_ORBIT] = 900, [FX_WAVE] = 900,  [FX_FANG] = 720,
    [FX_SPORE] = 800,   [FX_WHIFF] = 240,
};
static inline uint16_t fxt_duration(uint8_t kind) {
    return kind < FX_COUNT ? FX_MS[kind] : 0;
}

// ---- classes: the artwork is baked for a few target boxes; the player picks the nearest one and centres it on the real box
// (horizontally on the middle, vertically on the bottom edge = the creature's feet).
#define FXC_N 4 // width classes: 0 = the capture crystal's 18 x 20 box (stars only), 1..3 = a creature's box
static const uint8_t FXC_W[FXC_N] = {18, 32, 44, 54};
#define FXC_H 44      // creature boxes are baked 44 high (the enemy's picture is 42 rows, the player's 46)
#define FXC_SMALL_H 20 // class 0
// class of a real box of w x h px
static inline uint8_t fxt_class(int w, int h) {
    return h < 32 ? 0 : (w < 38 ? 1 : (w < 49 ? 2 : 3));
}

// ---- time: the key n covers [n * FX_KS[kind], (n + 1) * FX_KS[kind]) ms (identical neighbours are merged into one key) and is rendered
// at the middle of that interval, fxt_sample(); the player shows, at time t, the last key whose start is <= t.  The steps divide the
// painters' discrete toggles (star blink 80, stars3 70, flame sway 90, bolt phases 80 / flash 60 + 260 + 310, whiff steps 30), so a
// toggle never falls inside an interval.
static const uint8_t FX_KS[FX_COUNT] = {
    [FX_STAR] = 40,  [FX_STARS3] = 35, [FX_FLAME] = 45, [FX_DROPS] = 30, [FX_LEAVES] = 30, [FX_BUBBLES] = 30, [FX_BOLT] = 10,
    [FX_RINGS] = 40, [FX_ROCKS] = 30,  [FX_SLASH] = 30, [FX_SWARM] = 30, [FX_UP] = 30,     [FX_DOWN] = 30,    [FX_ZZZ] = 30,
    [FX_ORBIT] = 30, [FX_WAVE] = 30,   [FX_FANG] = 40,  [FX_SPORE] = 30, [FX_WHIFF] = 30,
};
// The time the painter is evaluated at for the interval that starts at `t0` (a multiple of `step`).
static inline uint16_t fxt_sample(uint8_t kind, unsigned t0, unsigned step) {
    unsigned t = t0 + step / 2, d = fxt_duration(kind);
    return (uint16_t)(t < d ? t : d - 1);
}
// Classes (1 = bit 0 .. 3 = bit 2) baked per kind; the others alias the nearest baked one (a kind whose look hardly depends on the box's
// width keeps one class).  Class 0 (the crystal's box) exists for STAR and STARS3 only.
static const uint8_t FX_WMASK[FX_COUNT] = {
    [FX_STAR] = 7,  [FX_STARS3] = 7, [FX_FLAME] = 2, [FX_DROPS] = 2, [FX_LEAVES] = 2, [FX_BUBBLES] = 2, [FX_BOLT] = 2,
    [FX_RINGS] = 2, [FX_ROCKS] = 2,  [FX_SLASH] = 2, [FX_SWARM] = 2, [FX_UP] = 2,     [FX_DOWN] = 2,    [FX_ZZZ] = 2,
    [FX_ORBIT] = 2, [FX_WAVE] = 2,   [FX_FANG] = 7,  [FX_SPORE] = 2, [FX_WHIFF] = 2,
};
#define FXP_STEP 10 // projectile head frames (the rock turns): one per 10 ms over the 300 ms flight (the sprite is rendered at the middle)
#define FXP_MS FX_PROJ_MS

// ---- fx.bin (little-endian; built by tools/fxbake, documented in contract 10.4)
//   0  char[4] "NFX1" | u8 version 1 | u8 FX_COUNT | u8 FXC_N | u8 FXF_ENT | u16 entries | u16 reserved | u8 W classes[4]
//  16  directory: FXF_NENT entries of FXF_ENT bytes: u32 off (file offset of the animation = its key table) | u16 total (bytes of frame
//      data) | u16 maxf (largest frame) | u8 nkeys (0: no animation) | 3 pad.  Index: fxt_index() (kind, big, dir, class) for the
//      effects over a box, fxt_pindex() for a projectile's sprites.  Entries that are not baked (a kind that has one class only)
//      point at the nearest baked class; identical animations are stored once.
//  anim: nkeys x { u16 t_ms; u16 off } (off from the start of the frame data) then the frames
//  frame: u8 flags (bit 0 = invert the target box first; bits 1..3 = number of pieces) | pieces
//  piece: s8 dx (from the box's middle column) | s8 dy (from its bottom row; a projectile: from the head) | u8 w | u8 h | u8 tr |
//         h rows of runs: one byte (code << 6 | (len - 1)), code 0 transparent / 1 white (level 0) / 3 black (level 3); the runs of a row add up to w
//  tr: projectile pieces only, 1..3 = a trail disc (the player moves it back along the flight), 0 = the head
#define FXF_HDR 16
#define FXF_ENT 12
#define FXF_MAXKEYS 63
#define FXF_NMAIN (FX_COUNT * 2 * 2 * FXC_N)
#define FXF_NENT (FXF_NMAIN + FX_COUNT * 2)
static inline unsigned fxt_index(unsigned kind, unsigned big, int dir, unsigned cls) {
    return ((kind * 2 + (big & 1)) * 2 + (dir > 0 ? 1u : 0u)) * FXC_N + cls;
}
static inline unsigned fxt_pindex(unsigned kind, int right) {
    return FXF_NMAIN + kind * 2 + (right ? 1u : 0u);
}
