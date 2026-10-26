#include "render.h"
#include "gen/hero.h"
#include "assets.h"
#include <stdlib.h>
#include <string.h>

static RenderStats g_stats;
typedef struct {
    int16_t x, y; // blit top-left
    int16_t ax,
        ay; // actor anchor on the screen (the grass overlay's reference), ground position (no hop lift)
    uint8_t kind, idx; // idx: object / npc index
    uint8_t i0, j0, i1, j1; // tile box
    uint8_t dir, frame;
    int16_t key; // depth tie-break in half tiles ((i+j)*2 of the front tile; hero: real position)
} Drawable;

// Contract 70: ONE heap block holds the draw list (RENDER_MAX_DRAW x 18 B = 864 B) and, right after it, the screen-anchored pin layer (PIN_PAGES x PIN_W = 240 B, it was .bss): allocated by the first
// render_world() with a bank, freed by render_cache_release() (the battle load), so the plugin's loader has both. g_pin is only valid while g_dr is.
static Drawable* g_dr;
#define g_pin ((uint8_t(*)[PIN_W])(g_dr + RENDER_MAX_DRAW))
static int16_t g_pin_x0, g_pin_y0, g_pin_x1,
    g_pin_y1; // bounding box of what was pinned (x1 / y1 exclusive); empty when x0 >= x1
static bool g_shadows = true;
static bool g_grass_overlay = true;

void render_set_grass_overlay(bool on) {
    g_grass_overlay = on;
}

void render_set_shadows(bool on) {
    g_shadows = on;
}

const RenderStats* render_stats(void) {
    return &g_stats;
}

const uint8_t* render_pin(void) {
    return g_dr ? &g_pin[0][0] : NULL;
}

static inline void pin_set(int x, int y, bool on) {
    int px = x - PIN_X0;
    if((unsigned)px >= PIN_W || (unsigned)y >= PIN_H) return;
    uint8_t bit = (uint8_t)(1 << (y & 7));
    uint8_t* p = &g_pin[y >> 3][px];
    if(on) {
        *p |= bit;
        if(x < g_pin_x0) g_pin_x0 = (int16_t)x;
        if(x >= g_pin_x1) g_pin_x1 = (int16_t)(x + 1);
        if(y < g_pin_y0) g_pin_y0 = (int16_t)y;
        if(y >= g_pin_y1) g_pin_y1 = (int16_t)(y + 1);
    } else {
        *p &= (uint8_t)~bit;
    }
}

// Marks (or unmarks) the opaque pixels of a sprite drawn at (x, y) -- the same pixels blit_auto would write.
static void pin_blit(const Blit* b, int x, int y, bool on) {
    int x0 = x > PIN_X0 ? x : PIN_X0, x1 = x + b->w < PIN_X0 + PIN_W ? x + b->w : PIN_X0 + PIN_W;
    int y0 = y > 0 ? y : 0, y1 = y + b->h < PIN_H ? y + b->h : PIN_H;
    if(!on) { // un-pinning only matters where something is pinned
        if(x0 < g_pin_x0) x0 = g_pin_x0;
        if(x1 > g_pin_x1) x1 = g_pin_x1;
        if(y0 < g_pin_y0) y0 = g_pin_y0;
        if(y1 > g_pin_y1) y1 = g_pin_y1;
    }
    if(x0 >= x1 || y0 >= y1) return;
    if(on) { // the bounding box grows by the sprite's rectangle (transparent border included: it only bounds the un-pin work)
        if(x0 < g_pin_x0) g_pin_x0 = (int16_t)x0;
        if(x1 > g_pin_x1) g_pin_x1 = (int16_t)x1;
        if(y0 < g_pin_y0) g_pin_y0 = (int16_t)y0;
        if(y1 > g_pin_y1) g_pin_y1 = (int16_t)y1;
    }
    for(int py = y0; py < y1; py++) {
        const int sy = py - y;
        const uint8_t bit = (uint8_t)(1 << (py & 7));
        uint8_t* col = g_pin[py >> 3];
        const uint8_t* mrow = b->mask ? b->mask + sy * b->mbpr : NULL;
        for(int px = x0; px < x1; px++) {
            const int k = px - x, sx = b->flip ? b->w - 1 - k : k;
            const int op = mrow ? (mrow[sx >> 3] >> (sx & 7)) & 1 : blit_px(b, sy, sx) != 0;
            if(!op) continue;
            if(on)
                col[px - PIN_X0] |= bit;
            else
                col[px - PIN_X0] &= (uint8_t)~bit;
        }
    }
}

