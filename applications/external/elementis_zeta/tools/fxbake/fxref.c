// Reference painters of the battle effects (contract 10.4): the ORIGINAL per-pixel code of nucleo/battle_fx.c (until the
// effects moved to nucleo/files/fx.bin), kept verbatim as the SOURCE OF THE ARTWORK.  tools/fxbake/fxbake.c compiles it with -DFXBAKE
// (every pixel goes to a big canvas instead of the 128 x 64 level buffer) and renders each effect's frames into fx.bin; the host
// proof (tools/host_battle/fxcheck.c) compiles it WITHOUT -DFXBAKE and compares it with the player.  Edit the art here, run
// `make -C tools/fxbake` (build_assets.sh does), commit the new fx.bin.  The only changes against the original: the function names
// (ref_*), fx_duration -> fxt_duration (fx_tab.h), the FXBAKE canvas / flash hooks and the flash loop clipped to the screen (it read
// outside the buffer for a box partly off-screen).
#include "battle_fx.h"
#include "fx_tab.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f

#ifdef FXBAKE
// bake build: the pixels go to a canvas bigger than the screen (fxbake.c); the bolt's "flash the target box" is a flag of the frame
void bk_put(int x, int y, uint8_t v);
void bk_span(int y, int x0, int x1, uint8_t v);
extern int bk_flash;
#undef lvl_putc
#define lvl_putc(l, x, y, v) bk_put((x), (y), (v))
#endif
// ------------------------------------------------------------------------------------------------ float math
// No libm: newlib's sinf/cosf/powf/atan2f/fmodf/roundf... are soft code (~5 KB with the argument-reduction tables)
// that is loaded into RAM with the plugin. The Cortex-M4F does + - * / and int<->float in hardware, so these few
// helpers (Taylor / log-exp polynomials, accurate to ~1e-6) are all the effects need.
static inline float fabs_(float x) {
    return x < 0 ? -x : x;
}
static inline float fmax_(float a, float b) {
    return a > b ? a : b;
}
static inline float fmin_(float a, float b) {
    return a < b ? a : b;
}
static inline float floor_(float x) { // |x| < 2^31
    int i = (int)x;
    return (float)(i - (x < (float)i));
}
static inline float round_(float x) { // half away from zero, like roundf
    return (float)(int)(x + (x < 0 ? -0.5f : 0.5f));
}
static float sin_(float x) {
    float r = x - round_(x * (1.0f / (2 * PI_F))) * (2 * PI_F); // -pi..pi
    if(r > PI_F / 2) r = PI_F - r;
    else if(r < -PI_F / 2) r = -PI_F - r;
    float r2 = r * r;
    return r * (1.0f + r2 * (-1.0f / 6 + r2 * (1.0f / 120 + r2 * (-1.0f / 5040 + r2 * (1.0f / 362880 + r2 * (-1.0f / 39916800))))));
}
static inline float cos_(float x) {
    return sin_(x + PI_F / 2);
}

// ------------------------------------------------------------------------------------------------ primitives
#define BLK 3
#define WHT 0
static inline void px(LvlBuf lvl, int x, int y, uint8_t v) {
    lvl_putc(lvl, x, y, v);
}
static inline unsigned hash8(unsigned i) {
    i = i * 2654435761u;
    return (i >> 13) & 0xFF;
}
static inline int imax_(int a, int b) {
    return a > b ? a : b;
}
static inline int imin_(int a, int b) {
    return a < b ? a : b;
}
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static void span(LvlBuf lvl, int y, int x0, int x1, uint8_t v) { // pixels [x0, x1) of row y
#ifdef FXBAKE
    bk_span(y, x0, x1, v);
#else
    if((unsigned)y < SCREEN_H) lvl_span(lvl, y, x0, x1, v);
#endif
}

typedef struct {
    LvlBuf lvl;
    int t, x0, y0, x1, y1, W, H, cx, cy, dir, big;
    float u;
} Fx;

// Filled disc with a 1 px edge (radii in px).
static void disc(LvlBuf lvl, int cx, int cy, int r, uint8_t fill, uint8_t edge) {
    int lim = r * r + r;
    for(int dy = -r; dy <= r; dy++)
        for(int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if(d2 > lim) continue;
            px(lvl, cx + dx, cy + dy, d2 > lim - 2 * r ? edge : fill);
        }
}

