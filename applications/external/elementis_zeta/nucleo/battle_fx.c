#include "battle_fx.h"
#include "fx_tab.h"
#include "assets.h"
#include "gen/reg_gen.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Battle effects (contract 10.4).  The ARTWORK is data: tools/fxbake renders the original per-pixel painters (tools/fxbake/fxref.c)
// into nucleo/files/fx.bin, and this file is the small player that streams one effect's frames from the card while it plays.
// Code that is left: the tables (move styles, durations), the frame player, a projectile's flight, the crackling beam of an
// electric projectile (it joins the user to the head: no sprite can be baked) and the capture crystal.
// Everything is drawn BLACK and WHITE (levels 3 / 0): a gray body flickers and reads weakly on the real LCD.

#define BLK 3
#define WHT 0

// ------------------------------------------------------------------------------------------------ the arc
// The projectile's lob, 8 sin(pi x) floored (x = the flight's fraction): the 4096ths of the flight where it first reaches 1 .. 8 pixels (no float, no libm: contract 82 give-back)
static const uint16_t ARC_AT[8] = {164, 330, 502, 683, 881, 1106, 1390, 2048};
static int arc_(uint16_t t, uint16_t ms) {
    uint32_t x = (uint32_t)(t < ms - t ? t : ms - t) * 4096;
    int k = 0;
    while(k < 8 && x >= (uint32_t)ARC_AT[k] * ms) k++;
    return k;
}

// ------------------------------------------------------------------------------------------------ tables
static const uint8_t TYPE_FX[TY_COUNT] = {
    [TY_NORMAL] = FX_STAR,     [TY_FIGHTING] = FX_STARS3, [TY_FLYING] = FX_SLASH, [TY_POISON] = FX_BUBBLES,
    [TY_GROUND] = FX_ROCKS,    [TY_ROCK] = FX_ROCKS,      [TY_BUG] = FX_SWARM,    [TY_GHOST] = FX_RINGS,
    [TY_FIRE] = FX_FLAME,      [TY_WATER] = FX_DROPS,     [TY_GRASS] = FX_LEAVES, [TY_ELECTRIC] = FX_BOLT,
    [TY_PSYCHIC] = FX_RINGS,   [TY_ICE] = FX_RINGS,      [TY_DRAGON] = FX_RINGS,
};

#define C_ MST_CONTACT
#define P_ MST_PROJ
#define B_ MST_BIG
// Per move id (moves.toml order). 0 = no effect (a status move that only hops, then its stat arrows play).
static const uint8_t MV_STYLE[MV_WAVEWALL + 1] = { // (ids past the last one listed have no style: contract 64 appended 66 moves)
    [MV_BUMP] = C_ | FX_STAR,        [MV_CLAW] = C_ | FX_SLASH,      [MV_EMBER] = P_ | FX_FLAME,   [MV_JET] = P_ | FX_DROPS,
    [MV_LASH] = FX_LEAVES,           [MV_QUICK] = C_ | FX_STAR,      [MV_SLASH] = C_ | B_ | FX_SLASH, [MV_SPORE] = FX_SPORE,
    [MV_VENOMDUST] = FX_SPORE,       [MV_BITE] = C_ | FX_FANG,       [MV_BUBBLE] = P_ | FX_BUBBLES, [MV_FLAP] = FX_SLASH,
    [MV_DUSTKICK] = FX_SPORE,        [MV_SILKSHOT] = P_ | FX_SPORE,    [MV_NEEDLE] = P_ | FX_SWARM,  [MV_LANCE] = C_ | FX_SWARM,
    [MV_ZAP] = P_ | FX_BOLT,         [MV_NUMB] = P_ | FX_BOLT,       [MV_ARC] = P_ | B_ | FX_BOLT, [MV_SWOOP] = C_ | B_ | FX_SLASH,
    [MV_CHOMP] = C_ | B_ | FX_FANG,  [MV_PEBBLE] = P_ | FX_ROCKS,    [MV_RUMBLE] = B_ | FX_ROCKS, [MV_FLATTEN] = C_ | B_ | FX_STAR,
    [MV_MACE] = C_ | FX_ROCKS,       [MV_POLLEN] = FX_SPORE,         [MV_GLIMMER] = FX_STARS3,     [MV_RACK] = C_ | FX_LEAVES,
    [MV_BLOOM] = B_ | FX_LEAVES,     [MV_MANE] = C_ | B_ | FX_FLAME, [MV_WAVEWALL] = B_ | FX_WAVE,
};

uint8_t fx_for_type(uint8_t type) {
    return type < TY_COUNT ? TYPE_FX[type] : FX_STAR;
}

uint8_t fx_move_style(uint8_t move, uint8_t type) {
    if(move >= 1 && move < MV_COUNT) return move <= MV_WAVEWALL ? MV_STYLE[move] : 0;
    return (uint8_t)(fx_for_type(type) | (type < TY_FIRE ? MST_CONTACT : 0));
}