// The hero's sprite data (18x35, 5 stored facings x 5 frames, pixels + mask = 1,400 B per facing) is NOT in the image: it
// lives in files/hero.bin and ONE facing is cached in RAM (contract 5, Memory). gen/hero.h stores S SE E NE N only
// (PCHARW_DIRS = 5): NW / W / SW are the pixel-exact mirrors of NE / E / SE (tools/pl2h.py --fold-mirror checks it),
// drawn with Blit.flip. A turn reads the new facing from the card (one open + read, a few ms, game thread) the first time
// it is drawn; walking in a line never touches the card.
#define HERO_FILE "hero.bin"
#define DARK_LIFT \
    10 // the light is centred this many px above the hero's feet (his whole sprite stays inside it)
#define HERO_BIKE_FILE \
    "hero_bike.bin" // C4-A (contract 35.4): the same format with the hero on the bicycle; optional (tools/pl2h.py --bin)
#define HERO_SURF_FILE \
    "hero_surf.bin" // C5 (contract 42.1): the same with the hero on the board while he surfs; optional (tools/mkbike.py builds it from assets/art/hero_surf)
#define HERO_HDR        16
#define HERO_PIX_BYTES  (PCHARW_FRAMES * PCHARW_BPR * PCHARW_H)
#define HERO_MASK_BYTES (PCHARW_FRAMES * PCHARW_MBPR * PCHARW_H)
#define HERO_DIR_BYTES  (HERO_PIX_BYTES + HERO_MASK_BYTES)

// HERO_CACHE_DIRS facings are kept (1 = 1.4 KB, the least; 2 = 2.8 KB keeps a zig-zag between two facings off the card). Each one is a heap
// block allocated when the facing is first drawn (it was .bss until MEM3): the battle load frees it (render_cache_release) so the plugin's
// loader has 1.4 KB more, and the first draw after it (the battle's back view) reads the facing again.
#ifndef HERO_CACHE_DIRS
#define HERO_CACHE_DIRS 1
#endif
static struct {
    bool hdr_ok; // hero.bin's header matched gen/hero.h
    uint8_t tick; // LRU clock
    struct {
        uint8_t dir,
            used; // dir: the stored facing 0..4, + 8 for the bicycle sprites (hero_bike.bin), + 16 for the surfing ones (hero_surf.bin)
        bool have;
        uint8_t*
            d; // FRAMES x pixels, then FRAMES x masks (file order); NULL until the slot is first used / after render_cache_release
    } e[HERO_CACHE_DIRS];
} g_hero; // zero-initialised: .bss, not .data
#ifndef NUCLEO_DEVICE // contract 70: the card-read timings feed the host tests (contract 73: and no longer the DEV overlay's H field)
#define STAT_ON 1
static HeroLoadStats g_hero_stats;
#endif
static uint8_t
    g_set; // render_world() sets it while the hero is drawn: 1 = riding (the bicycle sprites), 2 = surfing; everybody else (battle, title, naming) gets the walking hero
static uint8_t
    g_nofile; // bit s: files/hero_bike.bin (s = 1) / hero_surf.bin (s = 2) is missing or short: the walking sprites are used meanwhile (and bob 1 px), the file is not tried again

#ifndef NUCLEO_DEVICE
void render_hero_reset(
    void) { // host tests: forget which hero files were found (the bicycle file is probed once per run)
    render_cache_release();
    g_hero.hdr_ok = false;
    g_hero.tick = 0;
    g_nofile = 0;
    memset(g_hero.e, 0, sizeof(g_hero.e));
    g_set = 0;
}
#endif

#ifdef STAT_ON
const HeroLoadStats* hero_load_stats(void) {
    return &g_hero_stats;
}
#endif

static bool hero_load_dir(
    uint8_t dir,
    uint8_t* dst) { // dir >> 3: the sprite set (0 walking, 1 bicycle, 2 surfing)
#ifdef STAT_ON
    uint32_t t0 = asset_tick();
#endif
    const uint8_t set = dir >> 3;
    dir &= 7;
    if(!set && !g_hero.hdr_ok) {
        uint8_t h[HERO_HDR];
        if(!read_at(HERO_FILE, 0, h, sizeof(h)) || memcmp(h, "NHRO", 4) != 0 || h[4] != 1 ||
           h[5] != PCHARW_W || h[6] != PCHARW_H || h[7] != PCHARW_BPR || h[8] != PCHARW_MBPR ||
           h[9] != PCHARW_FRAMES || h[10] != PCHARW_DIRS)
            return false;
        g_hero.hdr_ok = true;
    }
    if(!read_at(
           set == 2 ? HERO_SURF_FILE :
           set      ? HERO_BIKE_FILE :
                      HERO_FILE,
           HERO_HDR + (uint32_t)dir * HERO_DIR_BYTES,
           dst,
           HERO_DIR_BYTES))
        return false; // (the bicycle / surf files are optional, without a header check)
#ifdef STAT_ON
    uint32_t ms = asset_tick() - t0;
    g_hero_stats.loads++;
    g_hero_stats.last_ms = ms;
    if(ms > g_hero_stats.max_ms) g_hero_stats.max_ms = ms;
#endif
    return true;
}

