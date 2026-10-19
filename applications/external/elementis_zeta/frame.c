#include "frame.h"
#include <string.h>
#include <stdlib.h>

// PM[level][phase] = bitmask of sub-frames in which the pixel is black:
// bit s set iff ((s + phase) % 3) < level, phase = (x + y) % 3.
static const uint8_t PM[4][3] = {{0, 0, 0}, {1, 4, 2}, {3, 5, 6}, {7, 7, 7}};

// SPREAD[m]: sub-frame mask m -> one bit per plane at bit 0 / 8 / 16 of a 32-bit accumulator.
static const uint32_t SPREAD[8] =
    {0x000000, 0x000001, 0x000100, 0x000101, 0x010000, 0x010001, 0x010100, 0x010101};

static uint8_t (*ov_planes)[PLANE_BYTES]; // planes of the set being composed

bool frame_init(Frame* f) {
    memset(f, 0, sizeof(*f));
    f->pending = -1;
    for(int i = 0; i < 2; i++) {
        f->sets[i] = calloc(CYCLE, PLANE_BYTES);
        if(!f->sets[i]) return false;
    }
    return true;
}

void frame_deinit(Frame* f) {
    for(int i = 0; i < 2; i++) {
        free(f->sets[i]);
        f->sets[i] = NULL;
    }
}

bool frame_idle_release(Frame* f) {
    if(f->pending >= 0) return false; // the idle set is about to be adopted
    uint8_t idle = (uint8_t)(1 - f->front);
    free(f->sets[idle]);
    f->sets[idle] = NULL;
    ov_planes = NULL;
    return true;
}

bool frame_idle_restore(Frame* f) {
    uint8_t idle = (uint8_t)(1 - f->front);
    if(f->sets[idle]) return true;
    f->sets[idle] = calloc(CYCLE, PLANE_BYTES);
    return f->sets[idle] != NULL;
}

void frame_set_dither(Frame* f, int camx, int camy) {
    int o = (camx + camy) % 3;
    f->dither_off = (uint8_t)(o < 0 ? o + 3 : o);
}

void frame_set_pins(Frame* f, const uint8_t* pin) {
    f->pin = pin;
}

void frame_dither_reset(Frame* f) {
    f->dither_off = 0;
    f->pin = NULL;
}

void frame_clear(Frame* f, uint8_t level) {
    frame_dither_reset(f);
    memset(f->lvl, lvl_pat(level), sizeof(f->lvl));
}

void frame_overlay_begin(Frame* f) {
    f->back = (uint8_t)(1 - f->front);
    ov_planes =
        f->sets[f->back]; // NULL while the idle set is released: the overlay primitives and compose do nothing
}

void frame_compose(Frame* f, uint8_t fade) {
    frame_overlay_begin(f);
    if(!ov_planes) return;

    // T[phase * 4 + level] = bits to OR (shifted by the pixel's row in the page) into the
    // accumulator: plane s lands in byte s. Includes the fade LUT lut[l] = max(0, l - fade).
    uint32_t T[12];
    for(int ph = 0; ph < 3; ph++)
        for(int l = 0; l < 4; l++)
            T[ph * 4 + l] = SPREAD[PM[l > fade ? l - fade : 0][ph]];

    const int off = f->dither_off;
    uint8_t* o0 = ov_planes[0];
    uint8_t* o1 = ov_planes[1];
    uint8_t* o2 = ov_planes[2];

    for(int p = 0; p < PAGES; p++) {
        const uint8_t* rows = &f->lvl[p * 8][0];
        int cell = p * SCREEN_W;
        for(int xq = 0; xq < SCREEN_W / 4; xq++, cell += 4) {
            // 8 bytes, one per row of the page, each holding 4 horizontally adjacent pixels (2 bits each).
            uint8_t w[8], any = 0;
            for(int k = 0; k < 8; k++) {
                w[k] = rows[k * LVL_STRIDE + xq];
                any |= w[k];
            }
            if(!any) { // all white
                memset(o0 + cell, 0, 4);
                memset(o1 + cell, 0, 4);
                memset(o2 + cell, 0, 4);
                continue;
            }
            int base = (p * 8 + xq * 4 + off) % 3; // phase of pixel (x, page top row)
            for(int c = 0; c < 4; c++) {
                int ph = base + c;
                if(ph >= 3) ph -= 3;
                uint32_t acc = 0;
                for(int k = 0; k < 8; k++) {
                    acc |= T[ph * 4 + ((w[k] >> (2 * c)) & 3)] << k;
                    if(++ph == 3) ph = 0;
                }
                o0[cell + c] = (uint8_t)acc;
                o1[cell + c] = (uint8_t)(acc >> 8);
                o2[cell + c] = (uint8_t)(acc >> 16);
            }
        }
    }

    // Screen-anchored pixels (the hero): the columns that hold any are composed again with the phase of the screen position (the
    // same T table, the base phase without the offset) and merged under the pin mask.
    if(off && f->pin) {
        for(int p = 0; p < PIN_PAGES; p++) {
            for(int cx = 0; cx < PIN_W; cx++) {
                uint8_t m = f->pin[p * PIN_W + cx];
                if(!m) continue;
                int x = PIN_X0 + cx;
                int ph = (p * 8 + x) % 3;
                const uint8_t* src = &f->lvl[p * 8][x >> 2];
                const int sh = (x & 3) * 2;
                uint32_t acc = 0;
                for(int k = 0; k < 8; k++) {
                    acc |= T[ph * 4 + ((src[k * LVL_STRIDE] >> sh) & 3)] << k;
                    if(++ph == 3) ph = 0;
                }
                int idx = p * SCREEN_W + x;
                o0[idx] = (uint8_t)((o0[idx] & ~m) | ((uint8_t)acc & m));
                o1[idx] = (uint8_t)((o1[idx] & ~m) | ((uint8_t)(acc >> 8) & m));
                o2[idx] = (uint8_t)((o2[idx] & ~m) | ((uint8_t)(acc >> 16) & m));
            }
        }
    }
}