uint8_t fx_for_status(uint8_t kind) {
    switch(kind) {
    case SK_SLEEP: return FX_ZZZ;
    case SK_POISON: return FX_BUBBLES;
    case SK_BURN: return FX_FLAME;
    case SK_PARALYSIS: return FX_BOLT;
    case SK_CONFUSION: return FX_ORBIT;
    default: return FX_RINGS;
    }
}

uint16_t fx_duration(uint8_t kind) {
    return kind < FX_COUNT ? FX_MS[kind] : 0;
}

// ------------------------------------------------------------------------------------------------ the beam
static inline void px(LvlBuf lvl, int x, int y, uint8_t v) {
    lvl_putc(lvl, x, y, v);
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
static void heavy(LvlBuf lvl, int x0, int y0, int x1, int y1) { // black 3 x 3 with a white halo
    line_px(lvl, x0, y0, x1, y1, WHT, 3);
    line_px(lvl, x0, y0, x1, y1, BLK, 2);
}

// ------------------------------------------------------------------------------------------------ the player
#define FX_FILE "fx.bin"
#define FX_WIN 512 // (contract 67: was 1024, contract 65.1: 1536) bytes of frames held at once (an effect is 3 - 20 KB on the card; a read refills the window when t leaves it)
static inline unsigned le16(const uint8_t* p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}
#define KT(p, i) le16((p)->blk + 4 * (i))       // start (ms) of key i
#define KO(p, i) le16((p)->blk + 4 * (i) + 2)   // its frame's offset in the frame data
static unsigned key_end(const FxPlayer* p, unsigned i) {
    return i + 1 < p->nk ? KO(p, i + 1) : p->total;
}
// one past the last key whose frame ends within the window that starts at key i
static unsigned key_fit(const FxPlayer* p, unsigned i) {
    unsigned j = i;
    while(j < p->nk && key_end(p, j) - KO(p, i) <= p->win) j++;
    return j;
}

void fx_release(FxPlayer* p) {
    free(p->blk);
    memset(p, 0, sizeof(*p));
}

// Makes animation `idx` of the directory the loaded one: one read of its directory entry, one malloc (key table + window), one read of
// the key table and the first frames.  Nothing stays open.  False (and nothing drawn, the attempt is remembered) when the file is
// missing / does not match / the heap is short.
static bool fx_open(FxPlayer* p, unsigned idx) {
    if(p->id == idx + 1) return p->blk != NULL;
    uint8_t live = p->live;
    fx_release(p);
    p->live = live;
    p->id = (uint16_t)(idx + 1);
    uint8_t e[FXF_ENT];
    if(!read_at(FX_FILE, FXF_HDR + idx * FXF_ENT, e, sizeof(e))) return false;
    unsigned nk = e[8], total = le16(e + 4), maxf = le16(e + 6);
    uint32_t off = (uint32_t)le16(e) | ((uint32_t)le16(e + 2) << 16);
    if(!nk || nk > FXF_MAXKEYS || !maxf || maxf > total || off < FXF_HDR + FXF_ENT * FXF_NENT) return false; // not our table (or no animation)
    unsigned win = total < FX_WIN ? total : FX_WIN;
    if(win < maxf) win = maxf;
    unsigned need = nk * 4 + win;
    if(!heap_can_alloc(need)) return false;
    uint8_t* b = malloc(need);
    if(!b) return false;
    if(!read_at(FX_FILE, off, b, need)) {
        free(b);
        return false;
    }
    p->blk = b;
    p->nk = (uint8_t)nk;
    p->total = (uint16_t)total;
    p->win = (uint16_t)win;
    p->base = off + nk * 4;
    p->wf0 = 0;
    p->cur = 0;
    p->wf1 = (uint8_t)key_fit(p, 0);
    bool ok = KO(p, 0) == 0 && p->wf1 != 0 && KO(p, nk - 1) < total;
    for(unsigned i = 1; ok && i < nk; i++) ok = KO(p, i) > KO(p, i - 1) && KT(p, i) > KT(p, i - 1); // offsets and times strictly rise
    if(!ok) { // not a table of ours: never trust it with the window
        free(b);
        p->blk = NULL;
        return false;
    }
    return true;
}

// Frame data of key i, or NULL (the window is refilled from the card when it does not hold it).
static const uint8_t* fx_frame(FxPlayer* p, unsigned i, const uint8_t** end) {
    uint8_t* w = p->blk + 4u * p->nk;
    if(i < p->wf0 || i >= p->wf1) {
        unsigned j = key_fit(p, i);
        if(j == i || !read_at(FX_FILE, p->base + KO(p, i), w, key_end(p, j - 1) - KO(p, i))) {
            free(p->blk);
            p->blk = NULL;
            return NULL;
        }
        p->wf0 = (uint8_t)i;
        p->wf1 = (uint8_t)j;
    }
    unsigned o = KO(p, p->wf0);
    *end = w + key_end(p, i) - o;
    return w + KO(p, i) - o;
}

// Draws the pieces of a frame at origin (ox, oy).  A trail piece (tr 1..3) sits back along the flight (tdx, tdy) like the painter did.
static void draw_pieces(LvlBuf lvl, const uint8_t* f, const uint8_t* e, int ox, int oy, int tdx, int tdy) {
    unsigned np = (*f >> 1) & 7;
    for(f++; np && e - f >= 5; np--) {
        int x = ox + (int8_t)f[0], y = oy + (int8_t)f[1], w = f[2], h = f[3], tr = f[4];
        f += 5;
        if(tr) {
            x -= tdx * tr * 7 / 100;
            y -= tdy * tr * 7 / 100;
        }
        for(; h > 0; h--, y++)
            for(int cx = 0; cx < w;) {
                if(f >= e) return;
                uint8_t b = *f++;
                int n = (b & 63) + 1;
                if((b >> 6) && (unsigned)y < SCREEN_H) lvl_span(lvl, y, x + cx, x + cx + n, (b >> 6) == 1 ? WHT : BLK);
                cx += n;
            }
    }
}

// The frame of p shown at t ms, or NULL.
static const uint8_t* fx_at(FxPlayer* p, uint16_t t, const uint8_t** end) {
    unsigned i = p->cur;
    if(t < KT(p, i)) i = 0;
    while(i + 1u < p->nk && KT(p, i + 1) <= t) i++;
    p->cur = (uint8_t)i;
    return fx_frame(p, i, end);
}

void fx_frame_end(FxPlayer* p) {
    if(!p->live) fx_release(p);
    p->live = 0;
}

void fx_draw(FxPlayer* p, LvlBuf lvl, uint8_t kind, uint16_t t, int x0, int y0, int x1, int y1, int dir, int big) {
    uint16_t dur = fx_duration(kind);
    if(!dur || t >= dur) return;
    p->live = 1;
    if(!fx_open(p, fxt_index(kind, (unsigned)big, dir, fxt_class(x1 - x0, y1 - y0)))) return;
    const uint8_t *e, *f = fx_at(p, t, &e);
    if(!f) return;
    if(*f & 1) // the bolt's flash: the target box inverted
        for(int y = y0 < 0 ? 0 : y0; y < y1 && y < SCREEN_H; y++)
            for(int x = x0 < 0 ? 0 : x0; x < x1 && x < SCREEN_W; x++) lvl_put(lvl, x, y, (uint8_t)(3 - lvl_get(lvl, x, y)));
    draw_pieces(lvl, f, e, (x0 + x1) / 2, y1, 0, 0);
}

// A projectile: a ball (or needle / rock / thread) with a short trail, from (ax, ay) to (bx, by) in `ms`; the electric one is a crackling beam.
void fx_proj(FxPlayer* p, LvlBuf lvl, uint8_t kind, uint16_t t, uint16_t ms, int ax, int ay, int bx, int by) {
    if(t >= ms) return;
    float pr = t / (float)ms;
    int hx = ax + (int)((bx - ax) * pr), hy = ay + (int)((by - ay) * pr) - arc_(t, ms);
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
    p->live = 1;
    if(!fx_open(p, fxt_pindex(kind, bx > ax))) return;
    const uint8_t *e, *f = fx_at(p, t, &e);
    if(f) draw_pieces(lvl, f, e, hx, hy, bx - ax, by - ay);
}

// ------------------------------------------------------------------------------------------------ the capture crystal
static const uint8_t CRY_HW[CRYSTAL_H] = {0, 1, 2, 3, 3, 3, 3, 3, 2, 1, 0}; // half widths (the lab crystal's profile)

void fx_crystal(LvlBuf lvl, int cx, int by, int tilt, int dim) {
    for(int r = -1; r <= CRYSTAL_H; r++) { // rows -1 and CRYSTAL_H are the outline's tip and base
        bool cap = r < 0 || r >= CRYSTAL_H;
        int hw = CRY_HW[r < 0 ? 0 : (r >= CRYSTAL_H ? CRYSTAL_H - 1 : r)];
        int y = by - (CRYSTAL_H - 1 - r);
        int sx = cx + tilt * (CRYSTAL_H - 1 - r) / (CRYSTAL_H - 1);
        for(int x = -hw - 1; x <= hw + 1; x++) {
            int ax = x < 0 ? -x : x;
            if(cap && ax > hw) continue;
            uint8_t v = (cap || ax > hw) ? 3 : (x < 0 ? 0 : (x == 0 ? 1 : 2));
            if(dim && v < 3) v = (uint8_t)(v < 2 ? v + 1 : 2);
            lvl_putc(lvl, sx + x, y, v);
        }
    }
}