void hero_view(uint8_t dir, uint8_t frame, Blit* out) {
    static const uint8_t none[1] = {0};
    dir &= 7;
    uint8_t flip = dir >= PCHARW_DIRS;
    if(flip) dir = (uint8_t)(8 - dir);
    if(frame >= PCHARW_FRAMES) frame = (uint8_t)(frame % PCHARW_FRAMES);
    int hit = -1, lru = 0;
    if(!((g_nofile >> g_set) & 1)) dir |= (uint8_t)(g_set << 3);
    for(int k = 0; k < HERO_CACHE_DIRS; k++) {
        if(g_hero.e[k].have && g_hero.e[k].dir == dir) hit = k;
        if(!g_hero.e[k].have ||
           (uint8_t)(g_hero.tick - g_hero.e[k].used) > (uint8_t)(g_hero.tick - g_hero.e[lru].used))
            lru = k;
    }
    if(hit < 0) { // a turn: read the facing into the least recently used slot
        hit = lru;
        if(!g_hero.e[hit].d && heap_can_alloc(HERO_DIR_BYTES))
            g_hero.e[hit].d = malloc(HERO_DIR_BYTES);
        g_hero.e[hit].have = g_hero.e[hit].d && hero_load_dir(dir, g_hero.e[hit].d);
        if(!g_hero.e[hit].have && g_hero.e[hit].d &&
           (dir & 24)) { // no bicycle / surf art: the walking hero from now on
            g_nofile |= (uint8_t)(1 << (dir >> 3));
            dir &= 7;
            g_hero.e[hit].have = hero_load_dir(dir, g_hero.e[hit].d);
        }
        g_hero.e[hit].dir = dir;
        if(!g_hero.e[hit].have && g_hero.e[hit].d)
            memset(
                g_hero.e[hit].d,
                0,
                HERO_DIR_BYTES); // card / file problem: an invisible hero until a load works (retried at every draw)
    }
    g_hero.e[hit].used = ++g_hero.tick;
    const uint8_t* d = g_hero.e[hit].d;
    if(!d) { // no heap for the cache: nothing is drawn (0 x 0), retried at the next draw
        *out = (Blit){.pix = none, .mask = none, .flip = flip};
        return;
    }
    out->pix = d + frame * (PCHARW_BPR * PCHARW_H);
    out->mask = d + HERO_PIX_BYTES + frame * (PCHARW_MBPR * PCHARW_H);
    out->flip = flip;
    out->w = PCHARW_W;
    out->h = PCHARW_H;
    out->bpr = PCHARW_BPR;
    out->mbpr = PCHARW_MBPR;
}

void render_camera(int32_t fi, int32_t fj, int* camx, int* camy) {
    *camx = (int)(((fi - fj) * (TW / 2)) >> 8);
    *camy = (int)(((fi + fj) * (TH / 2)) >> 8);
}

// ---------------------------------------------------------------- ground
static inline int fdiv(int a, int b) { // floor(a / b), b > 0
    int q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}
static inline int cdiv(int a, int b) { // ceil(a / b), b > 0
    return -fdiv(-a, b);
}

// Shaking grass: the tile's art is nudged left / up / right / up every phase and a few taller blades flick above it.
static const int8_t RUSTLE_DX[RUSTLE_PHASES] = {-1, 0, 1, 0};
static const int8_t RUSTLE_DY[RUSTLE_PHASES] = {0, -1, 0, -1};

static void draw_rustle(LvlBuf lvl, int cx, int cy, uint8_t ph) {
    // the tile is cleared to white and five bold black blades (2 px wide) sway and grow out of phase: it stands out of the
    // grass pattern even at a glance
    for(int k = 0; k < 5; k++) {
        int bx = cx - 10 + k * 5, by = cy + 3 - ((k * 2) % 3);
        int hgt = 5 + ((ph + k * 3) & 3) * 2; // 5, 7, 9, 11
        int lean = (((ph + k) & 3) < 2) ? -1 : 1;
        for(int r = 0; r < hgt; r++) {
            int x = bx + (r >= hgt - 4 ? lean * (r - (hgt - 4) + 1) / 2 : 0);
            lvl_putc(lvl, x, by - r, 3);
            lvl_putc(lvl, x + 1, by - r, r >= hgt - 2 ? 0 : 2);
        }
    }
}

