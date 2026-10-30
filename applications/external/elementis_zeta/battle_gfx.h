#pragma once
// Battle scene drawing helpers (contract section 10): isometric platforms, the 3x5 digit font of the plates, HP bar,
// picture loading from mon.bin. No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "blit.h"
#include "monpic.h"
#include "hud_gfx.h" // tiny_draw / tiny_width / hpbar_draw (plate pieces, also used by the party screen)

// ---- mon.bin entries (contract 10.1): BP_FRONT_MED .. BP_TRAINER are in monpic.h
// Loads the battle picture `kind` (BP_*) of `species`, or (kind == BP_TRAINER) trainer picture `k` (= monpic_load_kind).
// Heap = the picture's bytes (one malloc, no handle kept). False: no art / missing file / no heap.
bool bpic_load(MonPic* p, uint8_t species, uint8_t kind);

// ---- platform: iso ellipse slab (white centre, level-1 rim, level-2 side band, black outline, level-2 contact
// shadow = tools/battle_mock.py platform() + shadow()), pre-rendered into one masked picture (heap about 300-800 B).
typedef struct {
    uint8_t* blk;
    Blit b;
    int8_t ox, oy; // pixel of the picture that is the platform centre
} Plat;
bool plat_build(Plat* p, int rx, int ry, int thick);
void plat_free(Plat* p);
void plat_draw(
    LvlBuf lvl,
    Plat* p,
    int cx,
    int cy); // (no mask since contract 67: level 0 is transparent; contract 82: the picture is the left half, drawn twice)