// Rotated ellipse: `body` inside, `edge` as the 1 px ring.
static void blob(LvlBuf lvl, float cx, float cy, float rx, float ry, float ang, uint8_t body, uint8_t edge) {
    float ca = cos_(ang), sa = sin_(ang);
    int R = (int)(fmax_(rx, ry) + 2);
    int fx = (int)floor_(cx), fy = (int)floor_(cy);
    for(int dy = -R; dy <= R; dy++)
        for(int dx = -R; dx <= R; dx++) {
            float x = dx - (cx - fx), y = dy - (cy - fy);
            float u = (x * ca + y * sa) / rx, v = (-x * sa + y * ca) / ry;
            if(u * u + v * v > 1.0f) continue;
            float ui = (x * ca + y * sa) / fmax_(rx - 1.0f, 0.5f), vi = (-x * sa + y * ca) / fmax_(ry - 1.0f, 0.5f);
            px(lvl, fx + dx, fy + dy, ui * ui + vi * vi <= 1.0f ? body : edge);
        }
}

// thick 0: one pixel, 1: plus (3 wide), 2: 3x3 block, 3: 5-wide diamond
static void line_px(LvlBuf lvl, int x0, int y0, int x1, int y1, uint8_t v, int thick) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for(;;) {
        px(lvl, x0, y0, v);
        if(thick >= 1) {
            px(lvl, x0 + 1, y0, v);
            px(lvl, x0 - 1, y0, v);
            px(lvl, x0, y0 + 1, v);
            px(lvl, x0, y0 - 1, v);
        }
        if(thick >= 2) {
            px(lvl, x0 + 1, y0 + 1, v);
            px(lvl, x0 - 1, y0 - 1, v);
            px(lvl, x0 + 1, y0 - 1, v);
            px(lvl, x0 - 1, y0 + 1, v);
        }
        if(thick >= 3) {
            px(lvl, x0 + 2, y0, v);
            px(lvl, x0 - 2, y0, v);
            px(lvl, x0, y0 + 2, v);
            px(lvl, x0, y0 - 2, v);
        }
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
// A black 3 px line with a white halo.
static void bold(LvlBuf lvl, int x0, int y0, int x1, int y1) { // black 3 px (plus) with a white halo
    line_px(lvl, x0, y0, x1, y1, WHT, 3);
    line_px(lvl, x0, y0, x1, y1, BLK, 1);
}
static void heavy(LvlBuf lvl, int x0, int y0, int x1, int y1) { // black 3 x 3 with a white halo (slashes, bolts)
    line_px(lvl, x0, y0, x1, y1, WHT, 3);
    line_px(lvl, x0, y0, x1, y1, BLK, 2);
}

// A star with 4 long and 4 short points (radius R), `fill` inside, `edge` as the ring.
static void star(LvlBuf lvl, int cx, int cy, float R, float rot, uint8_t fill, uint8_t edge) {
    int Ri = (int)R + 1;
    // c = |cos(4 * (atan2(dy, dx) + rot))| without atan2 / sqrt: cos 4a and sin 4a of the pixel's angle a are
    // polynomials of (dx, dy) divided by (dx^2 + dy^2)^2; the rotation is applied with the angle addition formula.
    float cr = cos_(4.0f * rot), sr = sin_(4.0f * rot);
    for(int dy = -Ri; dy <= Ri; dy++)
        for(int dx = -Ri; dx <= Ri; dx++) {
            int d2 = dx * dx + dy * dy;
            float c4 = 1.0f, s4 = 0.0f; // the centre pixel has angle 0
            if(d2) {
                int xx = dx * dx, yy = dy * dy;
                float inv = 1.0f / ((float)d2 * (float)d2);
                c4 = (float)(xx * xx - 6 * xx * yy + yy * yy) * inv;
                s4 = (float)(4 * dx * dy * (xx - yy)) * inv;
            }
            float c = fabs_(c4 * cr - s4 * sr);
            float rr = R * (0.38f + 0.62f * c * c);
            float fd = (float)d2;
            if(fd > rr * rr) continue;
            float in = rr - 1.4f;
            px(lvl, cx + dx, cy + dy, (in < 0 || fd > in * in) ? edge : fill);
        }
}

// ------------------------------------------------------------------------------------------------ painters
static void fx_star(const Fx* c) {
    float u = c->u;
    float R = fmin_(c->big ? 30.0f : 25.0f, 8.0f + fmin_(c->W, c->H) * 0.42f);
    float e = u < 0.22f ? 1.15f * u / 0.22f : 1.15f - 0.3f * (u - 0.22f); // grows past full size, then settles
    if(u > 0.78f) e *= (1.0f - u) / 0.22f;
    uint8_t f = ((c->t / 80) & 1) ? BLK : WHT;
    star(c->lvl, c->cx, c->cy, R * e, u * 0.9f, f, f ^ 3);
    if(u > 0.1f && u < 0.7f)
        for(int i = 0; i < 8; i++) {
            float a = i * (PI_F / 4) + 0.4f, r0 = R * e + 3 + (u - 0.1f) * 14.0f;
            bold(c->lvl, c->cx + (int)(cos_(a) * r0), c->cy + (int)(sin_(a) * r0), c->cx + (int)(cos_(a) * (r0 + 6)),
                 c->cy + (int)(sin_(a) * (r0 + 6)));
        }
}

static void fx_stars3(const Fx* c) {
    static const int8_t PX[3] = {-22, 18, -2}, PY[3] = {-14, -4, 14};
    for(int i = 0; i < 3; i++) {
        float ui = (c->u - i * 0.22f) / 0.4f;
        if(ui < 0 || ui > 1) continue;
        float R = 7.0f + (c->big ? 9.0f : 7.0f) * sin_(ui * PI_F);
        uint8_t f = ((c->t / 70) & 1) ? BLK : WHT;
        star(c->lvl, c->cx + PX[i] * c->W / 60, c->cy + PY[i] * c->H / 60, R, ui, f, f ^ 3);
    }
}

// Flame: a teardrop drawn as spans (white halo, black body, white core), swaying with the height.
static const uint8_t FLP[12] = {4, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1};
static void flame(LvlBuf lvl, int cx, int base, int h, int w, float ph, int pass) {
    if(h < 5) return;
    {
        for(int r = 0; r < h; r++) {
            int hw = FLP[r * 12 / h] * w / 16;
            if(pass == 2) {
                if(r < h / 5 || r > h * 62 / 100) continue;
                hw = hw / 3;
            }
            int off = (int)round_(sin_(r * 0.3f + ph) * (float)(r * w) / (float)(h * 7));
            int x0 = cx + off - hw, x1 = cx + off + hw + 1, y = base - r;
            if(pass == 0) {
                span(lvl, y, x0 - 2, x1 + 2, WHT);
                if(r == h - 1) span(lvl, y - 1, x0 - 1, x1 + 1, WHT), span(lvl, y - 2, x0, x1, WHT);
                if(r == 0) span(lvl, y + 1, x0 - 1, x1 + 1, WHT);
            } else {
                span(lvl, y, x0, x1, pass == 1 ? BLK : WHT);
            }
        }
    }
}

static void fx_flame(const Fx* c) {
    static const struct { int8_t fx, fy, h, w; } SPEC[5] = {
        {14, 80, 22, 12}, {86, 80, 22, 12}, {32, 94, 30, 14}, {68, 94, 30, 14}, {50, 102, 42, 18}};
    float u = c->u;
    float g = fmin_(1.0f, u * 3.0f) * (u > 0.84f ? (1.0f - u) / 0.16f : 1.0f);
    float sc = fmin_(1.3f, c->H / 40.0f) * (c->big ? 1.15f : 1.0f), ph = (float)(c->t / 90) * 0.9f;
    for(int pass = 0; pass < 3; pass++) // halo of all flames, then their bodies, then their cores
        for(int i = 0; i < 5; i++) {
            float gi = clampf(g * 1.3f - i * 0.05f, 0.0f, 1.0f);
            flame(c->lvl, c->x0 + c->W * SPEC[i].fx / 100, c->y0 + c->H * SPEC[i].fy / 100, (int)(SPEC[i].h * sc * gi),
                  (int)(SPEC[i].w * sc), ph + i * 1.7f, pass);
        }
    for(int i = 0; i < 7; i++) { // embers
        if(u < 0.25f + i * 0.07f || u > 0.92f) continue;
        int x = c->x0 + c->W * (int)(hash8(i + 3)) / 256, y = c->y0 + c->H * (int)(hash8(i + 9)) / 400 - (int)(u * 16);
        disc(c->lvl, x, y, 1, BLK, WHT);
    }
}

static void fx_drops(const Fx* c) {
    float u = c->u;
    for(int i = 0; i < 9; i++) {
        float ui = (u - i * 0.06f) / 0.3f;
        if(ui < 0 || ui > 1.5f) continue;
        int dx = c->x0 + 3 + (int)((hash8(i + 3) * (c->W - 6)) / 255);
        float bot = c->y0 + c->H * (0.3f + 0.55f * hash8(i + 9) / 255.0f);
        if(ui <= 1.0f) {
            int y = (int)(c->y0 - 12 + (bot - (c->y0 - 12)) * ui * ui);
            bold(c->lvl, dx, y - 9, dx, y - 4);
            disc(c->lvl, dx, y, 3, WHT, BLK);
        } else { // splash
            int y = (int)bot, d = (int)((ui - 1.0f) * 14);
            for(int k = -1; k <= 1; k += 2) bold(c->lvl, dx + k * (2 + d), y - d, dx + k * (4 + d), y - d - 3);
        }
    }
    if(u > 0.4f) { // a wave sweeping across the lower body
        float p = clampf((u - 0.4f) / 0.45f, 0, 1), q = u > 0.85f ? (u - 0.85f) / 0.15f : 0.0f;
        int xa = c->x0 - 6 + (int)((c->W + 12) * q), xb = c->x0 - 6 + (int)((c->W + 12) * p);
        for(int k = 0; k < 2; k++) {
            int ym = c->y0 + c->H * (60 + 22 * k) / 100;
            for(int x = xa; x < xb; x++) {
                int y = ym + (int)round_(2.4f * sin_((x + (k ? 5 : 0)) / 3.0f + c->t / 50.0f));
                span(c->lvl, y - 1, x, x + 1, WHT);
                span(c->lvl, y, x, x + 1, BLK);
                span(c->lvl, y + 1, x, x + 1, BLK);
                span(c->lvl, y + 2, x, x + 1, WHT);
            }
        }
    }
}

// A wall of water: its front sweeps across the target in the direction of the attack (black crest and stripes).
static void fx_wave(const Fx* c) {
    float p = clampf(c->u / 0.8f, 0, 1);
    int span_w = c->W + 90, d = c->dir;
    int front = d > 0 ? c->x0 - 45 + (int)(span_w * p) : c->x1 + 45 - (int)(span_w * p);
    int hmax = c->H + 6;
    for(int k = 0; k < 44; k++) { // k = distance behind the front
        int x = front - d * k;
        int h = k < 10 ? hmax * (k + 4) / 14 : hmax * (60 - k) / 50;
        if(h < 3) continue;
        int yt = c->y1 + 2 - h + (int)round_(2.0f * sin_(x / 3.0f + c->t / 40.0f));
        span(c->lvl, yt - 1, x, x + 1, WHT);
        span(c->lvl, yt, x, x + 1, BLK);
        span(c->lvl, yt + 1, x, x + 1, BLK);
        for(int y = yt + 2; y <= c->y1 + 2; y++)
            if(((y - yt) & 3) == 0) span(c->lvl, y, x, x + 1, BLK);
    }
}

static void fx_leaves(const Fx* c) {
    float u = c->u, cxf = (c->x0 + c->x1) / 2.0f;
    float fade = u > 0.85f ? (1.0f - u) / 0.15f : 1.0f;
    int n = c->big ? 14 : 10;
    for(int i = 0; i < n; i++) {
        float ph = u * 1.25f + i * (1.0f / n);
        if(ph < 0.0f) continue;
        ph -= floor_(ph);
        float ang = u * 7.0f + i * 1.3f;
        float x = cxf + (c->W / 2 + 6) * sin_(ang) * (0.35f + 0.65f * ph);
        float y = c->y1 + 6 - (c->H + 12) * ph;
        float rot = ang * 1.3f;
        if(fade < 1.0f && hash8(i) > fade * 255) continue;
        blob(c->lvl, x, y, 6.5f, 3.2f, rot, BLK, WHT);
        px(c->lvl, (int)floor_(x - cos_(rot) * 3), (int)floor_(y - sin_(rot) * 3), WHT);
        px(c->lvl, (int)floor_(x), (int)floor_(y), WHT);
        px(c->lvl, (int)floor_(x + cos_(rot) * 3), (int)floor_(y + sin_(rot) * 3), WHT);
    }
}

static void fx_bubbles(const Fx* c) {
    for(int i = 0; i < 8; i++) {
        float ui = (c->u - i * 0.05f) / 0.65f;
        if(ui < 0) continue;
        int r = 3 + (int)(hash8(i + 5) % 4);
        float x = c->x0 + 4 + (hash8(i + 11) * (c->W - 8)) / 255.0f + 2.5f * sin_(ui * 9 + i);
        float y = c->y1 - 2 - (c->H - 4) * fmin_(ui, 1.0f);
        int ix = (int)x, iy = (int)y;
        if(ui > 1.0f) continue; // popped
        disc(c->lvl, ix, iy, r, WHT, BLK);
        px(c->lvl, ix - r / 2, iy - r / 2, BLK);
        px(c->lvl, ix - r / 2 + 1, iy - r / 2, BLK);
    }
}

static void fx_bolt(const Fx* c) {
    int phase = c->t / 80, n = c->big ? 4 : 3;
    if(c->t < 60 || (c->t >= 260 && c->t < 310)) // flash of the whole target
#ifdef FXBAKE
        bk_flash = 1;
#else
        for(int y = c->y0; y < c->y1; y++)
            for(int x = c->x0; x < c->x1; x++)
                if((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) lvl_put(c->lvl, x, y, (uint8_t)(3 - lvl_get(c->lvl, x, y)));
#endif
    for(int b = 0; b < n; b++) {
        if(((phase + b) % 3) == 0 && c->t > 100) continue; // flicker
        int px_ = c->x0 + c->W * (b + 1) / (n + 1) + (((phase + b) & 1) ? 4 : -4);
        int y = c->y0 - 6, seg = 0;
        while(y < c->y1 + 4) {
            int nx = px_ + ((seg & 1) ? 9 : -9) + (int)(hash8(b * 7 + seg + phase) % 5) - 2;
            nx = imax_(c->x0 - 6, imin_(c->x1 + 6, nx));
            int ny = y + 10;
            heavy(c->lvl, px_, y, nx, ny);
            px_ = nx;
            y = ny;
            seg++;
        }
    }
}

static void fx_rings(const Fx* c) {
    float Rx = c->W / 2.0f + 8, Ry = c->H / 2.0f + 6;
    for(int i = 0; i < 3; i++) {
        float ui = c->u * 1.3f - i * 0.2f;
        if(ui <= 0 || ui >= 1) continue;
        float rx = Rx * ui, ry = Ry * ui;
        int steps = (int)(2 * PI_F * fmax_(rx, ry)) + 8;
        for(int k = 0; k < steps; k++) {
            float a = 2 * PI_F * k / steps, ca = cos_(a), sa = sin_(a);
            int X = (int)round_(c->cx + ca * rx), Y = (int)round_(c->cy + sa * ry);
            px(c->lvl, X + (ca > 0 ? -2 : 2), Y + (sa > 0 ? -2 : 2), WHT);
            px(c->lvl, X, Y, BLK);
            px(c->lvl, X + (ca > 0 ? -1 : 1), Y + (sa > 0 ? -1 : 1), BLK);
        }
    }
}

static void fx_rocks(const Fx* c) {
    for(int i = 0; i < 7; i++) {
        float ui = (c->u - i * 0.07f) / 0.42f;
        if(ui < 0) continue;
        float x = c->x0 + 5 + (hash8(i + 21) * (c->W - 10)) / 255.0f;
        float ground = c->y0 + c->H * (0.45f + 0.45f * hash8(i + 5) / 255.0f);
        if(ui < 1.0f) {
            float y = c->y0 - 14 + (ground - (c->y0 - 14)) * ui * ui;
            blob(c->lvl, x, y, 6.2f, 5.4f, 0.5f * i + ui * 3.0f, BLK, WHT);
            px(c->lvl, (int)x - 2, (int)y - 2, WHT);
        } else if(ui < 1.8f) { // dust puff + a rock that stays
            int d = (int)((ui - 1.0f) * 10);
            bold(c->lvl, (int)x - 6 - d, (int)ground - d / 2, (int)x - 9 - d, (int)ground - d / 2 - 2);
            bold(c->lvl, (int)x + 6 + d, (int)ground - d / 2, (int)x + 9 + d, (int)ground - d / 2 - 2);
            blob(c->lvl, x, ground, 5.0f, 4.0f, 0, BLK, WHT);
        }
    }
}

static void fx_slash(const Fx* c) {
    int n = c->big ? 5 : 3;
    for(int i = 0; i < n; i++) {
        float ui = (c->u - i * 0.12f) / 0.3f;
        if(ui <= 0 || (c->u > 0.88f)) continue;
        ui = fmin_(ui, 1.0f);
        int sx = c->x0 + c->W * i / n - 2, sy = c->y0 - 4;
        int ex = sx + c->H / 2 + 8, ey = c->y1 + 2;
        heavy(c->lvl, sx, sy, sx + (int)((ex - sx) * ui), sy + (int)((ey - sy) * ui));
    }
}

static void fx_swarm(const Fx* c) {
    for(int i = 0; i < 8; i++) {
        float ui = (c->u - i * 0.07f) / 0.3f;
        if(ui < 0 || ui > 1.7f) continue;
        float a = i * 0.9f + 0.4f, ca = cos_(a), sa = sin_(a);
        float Rx = c->W / 2.0f, Ry = c->H / 2.0f;
        float s0 = 1.0f + 14.0f / Rx, e0 = 0.25f;
        float k = ui < 1.0f ? s0 + (e0 - s0) * ui * ui : e0;
        int hx = c->cx + (int)(ca * Rx * k), hy = c->cy + (int)(sa * Ry * k);
        if(ui < 1.0f) {
            int tx = hx + (int)(ca * 12), ty = hy + (int)(sa * 12);
            heavy(c->lvl, hx, hy, tx, ty);
        } else if(ui < 1.5f) {
            bold(c->lvl, hx - 3, hy - 3, hx + 3, hy + 3);
            bold(c->lvl, hx - 3, hy + 3, hx + 3, hy - 3);
        }
    }
}

// A big arrow: head half-width = row, shaft 2 each side; `dir` -1 points up. Filled `fill` with a 1 px `edge`.
static void arrow(LvlBuf lvl, int cx, int tipy, int dir, uint8_t fill, uint8_t edge) {
    for(int pass = 0; pass < 2; pass++) {
        int g = pass == 0 ? 1 : 0;
        for(int r = -g; r <= 14 + g; r++) {
            int hw = (r < 7 ? (r < 0 ? 0 : r) : 2) + g;
            int y = tipy - dir * r; // dir -1 (up): the tip is the top row, rows go down
            span(lvl, y, cx - hw, cx + hw + 1, pass == 0 ? edge : fill);
        }
    }
}

static void fx_arrows(const Fx* c, int up) {
    int passes = c->big ? 3 : 2;
    for(int i = 0; i < 3; i++)
        for(int k = 0; k < passes; k++) {
            float ph = c->u * passes - k - i * 0.22f;
            if(ph < 0 || ph >= 1.0f) continue;
            int travel = c->H + 14;
            int y = up ? c->y1 + 2 - (int)(travel * ph) : c->y0 - 14 + (int)(travel * ph);
            // tip row: the arrow points up (tip at the top, rows go down) or down (tip at the bottom)
            arrow(c->lvl, c->cx + (i - 1) * 15, up ? y : y + 14, up ? -1 : 1, up ? BLK : WHT, up ? WHT : BLK);
        }
}

static void zed(LvlBuf lvl, int x, int y, int s) {
    bold(lvl, x, y, x + s, y);
    bold(lvl, x + s, y, x, y + s);
    bold(lvl, x, y + s, x + s, y + s);
}
static void fx_zzz(const Fx* c) {
    for(int i = 0; i < 3; i++) {
        float ui = (c->u - i * 0.2f) / 0.55f;
        if(ui <= 0 || ui >= 1) continue;
        zed(c->lvl, c->x1 - 18 - i * 4 + (int)(ui * 8), c->y0 + 24 - i * 11 - (int)(ui * 14), 7 + i * 3);
    }
}

static void fx_orbit(const Fx* c) {
    float cy = c->y0 + c->H * 0.2f;
    for(int i = 0; i < 4; i++) {
        float a = c->u * 12.0f + i * PI_F / 2;
        star(c->lvl, (int)(c->cx + cos_(a) * (c->W / 2.0f + 1)), (int)(cy + sin_(a) * 6), 6.5f, a, BLK, WHT);
    }
}

// Jaws: two rows of teeth close on the middle of the target, hold, then open.
static void teeth(LvlBuf lvl, int x0, int x1, int y, int dir) { // dir +1: a jaw above `y` (teeth point down)
    for(int pass = 0; pass < 2; pass++)
        for(int k = x0 + 4; k < x1; k += 9)
            for(int r = 0; r < 8; r++) {
                int hw = (7 - r) / 2 + (pass == 0 ? 1 : 0);
                span(lvl, y + dir * r + (pass == 0 && r == 7 ? dir : 0), k - hw, k + hw + 1, pass == 0 ? BLK : WHT);
            }
    for(int r = 1; r <= 5; r++) span(lvl, y - dir * r, x0, x1, BLK);
}
static void fx_fang(const Fx* c) {
    float u = c->u;
    int gap = u < 0.4f ? (int)((c->H / 2 + 6) * (1.0f - u / 0.4f)) : (u > 0.8f ? (int)((u - 0.8f) * 60) : 0);
    int x0 = c->x0 + 2, x1 = c->x1 - 2;
    teeth(c->lvl, x0, x1, c->cy - gap - 2, 1);
    teeth(c->lvl, x0, x1, c->cy + gap + 2, -1);
    if(u >= 0.4f && u < 0.55f) star(c->lvl, c->cx, c->cy, 9.0f, 0.3f, BLK, WHT);
}

static void fx_spore(const Fx* c) {
    for(int i = 0; i < 13; i++) {
        float ui = (c->u - i * 0.04f) / 0.4f;
        if(ui < 0) continue;
        if(c->u > 0.88f && hash8(i + 1) > (1.0f - c->u) * 8 * 255) continue;
        int x = c->x0 + 3 + (int)((hash8(i + 50) * (c->W - 6)) / 255) + (int)(3 * sin_(c->t / 90.0f + i));
        int y = c->y0 + 3 + (int)((hash8(i + 70) * (c->H - 6)) / 255) - (int)(c->u * 8);
        int r = 2 + (int)(fmin_(ui, 1.0f) * (1 + hash8(i + 2) % 3));
        disc(c->lvl, x, y, r, (i & 1) ? BLK : WHT, (i & 1) ? WHT : BLK);
    }
}

static void fx_whiff(const Fx* c) {
    int d = c->t / 30;
    for(int k = 0; k < 4; k++) {
        int sx = (k & 1) ? 1 : -1, sy = (k & 2) ? 1 : -1;
        bold(c->lvl, c->cx + sx * (3 + d), c->cy + sy * (3 + d), c->cx + sx * (6 + d), c->cy + sy * (6 + d));
    }
}

void ref_fx_draw(LvlBuf lvl, uint8_t kind, uint16_t t, int x0, int y0, int x1, int y1, int dir, int big) {
    uint16_t dur = fxt_duration(kind);
    if(!dur || t >= dur) return;
    Fx c = {lvl, t, x0, y0, x1, y1, x1 - x0, y1 - y0, (x0 + x1) / 2, (y0 + y1) / 2, dir, big, t / (float)dur};
    switch(kind) {
    case FX_STAR: fx_star(&c); break;
    case FX_STARS3: fx_stars3(&c); break;
    case FX_FLAME: fx_flame(&c); break;
    case FX_DROPS: fx_drops(&c); break;
    case FX_LEAVES: fx_leaves(&c); break;
    case FX_BUBBLES: fx_bubbles(&c); break;
    case FX_BOLT: fx_bolt(&c); break;
    case FX_RINGS: fx_rings(&c); break;
    case FX_ROCKS: fx_rocks(&c); break;
    case FX_SLASH: fx_slash(&c); break;
    case FX_SWARM: fx_swarm(&c); break;
    case FX_UP: fx_arrows(&c, 1); break;
    case FX_DOWN: fx_arrows(&c, 0); break;
    case FX_ZZZ: fx_zzz(&c); break;
    case FX_ORBIT: fx_orbit(&c); break;
    case FX_WAVE: fx_wave(&c); break;
    case FX_FANG: fx_fang(&c); break;
    case FX_SPORE: fx_spore(&c); break;
    case FX_WHIFF: fx_whiff(&c); break;
    default: break;
    }
}

// A projectile: a ball (or needle / rock / bolt / thread) with a short trail, from (ax, ay) to (bx, by) in `ms`.
void ref_fx_proj(LvlBuf lvl, uint8_t kind, uint16_t t, uint16_t ms, int ax, int ay, int bx, int by) {
    if(t >= ms) return;
    float p = t / (float)ms;
    int hx = ax + (int)((bx - ax) * p), hy = ay + (int)((by - ay) * p) - (int)(8.0f * sin_(p * PI_F));
    if(kind == FX_BOLT) { // a crackling beam from the user to the head
        int px_ = ax, py_ = ay, seg = 6;
        for(int k = 1; k <= seg; k++) {
            int nx = ax + (hx - ax) * k / seg, ny = ay + (hy - ay) * k / seg + (k < seg ? ((k + t / 60) & 1 ? 5 : -5) : 0);
            heavy(lvl, px_, py_, nx, ny);
            px_ = nx;
            py_ = ny;
        }
        return;
    }
    if(kind == FX_SWARM) {
        bold(lvl, hx - (bx > ax ? 12 : -12), hy, hx, hy);
        return;
    }
    for(int k = 3; k >= 1; k--) { // trail
        int tx = hx - (int)((bx - ax) * 0.07f * k), ty = hy - (int)((by - ay) * 0.07f * k);
        disc(lvl, tx, ty, k == 1 ? 3 : (k == 2 ? 2 : 1), kind == FX_FLAME ? WHT : BLK, kind == FX_FLAME ? BLK : WHT);
    }
    switch(kind) {
    case FX_ROCKS: blob(lvl, hx, hy, 6.0f, 5.0f, t / 50.0f, BLK, WHT); break;
    case FX_FLAME: disc(lvl, hx, hy, 5, WHT, BLK); disc(lvl, hx, hy, 2, BLK, BLK); break;
    default: disc(lvl, hx, hy, kind == FX_BUBBLES ? 5 : 4, WHT, BLK); break;
    }
}

#ifdef FXBAKE
// One part of a projectile drawn at (hx, hy) (the baker needs the sprites apart: the trail's offsets depend on the flight):
// part 3 / 2 / 1 = the trail discs (k), 0 = the head; `right` = the target is to the right of the user (swarm needle).
void ref_proj_part(uint8_t kind, uint16_t t, int part, int right, int hx, int hy) {
    LvlBuf lvl = NULL;
    if(kind == FX_SWARM) {
        if(!part) bold(lvl, hx - (right ? 12 : -12), hy, hx, hy);
        return;
    }
    if(part) {
        disc(lvl, hx, hy, part == 1 ? 3 : (part == 2 ? 2 : 1), kind == FX_FLAME ? WHT : BLK, kind == FX_FLAME ? BLK : WHT);
        return;
    }
    switch(kind) {
    case FX_ROCKS: blob(lvl, hx, hy, 6.0f, 5.0f, t / 50.0f, BLK, WHT); break;
    case FX_FLAME: disc(lvl, hx, hy, 5, WHT, BLK); disc(lvl, hx, hy, 2, BLK, BLK); break;
    default: disc(lvl, hx, hy, kind == FX_BUBBLES ? 5 : 4, WHT, BLK); break;
    }
}
#endif