uint32_t render_anim_step; // set by the world scene (clock / ANIM_MS)
static inline uint8_t anim_frame(
    const SprEntry*
        e) { // the frame of an animated tile / object (only maps with MAPF_ANIM have sprites with nframes > 1)
    return e->nframes > 1 ? (uint8_t)(render_anim_step % e->nframes) : 0;
}

static void draw_ground(LvlBuf lvl, const Map* m, int camx, int camy, const RenderHero* hero) {
    const int w = m->w, h = m->h;
    // tile centre: cx = 64 + u*14 - camx (u = i-j), cy = 32 + v*7 - camy (v = i+j).
    // Only tiles whose centre lies within M px of the screen are visited (M covers the largest
    // tile sprite); sprites that still miss the screen are culled per tile below.
    const int M = 24;
    int v_lo = cdiv(camy - 32 - M, TH / 2), v_hi = fdiv(camy - 32 + SCREEN_H + M, TH / 2);
    int ulo = cdiv(camx - 64 - M, TW / 2), uhi = fdiv(camx - 64 + SCREEN_W + M, TW / 2);
    if(v_lo < 0) v_lo = 0;
    if(v_hi > w + h - 2) v_hi = w + h - 2;
    for(int v = v_lo; v <= v_hi; v++) {
        int cy = 32 + v * (TH / 2) - camy;
        int u_min = -v, u_max = v; // i >= 0, j >= 0
        if(v - 2 * h + 2 > u_min) u_min = v - 2 * h + 2; // j <= h-1
        if(2 * w - 2 - v < u_max) u_max = 2 * w - 2 - v; // i <= w-1
        if(ulo > u_min) u_min = ulo;
        if(uhi < u_max) u_max = uhi;
        if((u_min ^ v) & 1) u_min++; // u and v share parity
        for(int u = u_min; u <= u_max; u += 2) {
            int i = (v + u) / 2, j = (v - u) / 2;
            uint8_t g = m->ground[j * w + i];
            if(g == 0xFF) continue;
            const SprEntry* e = bank_spr(m->bank, g);
            int cx = 64 + u * (TW / 2) - camx;
            if(e->fill != 0xFF) { // underlay: flat diamond first, tile pixels on top
                fill_diamond(lvl, cx, cy, e->fill);
                g_stats.fills++;
            }
            int x = cx - e->ax, y = cy - e->ay;
            uint8_t rph = (hero && hero->rustle_ph && i == hero->rustle_i && j == hero->rustle_j) ?
                              hero->rustle_ph :
                              0;
            if(rph) {
                x += RUSTLE_DX[rph - 1];
                y += RUSTLE_DY[rph - 1];
            }
            if(x >= SCREEN_W || y >= SCREEN_H || x + e->w <= 0 || y + e->h <= 0) continue;
            Blit b;
            bank_view(m->bank, g, 0, anim_frame(e), &b);
            if(rph) fill_diamond(lvl, cx, cy, 0);
            blit_auto(lvl, &b, x, y);
            if(rph) draw_rustle(lvl, cx, cy, rph);
            g_stats.tiles++;
        }
    }
}

// ---------------------------------------------------------------- drawables
enum {
    K_OBJ,
    K_NPC,
    K_HERO
};
#define PROP_KEY_BONUS \
    8 // half tiles: a prop NPC on furniture sorts after the furniture (3x1 bench: up to 4 tiles)

static int g_ndr;

void render_cache_release(void) {
    for(int k = 0; k < HERO_CACHE_DIRS; k++) {
        free(g_hero.e[k].d);
        g_hero.e[k].d = NULL;
        g_hero.e[k].have = false;
    }
    free(g_dr);
    g_dr = NULL;
    g_ndr = 0;
    map_stream_release(); // the streamed NPC frames (map.c): read again when the NPC is next drawn
}

#ifndef NUCLEO_DEVICE
uint32_t render_cache_bytes(
    void) { // host tests: what the renderer holds in heap blocks (hero facing + draw list + the streamed NPC frames)
    uint32_t n = g_dr ? RENDER_MAX_DRAW * (uint32_t)sizeof(Drawable) + PIN_PAGES * PIN_W : 0;
    for(int k = 0; k < HERO_CACHE_DIRS; k++)
        if(g_hero.e[k].d) n += HERO_DIR_BYTES;
    return n + map_stream_cache_bytes();
}
#endif

static inline int on_screen(int x, int y, int w, int h) {
    return x < SCREEN_W && y < SCREEN_H && x + w > 0 && y + h > 0;
}

