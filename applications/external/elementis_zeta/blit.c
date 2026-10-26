#include "blit.h"

// Level buffer = 2 bits per pixel, 4 pixels per byte (lvl.h), the packing the sprite data already has: a source byte
// (4 pixels) is merged into the destination row with a shift and one or two read-modify-writes, instead of four
// per-pixel stores. A destination byte is either entirely on the screen (0..LVL_STRIDE-1) or entirely off it.

// Mask nibble (bit k = pixel k of a group of 4) -> 2-bit-per-pixel mask (0b11 where set).
static const uint8_t MEXP[16] =
    {0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F, 0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF};

// dst bytes covered by source group `i` of a row whose first pixel lands on screen x: merge `s` (levels) under the 2 bpp
// mask `m` at pixel offset x + 4 * i.
static inline __attribute__((always_inline)) void
    put_group(uint8_t* dst, int bx, unsigned r, int i, uint8_t s, uint8_t m) {
    int j = bx + i;
    if(!r) { // byte aligned
        if((unsigned)j < LVL_STRIDE) dst[j] = (uint8_t)((dst[j] & ~m) | (s & m));
        return;
    }
    unsigned S = (unsigned)s << r, M = (unsigned)m << r; // up to 14 bits
    if((unsigned)j < LVL_STRIDE) dst[j] = (uint8_t)((dst[j] & ~M) | (S & M));
    if((unsigned)(j + 1) < LVL_STRIDE) {
        uint8_t mh = (uint8_t)(M >> 8);
        dst[j + 1] = (uint8_t)((dst[j + 1] & ~mh) | ((S >> 8) & mh));
    }
}

// Mirrored draw (folded directions: W / NW / SW are stored as flipped E / NE / SE). Per pixel; only the hero and
// the NPCs facing west use it.
static void blit_flipped(LvlBuf lvl, const Blit* v, int x, int y, bool masked) {
    const int w = v->w;
    for(int sy = 0; sy < v->h; sy++) {
        int py = y + sy;
        if(py < 0 || py >= SCREEN_H) continue;
        const uint8_t* mrow = masked ? v->mask + sy * v->mbpr : 0;
        int k0 = x < 0 ? -x : 0,
            k1 = x + w > SCREEN_W ? SCREEN_W - x : w; // visible destination columns
        for(int k = k0; k < k1; k++) {
            int sx = w - 1 - k;
            uint8_t l = blit_px(v, sy, sx);
            if(mrow ? !((mrow[sx >> 3] >> (sx & 7)) & 1) : !l) continue;
            lvl_put(lvl, x + k, py, l);
        }
    }
}

void blit(LvlBuf lvl, const Blit* v, int x, int y) {
    if(v->flip) {
        blit_flipped(lvl, v, x, y, false);
        return;
    }
    const int bx = x >> 2; // floor
    const unsigned r = (unsigned)(x & 3) * 2;
    for(int sy = 0; sy < v->h; sy++) {
        int py = y + sy;
        if(py < 0 || py >= SCREEN_H) continue;
        const uint8_t* src = v->pix + sy * v->bpr;
        uint8_t* dst = lvl[py];
        for(int i = 0; i < v->bpr; i++) {
            uint8_t b = src[i];
            if(!b) continue;
            uint8_t t = (uint8_t)((b | (b >> 1)) &
                                  0x55); // bit 0 of a pair: the pixel is not level 0 (transparent)
            put_group(dst, bx, r, i, b, (uint8_t)(t * 3));
        }
    }
}

void blit_masked(LvlBuf lvl, const Blit* v, int x, int y) {
    if(v->flip) {
        blit_flipped(lvl, v, x, y, true);
        return;
    }
    const int bx = x >> 2;
    const unsigned r = (unsigned)(x & 3) * 2;
    for(int sy = 0; sy < v->h; sy++) {
        int py = y + sy;
        if(py < 0 || py >= SCREEN_H) continue;
        const uint8_t* mrow = v->mask + sy * v->mbpr;
        const uint8_t* src = v->pix + sy * v->bpr;
        uint8_t* dst = lvl[py];
        for(int mx = 0; mx < v->mbpr; mx++) {
            uint8_t m = mrow[mx];
            if(!m) continue;
            int i = mx << 1; // first source group of this mask byte
            uint8_t lo = MEXP[m & 15];
            if(lo) put_group(dst, bx, r, i, src[i], lo);
            uint8_t hi = MEXP[m >> 4];
            if(hi && i + 1 < v->bpr) put_group(dst, bx, r, i + 1, src[i + 1], hi);
        }
    }
}

void blit_auto(LvlBuf lvl, const Blit* v, int x, int y) {
    if(v->mask)
        blit_masked(lvl, v, x, y);
    else
        blit(lvl, v, x, y);
}

void fill_diamond(LvlBuf lvl, int cx, int cy, uint8_t level) {
    const int hh = TH / 2;
    for(int r = 0; r < TH; r++) {
        int py = cy - hh + r;
        if(py < 0 || py >= SCREEN_H) continue;
        int n = (r < hh) ? r : (TH - 1 - r); // 0..hh-1
        int w = (TW * (n + 1)) / hh; // grows to TW at the middle rows
        if(w > TW) w = TW;
        int x0 = cx - w / 2;
        lvl_span(lvl, py, x0, x0 + w, level);
    }
}

static inline void shade_px(LvlBuf lvl, int x, int y, uint8_t level) {
    lvl_darken(lvl, x, y, level);
}

void shade_rows(LvlBuf lvl, int xc, int y0, const uint8_t* w, int n, uint8_t level) {
    for(int r = 0; r < n; r++) {
        int y = y0 + r;
        if(y < 0 || y >= SCREEN_H) continue;
        int x0 = xc - w[r] / 2;
        for(int k = 0; k < w[r]; k++)
            shade_px(lvl, x0 + k, y, level);
    }
}

// One band: columns x0..x0+len-1 (screen), edge row y_e(x) = by - ((bx - x) >> 1) on the +j
// side (x < bx) or by - ((x - bx + 1) >> 1) on the +i side (x >= bx).
void shade_front_edges(LvlBuf lvl, int bx, int by, int fw, int fh, int kj, int ki, uint8_t level) {
    for(int side = 0; side < 2; side++) {
        int len = (side == 0 ? fw : fh) * (TW / 2);
        int k = side == 0 ? kj : ki;
        int xs = side == 0 ? bx - len : bx;
        for(int d = 0; d < len; d++) {
            int x = xs + d;
            if(x < 0 || x >= SCREEN_W) continue;
            int ye = side == 0 ? by - ((bx - x) >> 1) : by - ((x - bx + 1) >> 1);
            int far = side == 0 ? d : len - 1 - d; // distance from the open end of the band
            int t = 1 + (far >> 1);
            if(t > k) t = k;
            for(int r = 1; r <= t; r++) {
                int lv = (r == k && k > 1) ? level - 1 :
                                             level; // last row of a full band is lighter
                shade_px(lvl, x, ye + r, (uint8_t)(lv < 1 ? 1 : lv));
            }
        }
    }
}
