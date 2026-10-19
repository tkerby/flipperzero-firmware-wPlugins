// The battle platforms' ORIGINAL builder (nucleo/battle_gfx.c before contract 73, byte for byte the same pixels), kept here as the reference: tools/platbake bakes it into nucleo/files/plat.bin, and the host tests
// (tools/host_world/b1_check.c, b3_check.c) compare the plugin's loader with it. Plain C, no SDK. Port of tools/battle_mock.py platform() + shadow(): a slab of `thick` rows under an ellipse (rx, ry).
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// The shapes the game uses (thickness 2 always): a fighter's platform rx = 16 .. 30 with ry = rx / 3 (at least 4), the trainers' (17, 5 = the same as rx 17) and the focus platform (33, 7).
#define PLAT_N 16
static inline void plat_shape(int i, int* rx, int* ry) { // i = 0 .. PLAT_N - 1
    *rx = i < 15 ? 16 + i : 33;
    *ry = i < 15 ? (*rx / 3 < 4 ? 4 : *rx / 3) : 7;
}

static inline bool pr_in_ell(int x, int y, int oy, int rx, int ry, int k2_10000) {
    const int64_t dx = x, dy = y - oy; // 64-bit: the device's `long` is 32 bits and a big platform's left side passes 2^31
    return (dx * dx * ry * ry + dy * dy * rx * rx) * 10000 <= (int64_t)k2_10000 * rx * rx * ry * ry;
}
static inline int pr_lv(const uint8_t* blk, int bpr, int x, int y) {
    return (blk[y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3;
}
static inline void pr_set(uint8_t* blk, int bpr, int x, int y, int v) {
    uint8_t* q = blk + y * bpr + (x >> 2);
    const int sh = (x & 3) * 2;
    *q = (uint8_t)((*q & ~(3 << sh)) | (v << sh));
}
static inline bool pr_cov(int cx, int cy, int rx, int ry, int thick) { // the top face or one of its `thick` copies below it
    for(int k = 0; k <= thick; k++)
        if(pr_in_ell(cx, cy, k, rx, ry, 10000)) return true;
    return false;
}
// Builds the 2-bit picture (levels 0 white/transparent .. 3 black, 4 pixels per byte, row stride bpr = (w + 3) / 4); returns the block (calloc), *w_out / *h_out its size.
static inline uint8_t* plat_ref_build(int rx, int ry, int thick, int* w_out, int* h_out) {
    const int w = 2 * rx + 3, h = 2 * ry + thick + 3;
    const int ox = rx + 1, oy = ry + 1;
    const int bpr = (w + 3) / 4;
    uint8_t* blk = (uint8_t*)calloc(1, (size_t)bpr * h);
    if(!blk) return NULL;
#define LV(x, y) pr_lv(blk, bpr, x, y)
#define SETLV(x, y, v) pr_set(blk, bpr, x, y, v)
#define COV(x, y) pr_cov((x)-ox, (y)-oy, rx, ry, thick)
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            int cx = x - ox, cy = y - oy;
            if(pr_in_ell(cx, cy, 0, rx, ry, 10000)) SETLV(x, y, pr_in_ell(cx, cy, 0, rx, ry, 3844) ? 0 : 1);
            else if(pr_cov(cx, cy, rx, ry, thick)) SETLV(x, y, 2);
        }
    }
    // black outline where a covered pixel touches an uncovered one (the picture has a 1 px margin all round)
    for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
            if(COV(x, y) && (!COV(x + 1, y) || !COV(x - 1, y) || !COV(x, y + 1) || !COV(x, y - 1))) SETLV(x, y, 3);
    // rim line: the lower boundary of the top face, in front of the side band
    for(int x = 0; x < w; x++)
        for(int y = oy; y + 1 < h; y++) {
            int cx = x - ox, cy = y - oy;
            if(pr_in_ell(cx, cy, 0, rx, ry, 10000) && !pr_in_ell(cx, cy + 1, 0, rx, ry, 10000) && LV(x, y + 1) == 2) SETLV(x, y, 3);
        }
    // contact shadow (level 2 core, level 1 halo), only ever darkens
    int srx = (int)(rx * 0.55f * 1.15f), sry = (int)(ry * 0.55f);
    if(sry < 2) sry = 2;
    for(int pass = 0; pass < 2; pass++) {
        const int k2 = pass == 0 ? 18225 : 10000; // the ellipse (srx, sry) scaled by 1.35 / 1: integer math, the same on every compiler
        uint8_t v = pass == 0 ? 1 : 2;
        for(int y = 0; y < h; y++)
            for(int x = 0; x < w; x++) {
                if(pr_in_ell(x - ox, y - oy, 0, srx, sry, k2) && COV(x, y) && LV(x, y) < v) SETLV(x, y, v);
            }
    }
#undef LV
#undef SETLV
#undef COV
    *w_out = w;
    *h_out = h;
    return blk;
}