// ---------------------------------------------------------------- depth cues (shadows on the ground)
// Drawn into lvl right after the ground and before any drawable, so objects / actors in front
// always cover them and they only ever darken the floor (never the black void outside a room).
static const uint8_t ACTOR_SHADOW_W[] = {6, 12, 14, 12, 6}; // flat iso ellipse under the feet
static const uint8_t BLOB_BIG_W[] = {8, 14, 18, 14, 8}; // trees
static const uint8_t BLOB_SMALL_W[] = {6, 12, 12, 6}; // rocks, signs

// The hero's shadow raises the ground to SHADOW_ACTOR_LV: those pixels are the hero's (screen-anchored), call BEFORE actor_shadow.
static void pin_actor_shadow(LvlBuf lvl, int ax, int ay) {
    if(!g_shadows) return;
    for(int r = 0; r < (int)sizeof(ACTOR_SHADOW_W); r++) {
        int y = ay - 1 + r, x0 = ax - ACTOR_SHADOW_W[r] / 2;
        if((unsigned)y >= PIN_H) continue;
        for(int k = 0; k < ACTOR_SHADOW_W[r]; k++)
            if((unsigned)(x0 + k) < SCREEN_W && lvl_get(lvl, x0 + k, y) < SHADOW_ACTOR_LV)
                pin_set(x0 + k, y, true);
    }
}

static __attribute__((noinline)) void actor_shadow(LvlBuf lvl, int ax, int ay) {
    if(!g_shadows) return;
    // ax: anchor column (tile centre), ay: anchor row (tile centre + ACTOR_FOOT_DY): the feet end
    // 0..+3 rows below ay; the ellipse covers ay-1 .. ay+3: the legs hide its top, 2 rows and the sides show around the shoes.
    shade_rows(lvl, ax, ay - 1, ACTOR_SHADOW_W, (int)sizeof(ACTOR_SHADOW_W), SHADOW_ACTOR_LV);
}

// Object contact shadow, selected by the catalog `fill` of the OBJECT (bank directory byte):
//   0 / 0xFF none | 1, 2 band along the front edges, `level` at the object, fading by 1 per row
//   3 blob (level 2) centred on the footprint tile: big for tall sprites (trees), else small.
static void object_shadow(LvlBuf lvl, const SprEntry* e, int bx, int by) {
    uint8_t f = e->fill;
    if(!g_shadows || f == 0 || f > 3) return; // 0xFF = none
    if(f < 3) {
        int tall = e->h >= 30;
        shade_front_edges(
            lvl,
            bx,
            by,
            e->fw,
            e->fh,
            tall ? SHADOW_K_TALL_J : SHADOW_K_LOW_J,
            tall ? SHADOW_K_TALL_I : SHADOW_K_LOW_I,
            f);
    } else if(e->h >= 20) {
        shade_rows(lvl, bx, by - 8, BLOB_BIG_W, (int)sizeof(BLOB_BIG_W), 2);
    } else {
        shade_rows(lvl, bx, by - 6, BLOB_SMALL_W, (int)sizeof(BLOB_SMALL_W), 2);
    }
}

static void add_drawable(
    int x,
    int y,
    uint8_t kind,
    uint8_t idx,
    int i0,
    int j0,
    int i1,
    int j1,
    uint8_t dir,
    uint8_t frame,
    int key,
    int ax,
    int ay) {
    if(g_ndr >= RENDER_MAX_DRAW || !g_dr) {
        g_stats.overflow++;
        return;
    }
    Drawable* d = &g_dr[g_ndr++];
    d->x = (int16_t)x;
    d->y = (int16_t)y;
    d->kind = kind;
    d->idx = idx;
    d->i0 = (uint8_t)i0;
    d->j0 = (uint8_t)j0;
    d->i1 = (uint8_t)i1;
    d->j1 = (uint8_t)j1;
    d->dir = dir;
    d->frame = frame;
    d->key = (int16_t)key;
    d->ax = (int16_t)ax;
    d->ay = (int16_t)ay;
}

// A must be drawn before B (A is strictly behind B along i or j).
static inline int behind(const Drawable* a, const Drawable* b) {
    return a->i1 < b->i0 || a->j1 < b->j0;
}

