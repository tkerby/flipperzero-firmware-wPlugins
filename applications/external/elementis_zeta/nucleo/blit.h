#pragma once
// Sprite blitting into the 2-bit level buffer. No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "lvl.h" // LvlBuf: the packed 2 bpp level buffer + lvl_* pixel helpers

typedef struct {
    const uint8_t* pix;  // 2 bpp, 4 px/byte, pixel x at bits (x&3)*2
    const uint8_t* mask; // 1 bpp LSB-first, nullable
    uint8_t w, h;
    uint8_t bpr;  // (w+3)/4
    uint8_t mbpr; // (w+7)/8
    uint8_t flip; // 1: drawn mirrored left <-> right (source column w-1-k lands on destination column k); the top-left
                  // stays where it would be unmirrored. Set by bank_view / hero_view for the folded directions.
} Blit;

// Level (0..3) of source pixel (sx, sy) of a sprite view.
static inline uint8_t blit_px(const Blit* v, int sy, int sx) {
    return (uint8_t)((v->pix[sy * v->bpr + (sx >> 2)] >> ((sx & 3) * 2)) & 3);
}

// Unmasked: level 0 is transparent; zero source bytes are skipped.
void blit(LvlBuf lvl, const Blit* v, int x, int y);
// Masked: mask bit set = opaque (level 0 = white); zero mask bytes skipped, 0xFF fast path.
void blit_masked(LvlBuf lvl, const Blit* v, int x, int y);
// Uses blit_masked when v->mask != NULL, else blit.
void blit_auto(LvlBuf lvl, const Blit* v, int x, int y);
// Fill a TW x TH iso diamond centred at (cx, cy) with `level` (0 paints white).
void fill_diamond(LvlBuf lvl, int cx, int cy, uint8_t level);

// ---- ground shading (depth cues). Pixels are only ever DARKENED: lvl = max(lvl, level).
// Spans of `n` rows starting at (xc, y0): row r is w[r] px wide, centred so that it covers
// xc - w[r]/2 .. xc - w[r]/2 + w[r] - 1 (even widths are centred half a pixel left of xc).
void shade_rows(LvlBuf lvl, int xc, int y0, const uint8_t* w, int n, uint8_t level);
// Contact shadow along the two FRONT edges of an object footprint of fw x fh tiles whose front
// tile's bottom-vertex pixel is (bx, by) (the OBJECT anchor): a band under the +j edge (lower-left,
// `kj` rows thick) and under the +i edge (lower-right, `ki` rows). The row touching the object has
// `level` (the last row of a full band level-1, min 1); both ends taper to one row.
void shade_front_edges(LvlBuf lvl, int bx, int by, int fw, int fh, int kj, int ki, uint8_t level);

// ---- battle helpers (M2, scene_battle): masked blit with per-draw effects. Slower than blit_masked (per pixel),
// meant for the one or two creatures of a battle frame. Implemented in blit_fx.c (part of the battle plugin).
typedef struct {
    uint8_t invert;  // 1: opaque pixels are drawn as 3 - level (the hit blink; white 0 becomes black 3)
    uint8_t flat;    // 0: none, else every opaque pixel is drawn at level (flat - 1) (silhouette)
    int16_t clip_y;  // screen rows >= clip_y are not drawn (faint: the creature sinks into its platform); 0 = none
    uint8_t sc_n, sc_d; // scale sc_n / sc_d about the bottom centre (send-out pop); sc_d == 0: unscaled
    int16_t real_y;  // with `flat`: screen rows < real_y keep the picture's own levels (the legendary intro's silhouette fills in from the top, contract 52); 0 = none
} BlitFx;
// Draws the (masked) picture with its top-left at (x, y) before scaling; a scaled picture keeps the same bottom
// centre pixel (x + w / 2, y + h).
void blit_masked_fx(LvlBuf lvl, const Blit* v, int x, int y, const BlitFx* fx);