void frame_publish(Frame* f) {
    if(!f->sets[f->back]) return; // never hand the flip loop a released set
    f->pending = (int8_t)f->back;
}

bool frame_adopt(Frame* f) {
    if(f->pending >= 0) {
        f->front = (uint8_t)f->pending;
        f->pending = -1;
        return true;
    }
    return false;
}

void ov_px(int x, int y, uint8_t color) {
    if(!ov_planes || x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    int idx = (y >> 3) * SCREEN_W + x;
    uint8_t bit = (uint8_t)(1 << (y & 7));
    for(int s = 0; s < CYCLE; s++) {
        if(color)
            ov_planes[s][idx] |= bit;
        else
            ov_planes[s][idx] &= (uint8_t)~bit;
    }
}

void ov_fill(int x, int y, int w, int h, uint8_t color) {
    if(!ov_planes) return;
    int x1 = x + w, y1 = y + h;
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x1 > SCREEN_W) x1 = SCREEN_W;
    if(y1 > SCREEN_H) y1 = SCREEN_H;
    if(x >= x1 || y >= y1) return;
    for(int pg = y >> 3; pg <= (y1 - 1) >> 3; pg++) {
        int lo = pg << 3;
        int a = y > lo ? y - lo : 0;
        int b = y1 < lo + 8 ? y1 - lo : 8; // exclusive
        uint8_t mask = (uint8_t)(((1 << b) - 1) & ~((1 << a) - 1));
        for(int s = 0; s < CYCLE; s++) {
            uint8_t* row = &ov_planes[s][pg * SCREEN_W];
            if(color)
                for(int xx = x; xx < x1; xx++)
                    row[xx] |= mask;
            else
                for(int xx = x; xx < x1; xx++)
                    row[xx] &= (uint8_t)~mask;
        }
    }
}

void ov_frame(int x, int y, int w, int h, uint8_t color) {
    if(w <= 0 || h <= 0) return;
    ov_fill(x, y, w, 1, color);
    ov_fill(x, y + h - 1, w, 1, color);
    ov_fill(x, y, 1, h, color);
    ov_fill(x + w - 1, y, 1, h, color);
}

void ov_bitmap(int x, int y, int w, int h, const uint8_t* rows, int bpr, uint8_t color) {
    if(!ov_planes || w <= 0 || h <= 0) return;
    int c0 = x < 0 ? -x : 0;
    int c1 = x + w > SCREEN_W ? SCREEN_W - x : w;
    int r0 = y < 0 ? -y : 0;
    int r1 = y + h > SCREEN_H ? SCREEN_H - y : h;
    if(c0 >= c1 || r0 >= r1) return;
    int pg0 = (y + r0) >> 3, pg1 = (y + r1 - 1) >> 3;
    for(int c = c0; c < c1; c++) {
        const uint8_t* col = rows + (c >> 3);
        uint8_t bit = (uint8_t)(0x80 >> (c & 7));
        for(int pg = pg0; pg <= pg1; pg++) {
            int ra = pg * 8 - y, rb = ra + 8; // rows of the glyph that land in this page
            if(ra < r0) ra = r0;
            if(rb > r1) rb = r1;
            uint8_t m = 0;
            for(int r = ra; r < rb; r++)
                if(col[r * bpr] & bit) m |= (uint8_t)(1 << ((y + r) & 7));
            if(!m) continue;
            int idx = pg * SCREEN_W + x + c;
            for(int s = 0; s < CYCLE; s++) {
                if(color)
                    ov_planes[s][idx] |= m;
                else
                    ov_planes[s][idx] &= (uint8_t)~m;
            }
        }
    }
}