// Selection sort: repeatedly emit a drawable that has no unplaced drawable strictly required
// before it; mutual constraints (side by side, never overlapping in depth) are ignored; if all
// are blocked (cycle) or several are free, the smallest depth key wins (then the lowest index).
static void sort_drawables(void) {
    int n = g_ndr;
    for(int pos = 0; pos < n; pos++) {
        int best = -1, best_key = 0, fb = -1, fb_key = 0;
        for(int a = pos; a < n; a++) {
            int blocked = 0;
            for(int b = pos; b < n && !blocked; b++) {
                if(b != a && behind(&g_dr[b], &g_dr[a]) && !behind(&g_dr[a], &g_dr[b]))
                    blocked = 1;
            }
            int key = g_dr[a].key;
            if(!blocked) {
                if(best < 0 || key < best_key) {
                    best = a;
                    best_key = key;
                }
            } else if(fb < 0 || key < fb_key) {
                fb = a;
                fb_key = key;
            }
        }
        if(best < 0) best = fb;
        if(best != pos) {
            Drawable t = g_dr[pos];
            g_dr[pos] = g_dr[best];
            g_dr[best] = t;
        }
    }
}

// ---------------------------------------------------------------- tall grass over the feet
// A band of grass blades in front of an actor standing on a C_GRASS tile: columns dx = -GRASS_HW..GRASS_HW, each from a
// jagged tip row down to an elliptical bottom edge that stays inside the tile's diamond. White body (it hides the legs),
// level-1 / level-2 blade strokes and level-3 tips. `ay` is the actor anchor row (tile centre + ACTOR_FOOT_DY).
#define GRASS_HW 11
// Blades in front of the feet: spiky tufts (a tall centre blade between shorter ones, period 6 px). Blade columns are
// level-2 strokes with a level-3 tip, the rest is white (it hides the legs) with level-1 specks, over a dark base row.
static const int8_t GRASS_PROFILE[6] = {10, 7, 5, 4, 5, 7}; // blade height by (dx + 12) % 6

static void grass_overlay(LvlBuf lvl, int ax, int ay, bool pin) {
    for(int dx = -GRASS_HW; dx <= GRASS_HW; dx++) {
        int adx = dx < 0 ? -dx : dx;
        int yb = ay + 4 - adx / 3; // bottom row of this column (inside the tile's diamond)
        int c = (dx + 12) % 6;
        int top = yb - GRASS_PROFILE[c]; // tip row
        for(int y = top; y <= yb; y++) {
            int v;
            if(y == top)
                v = 3; // blade tip
            else if(y == yb)
                v = 3; // dark base
            else if(y <= top + 2)
                v = 2; // blade stroke
            else
                v = (((dx + y) & 3) == 0 || ((dx - y) & 7) == 0) ?
                        2 :
                        1; // light-gray body with darker blade strokes: hides the legs
            lvl_putc(lvl, ax + dx, y, (uint8_t)v);
            if(pin) pin_set(ax + dx, y, true);
        }
    }
}

// Dark maps (C4-A, contract 35.1): once the world is drawn, everything outside the diamond (half-height r, half-width 2r: the shape of a 2-tile radius) centred on (cx, cy) becomes black: the classic
// small circle of light. Per screen row two memsets blacken the bytes (4 pixels) that lie wholly left / right of the diamond, so its edge steps by 4 px; no buffer, nothing in the tile / object / NPC loops.
static void darken(LvlBuf lvl, int cx, int cy, int r) {
    for(int y = 0; y < SCREEN_H; y++) {
        int hw = 2 * (r - abs(y - cy)), a, b;
        if(hw < 0) hw = 0;
        a = (cx - hw) >> 2; // bytes [0, a) are wholly left of the light
        b = (cx + hw + 3) >> 2; // bytes [b, LVL_STRIDE) wholly right of it
        a = a < 0 ? 0 : a > LVL_STRIDE ? LVL_STRIDE : a;
        b = b < 0 ? 0 : b > LVL_STRIDE ? LVL_STRIDE : b;
        memset(lvl[y], 0xFF, (size_t)a);
        memset(lvl[y] + b, 0xFF, (size_t)(LVL_STRIDE - b));
    }
}

// Fog (C4b, contract 40.5; `@fog yes` = MAPF_FOG): once the world is drawn, bands of mist lighten the mid-tones one step (level 2 -> 1, level 1 -> 0; white and black stay) through an ordered 4 x 4 dither
// whose density follows a slow sine over the rows (FOG_COV: peak 6 / 16 = about 0.32 of the pixels, bands about every 5 px), and the whole pattern drifts one pixel per FOG_MS (f = the step, 0..31). Like the dark pass
// it is one loop over the finished buffer: no allocation, no float; per row the dither gives ONE byte mask (4 pixels x 2 bits, the same for all 32 bytes of the row) and each byte is lightened with two shifts and a mask
// (hi' = hi & lo, lo' = hi on the selected pixels); a row with no coverage costs one table read. The text box, the menus and the banner are drawn after render_world, so they are not fogged.
static const uint8_t FOG_COV[32] = {3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 5, 5, 5, 4, 4, 4,
                                    3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 3};
