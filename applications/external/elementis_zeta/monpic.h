#pragma once
// Elementi pictures (mon.bin, contract section 9.4): front (enemy) and back (player) view of every species, 4 grays
// + mask, loaded on demand and freed by the caller. No furi.h (files through read_at(), no handle is kept).
#include <stdint.h>
#include <stdbool.h>
#include "blit.h"

typedef struct {
    uint8_t* blk; // owned heap block (pixels then mask); NULL = nothing loaded
    Blit b; // view into blk (masked sprite, ready for blit_masked / blit_auto)
} MonPic;

// Loads the picture of `species` (1-based id of species.toml): back = false front view (enemy / popup), true back
// view (the player's creature). Returns false (and leaves *p empty) when mon.bin is missing / invalid, the species
// has no art, or the heap is short. Heap use = the picture's bytes only (about 300-600 B).
bool monpic_load(MonPic* p, uint8_t species, bool back);

// Battle-size pictures of mon.bin (contract 10.1): entry kind of a species, or (BP_TRAINER) trainer picture `species`.
// Heap = the picture's bytes (one malloc, no handle kept). False: no art / missing file / no heap.
enum {
    BP_FRONT_MED = 0,
    BP_BACK_MED = 1,
    BP_FRONT_NAT = 2,
    BP_BACK_NAT = 3,
    BP_TRAINER = 4
};
bool monpic_load_kind(MonPic* p, uint8_t species, uint8_t kind);
void monpic_free(MonPic* p);

// Contract 65 (variocolor): the shiny look of a loaded picture = its gray levels 1 and 2 swapped IN PLACE (an involution: applying it twice gives the original back; white 0, the black outline / eyes 3 and the mask
// are untouched, so the silhouette stays readable on every platform and background). Header-inline: the battle plugin, the party / summary screen and the Registro each compile their own copy (no symbol, no
// resident code). Only the pixel plane (bpr * h bytes: 4 pixels of 2 bits per byte; the padding pixels of a row are level 0 and stay) is touched.
static inline void monpic_shiny(MonPic* p) {
    if(!p->blk) return;
    for(uint32_t i = 0, n = (uint32_t)p->b.bpr * p->b.h; i < n; i++) {
        const uint8_t b = p->blk[i], t = (uint8_t)((b ^ (b >> 1)) & 0x55);
        p->blk[i] = (uint8_t)(b ^ (t | (t << 1)));
    }
}