static const uint8_t FOG_THR[16] = {
    0,
    8,
    2,
    10,
    12,
    4,
    14,
    6,
    3,
    11,
    1,
    9,
    15,
    7,
    13,
    5}; // gen_tiles.dither(): [(x & 3) * 4 + (y & 3)]
static void fog_pass(LvlBuf lvl, unsigned f) {
    for(unsigned y = 0; y < SCREEN_H; y++) {
        const unsigned c = FOG_COV[(y + f) & 31];
        unsigned m = 0;
        for(unsigned k = 0; k < 4; k++) // screen column x & 3 == k takes dither column (x + f) & 3
            if(FOG_THR[((k + f) & 3) * 4 + (y & 3)] < c) m |= 3u << (2 * k);
        if(!m) continue;
        uint8_t* r = lvl[y];
        for(unsigned b = 0; b < LVL_STRIDE; b++) {
            const unsigned v = r[b], h = (v >> 1) & 0x55;
            r[b] = (uint8_t)((v & ~m) | ((h | ((h & v) << 1)) & m));
        }
    }
}

void render_world(
    LvlBuf lvl,
    const Map* m,
    int camx,
    int camy,
    const RenderHero* hero,
    const NpcState* npcs) {
    int hx = 0, hy = 0; // the hero's anchor on the screen (the light of a dark map)
    memset(&g_stats, 0, sizeof(g_stats));
    if(g_dr) memset(g_pin, 0, PIN_PAGES * PIN_W);
    map_stream_begin(); // the streamed NPC frames drawn from here on are protected from eviction until the next frame
    g_pin_x0 = g_pin_y0 = SCREEN_W;
    g_pin_x1 = g_pin_y1 = 0;
    if(!m->bank) { // the bank is released (world_art_release): nothing to draw
        lvl_fill(lvl, m->bg_level & 3);
        return;
    }
    lvl_fill(lvl, m->bg_level & 3);
    if(!m->w || !m->h) return;

    draw_ground(lvl, m, camx, camy, hero);

    // ---- gather drawables (screen-bounds culled)
    g_ndr = 0;
    if(!g_dr && heap_can_alloc(RENDER_MAX_DRAW * sizeof(Drawable) + PIN_PAGES * PIN_W))
        g_dr = calloc(1, RENDER_MAX_DRAW * sizeof(Drawable) + PIN_PAGES * PIN_W);
    if(!g_dr) { // no heap for the draw list (cannot happen outside a battle): the ground only, retried at the next frame
        g_stats.overflow = 255;
        return;
    }
    for(uint16_t k = 0; k < m->n_objs; k++) {
        const ObjDef* o = &m->objs[k];
        const SprEntry* e = bank_spr(m->bank, o->spr);
        int i1 = o->i0 + e->fw - 1, j1 = o->j0 + e->fh - 1;
        int ax = 64 + (i1 - j1) * (TW / 2) - camx; // bottom vertex of the front tile
        int ay = 32 + (i1 + j1) * (TH / 2) - camy + TH / 2;
        int x = ax - e->ax, y = ay - e->ay;
        if(on_screen(x, y, e->w, e->h + SHADOW_PAD)) object_shadow(lvl, e, ax, ay);
        if(!on_screen(x, y, e->w, e->h)) continue;
        g_stats.objs_seen++;
        add_drawable(
            x, y, K_OBJ, (uint8_t)k, o->i0, o->j0, i1, j1, 0, anim_frame(e), (i1 + j1) * 2, ax, ay);
    }
    for(uint8_t k = 0; k < m->n_npcs; k++) {
        const NpcDef* n = &m->npcs[k];
        if(npcs && !npcs[k].visible) continue;
        const uint8_t sp =
            npcs ?
                npcs[k].spr :
                n->spr; // (the runtime sprite: a roamer's species, contract 55; FOL_SPR = the follower the world scene adds as one more NPC, contract 69: its species is the frame)
        const SprEntry* e = sp == FOL_SPR ? &g_fol_spr : bank_spr(m->bank, sp);
        // runtime position (Q8 tile units) when the world supplies it, else the map tile
        int32_t fi = npcs ? npcs[k].fi : n->i * 256, fj = npcs ? npcs[k].fj : n->j * 256;
        int ti = npcs ? npcs[k].near_i : n->i, tj = npcs ? npcs[k].near_j : n->j;
        int ax = 64 + (int)(((fi - fj) * (TW / 2)) >> 8) - camx;
        int ay = 32 + (int)(((fi + fj) * (TH / 2)) >> 8) - camy + ACTOR_FOOT_DY;
        int lift =
            npcs ?
                npcs[k].lift :
                0; // hop bob: the sprite rises, the shadow and the grass band stay on the ground
        int x = ax - e->ax, y = ay - e->ay - lift;
        if(!on_screen(x, y, e->w, e->h + lift)) continue;
        // an NPC standing on a solid tile is a prop on a piece of furniture (the Crystals on the lab bench): no ground
        // shadow, and it is drawn after that furniture (key bonus > the object's own depth key), never before it
        bool prop = (cflags_at(m, ti, tj) & C_SOLID) != 0;
        if(!prop) actor_shadow(lvl, ax, ay);
        add_drawable(
            x,
            y,
            K_NPC,
            k,
            ti,
            tj,
            ti,
            tj,
            npcs ? npcs[k].facing : n->facing,
            npcs ? npcs[k].frame : 0,
            (int)((fi + fj) >> 7) + (prop ? PROP_KEY_BONUS : 0),
            ax,
            ay);
    }
    if(hero && hero->draw) {
        int ax = 64 + (int)(((hero->fi - hero->fj) * (TW / 2)) >> 8) - camx;
        int ay = 32 + (int)(((hero->fi + hero->fj) * (TH / 2)) >> 8) - camy + ACTOR_FOOT_DY;
        // C4-A / C5: riding or surfing without the art (files/hero_bike.bin / hero_surf.bin): the walking sprite bobs 1 px on every other walk frame (g_nofile bit 0 is never set: a walking hero does not bob)
        int bob = ((g_nofile >> hero->bike) & 1) && (hero->frame & 1);
        hx = ax;
        hy = ay;
        int x = ax - HERO_AX, y = ay - HERO_AY - hero->lift - bob;
        if(on_screen(x, y, PCHARW_W, PCHARW_H + hero->lift + bob)) {
            pin_actor_shadow(lvl, ax, ay);
            actor_shadow(lvl, ax, ay);
            add_drawable(
                x,
                y,
                K_HERO,
                0,
                hero->i0,
                hero->j0,
                hero->i1,
                hero->j1,
                hero->dir,
                0,
                (int)((hero->fi + hero->fj) >> 7),
                ax,
                hero->lift ? -1000 : ay); // -1000: airborne, no grass
        }
    }

    sort_drawables();
    g_stats.drawables = (uint8_t)g_ndr;

    // ---- draw (every sprite un-pins the pixels it covers; the hero then pins his own)
    g_set =
        hero ?
            hero->bike :
            0; // (hero_view: the bicycle / surfing sprites while he rides / surfs; reset below so the battle / title get the walking hero)
    for(int k = 0; k < g_ndr; k++) {
        const Drawable* d = &g_dr[k];
        Blit b;
        bool ok;
        if(d->kind == K_OBJ) {
            ok = bank_view(m->bank, m->objs[d->idx].spr, 0, d->frame, &b);
        } else if(d->kind == K_NPC) {
            ok = map_actor_view(
                m, npcs ? npcs[d->idx].spr : m->npcs[d->idx].spr, d->dir, d->frame, &b);
        } else {
            hero_view(d->dir, hero->frame, &b);
            ok = true;
        }
        if(ok) {
            pin_blit(&b, d->x, d->y, d->kind == K_HERO);
            blit_auto(lvl, &b, d->x, d->y);
        }
        if(g_grass_overlay && d->kind != K_OBJ && d->ay != -1000 &&
           (cflags_at(m, d->i1, d->j1) & C_GRASS))
            grass_overlay(lvl, d->ax, d->ay, d->kind == K_HERO);
    }

    g_set = 0;

    // ---- the "!" balloon above a trainer that just saw the hero: MAPF_BALLOON maps carry it as their LAST bank sprite (an object
    // whose anchor is the tail tip); it sits on top of everything, its tip one pixel above the NPC sprite's top row
    if(hero && hero->balloon && (m->flags & MAPF_BALLOON) && m->n_spr > 0) {
        const uint8_t bidx = (uint8_t)(m->n_spr - 1);
        const SprEntry* be = bank_spr(m->bank, bidx);
        for(int k = 0; be && k < g_ndr; k++) {
            const Drawable* d = &g_dr[k];
            Blit b;
            if(d->kind == K_NPC && d->idx == hero->balloon - 1 &&
               bank_view(m->bank, bidx, 0, 0, &b)) {
                pin_blit(&b, d->ax - be->ax, d->y - 1 - be->ay, false);
                blit_auto(lvl, &b, d->ax - be->ax, d->y - 1 - be->ay);
            }
        }
    }
    if(hero && hero->dark) darken(lvl, hx, hy - DARK_LIFT, hero->dark);
    if(hero && hero->fog) fog_pass(lvl, hero->fog - 1u);
}
