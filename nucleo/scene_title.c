// TITLE scene "Elementis Zeta" (contract section 21): the pre-rendered logo, the spinning Cristal and "ZETA" (files/title.bin,
// tools/titlec.py), the parade of Elementis (mon.bin medium front pictures, mirrored to face the hero) and the hero, plus a
// blinking "Pulsa OK". With a valid save, OK opens a second screen: the Continuar / Nueva partida list.
// Heap while the art is on: the logo + ZETA + ONE crystal frame (one block, ~1.4 KB, the frame is re-read when it changes) and one
// picture of the parade; the title.bin file is never kept open. The art stays loaded under the list (Back returns without a reload).
#include "scene.h"
#include "world.h"
#include "render.h"
#include "font.h"
#include "frame.h"
#include "assets.h"
#include "text.h"
#include "save.h"
#include "monpic.h"
#include "ui_host.h"
#include "sound.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLINK_MS 500
#define TICK_MS 60 // one animation tick
#define TITLE_FILE "title.bin"
#define TT_NCR 12 // crystal frames: 60 degrees of spin, a seamless loop
#define SLIDE 6 // parade: ticks to slide in / out, ticks held
#define HOLD 40
#define PER (2 * SLIDE + HOLD)

static const uint8_t PARADE[] = {SP_PYREWOLF, SP_TIDALGARD, SP_GROVESTAG, SP_VOLTGECKO};
#define N_PARADE ((int)sizeof(PARADE))
static const uint16_t IDS[3] = {TXT_GL_TITLE_CONTINUE, TXT_GL_TITLE_NEW, TXT_GL_TITLE_PRESS_OK};
static const int8_t BOB[10] = {0, 0, -1, -1, -1, 0, 0, 1, 1, 1}; // the crystal floats: one step per 4 ticks
static const uint8_t SPK[6] = {1, 2, 3, 3, 2, 1};                  // twinkle sizes, the first 6 ticks of every 48

// title.bin's header (tools/titlec.py, contract 21): the file is the heap block itself, [header][logo][ZETA] read whole, then the
// crystal frame k read into the slot at `coff`. A record is u8 w, u8 h, 2 bpp pixels, 1 bpp mask.
typedef struct {
    char magic[4];
    uint8_t ver, n;
    uint16_t zoff, coff, clen;
    uint8_t hx, hy; // the hero's top-left (his feet on the last row, facing the parade)
    uint16_t pad;
} THdr;

typedef struct {
    tchar str[3][16]; // "Continuar", "Nueva partida" (only with a valid save.bin), "Pulsa OK" (active language, loaded on enter)
    MonPic pic;       // the parade picture on screen
    uint8_t* blk;     // title.bin's block (THdr, logo, ZETA, current crystal frame); NULL = no art
    uint32_t ms, tick;
    int8_t cur_cr, cur_pic;
    uint8_t cur;      // list cursor: 0 = Continuar, 1 = Nueva partida
    bool has_save, menu;
    bool aff_ask, aff_wait; // contract 66: the save being continued has no Solar / Lunar affinity yet (an older save): Continuar asks for it first; the picker is on
#ifdef NUCLEO_DEBUG
    char heap[32];
#endif
} Title;

static Title g_title;

// View of the record at p.
static void rec(Blit* b, const uint8_t* p) {
    b->flip = 0;
    b->w = p[0];
    b->h = p[1];
    b->bpr = (uint8_t)((b->w + 3) >> 2);
    b->mbpr = (uint8_t)((b->w + 7) >> 3);
    b->pix = p + 2;
    b->mask = b->pix + b->bpr * b->h;
}

static void art_free(Title* t) {
    free(t->blk);
    t->blk = NULL;
    monpic_free(&t->pic);
}

// Brings the crystal frame and the parade picture up to date with the tick (file reads happen here, not while drawing).
static void sync(Title* t) {
    if(!t->blk) return;
    const THdr* h = (const THdr*)t->blk;
    int cr = (int)((t->tick >> 1) % TT_NCR);
    if(cr != t->cur_cr) {
        t->cur_cr = (int8_t)cr;
        if(!read_at(TITLE_FILE, h->coff + cr * h->clen, t->blk + h->coff, h->clen)) memset(t->blk + h->coff, 0, h->clen); // w = h = 0: nothing drawn
    }
    int k = (int)((t->tick % (PER * N_PARADE)) / PER);
    if(k != t->cur_pic) {
        t->cur_pic = (int8_t)k;
        monpic_free(&t->pic);
        monpic_load_kind(&t->pic, PARADE[k], BP_FRONT_MED);
        t->pic.b.flip = 1; // faces the hero
    }
}

static void art_load(Title* t) {
    THdr h;
    if(!read_at(TITLE_FILE, 0, &h, sizeof(h)) || memcmp(h.magic, "NTTL", 4) || h.ver != 1 || h.n != TT_NCR) return;
    if(!heap_can_alloc(h.coff + h.clen)) return;
    uint8_t* p = malloc(h.coff + h.clen);
    if(!p) return;
    if(!read_at(TITLE_FILE, 0, p, h.coff)) {
        free(p);
        return;
    }
    t->blk = p;
    t->cur_cr = t->cur_pic = -1;
    sync(t);
}

static void t_enter(SceneCtx* c, void* d) {
    (void)d;
    ui_live_enter();
    sound_play(SND_TITLE); // the title music (contract 28)
    Title* t = &g_title;
    memset(t, 0, sizeof(*t));
    t->has_save = save_peek(NULL);
    t->aff_ask = t->has_save && g_peek_aff == 0;
    g_aff_choice = 0;
    const tchar* all[3] = {t->str[0], t->str[1], t->str[2]};
    for(int k = t->has_save ? 0 : 2; k < 3; k++) // the list only with a save; "Pulsa OK" always
        if(text_load(IDS[k], t->str[k], 16) < 0) t->str[k][0] = 0;
    text_prefetch_many(all + (t->has_save ? 0 : 2), t->has_save ? 3 : 1);
    art_load(t);
#ifdef NUCLEO_DEBUG
    HeapStats hs;
    heap_stats_now(&hs);
    heap_format(&hs, t->heap, sizeof(t->heap));
#endif
    scene_dirty(c, D_WORLD);
    scene_wake(c, TICK_MS);
}

static void t_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    art_free(&g_title);
    ui_live_leave();
}

static void t_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    Title* t = &g_title;
    if(ev->key == KEY_BACK && (ev->type == EV_SHORT || ev->type == EV_LONG)) {
        if(t->menu && ev->type == EV_SHORT) { // back to the animated title
            t->menu = false;
            scene_dirty(c, D_WORLD);
            scene_wake(c, TICK_MS);
        } else {
            scene_clear(c);
        }
    } else if(t->menu && (ev->type == EV_PRESS || ev->type == EV_REPEAT) && (ev->key == KEY_UP || ev->key == KEY_DOWN)) {
        t->cur = ev->key == KEY_UP ? 0 : 1;
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        sound_play(SND_FX_CONFIRM); // (contract 28)
        if(t->has_save && !t->menu) { // the Continuar / Nueva partida screen: a plain white page
            t->menu = true;
            t->cur = 0;
            scene_dirty(c, D_WORLD);
        } else if(t->menu && t->cur == 0) {
            if(t->aff_ask && !g_aff_choice) t->aff_wait = scene_name_open(c, 2); // contract 66: the one-time choice of an older save; t_update goes on when the picker has closed
            else scene_fade_continue(c); // fade out, load the saved map, swap in the world, fade in
        } else {
            scene_fade_new_game(c); // same with the start map
        }
    }
}

static void t_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)d;
    Title* t = &g_title;
    if(t->aff_wait && g_aff_choice) { // the picker closed with a choice: now continue
        t->aff_wait = false;
        scene_fade_continue(c);
        return;
    }
    if(t->menu) return;
    t->ms += dt;
    uint32_t k = t->ms / TICK_MS;
    if(k != t->tick) {
        t->tick = k;
        sync(t);
        scene_dirty(c, D_WORLD); // the picture, the Cristal, the blink: one redraw per tick
    }
    scene_wake(c, TICK_MS - t->ms % TICK_MS);
}

// 4-point twinkle in black, white centre from size 2 (the mockup's spark()).
static void spark(LvlBuf lvl, int cx, int cy, int size) {
    for(int d = -size; d <= size; d++) {
        uint8_t v = (d > -size && d < size) ? 3 : 2;
        lvl_putc(lvl, cx + d, cy, v);
        lvl_putc(lvl, cx, cy + d, v);
    }
    if(size >= 2) lvl_putc(lvl, cx, cy, 0);
}

static void t_draw_world(SceneCtx* c, void* d) {
    (void)d;
    Title* t = &g_title;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, 0);
    if(t->menu || !t->blk) return;
    const THdr* h = (const THdr*)t->blk;
    Blit b;
    rec(&b, t->blk + h->coff); // the Cristal, centred at (64, 42), floating
    int cw = b.w, ch = b.h, dy = BOB[(t->tick >> 2) % 10];
    blit_masked(lvl, &b, 64 - cw / 2, 42 - ch / 2 + dy);
    if(t->pic.blk) { // parade: slides in from the left edge, holds, slides out; standing on the bottom row
        int f = (int)(t->tick % PER), w = t->pic.b.w; // (PER divides the whole parade)
        int x = f < SLIDE ? -w + w * (f + 1) / SLIDE : f < SLIDE + HOLD ? 0 : -(w * (f - SLIDE - HOLD + 1) / SLIDE);
        blit_masked(lvl, &t->pic.b, x, SCREEN_H - t->pic.b.h);
    }
    hero_view(7, 0, &b); // south-west: the south-east facing mirrored
    blit_masked(lvl, &b, h->hx, h->hy);
    rec(&b, t->blk + sizeof(THdr));
    blit_masked(lvl, &b, (SCREEN_W - b.w) / 2, 1); // ELEMENTIS
    rec(&b, t->blk + h->zoff);
    blit_masked(lvl, &b, (SCREEN_W - b.w) / 2 + 1, 38); // ZETA, over the Cristal
    unsigned s = (unsigned)(t->tick % 48);
    if(s < 6) spark(lvl, 64 - cw / 2 + 5, 42 - ch / 2 + 12 + dy, SPK[s]);
}

static void t_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    Title* t = &g_title;
    // one plate for both screens: the list (2 rows, cursor column, top-left corner) or "Pulsa OK" (1 row, blinking, bottom centre)
    int n = t->menu ? 2 : 1, base = t->menu ? 0 : 2, tw = 0;
    if(!t->menu && ((t->ms / BLINK_MS) & 1 || !t->str[2][0])) n = 0;
    for(int k = 0; k < n; k++) {
        int w = font_text_width(t->str[base + k]);
        if(w > tw) tw = w;
    }
    if(n) {
        int cw = t->menu ? font_cell_w() + 2 : 0, pw = tw + cw + 6, ph = n * (font_pitch() + 2);
        int px = t->menu ? 0 : (SCREEN_W - pw) / 2, py = t->menu ? 0 : SCREEN_H - ph;
        ov_fill(px, py, pw, ph, 0);
        ov_frame(px, py, pw, ph, 1);
        for(int k = 0; k < n; k++) {
            int y = py + n + k * font_pitch();
            if(t->menu && t->cur == k) font_draw_glyph(GLYPH_CURSOR, px + 3, y, 1);
            font_draw_text(t->str[base + k], px + 3 + cw, y);
        }
    }
#ifdef NUCLEO_DEBUG
    if(t->menu) { // on the list's white page, bottom-left (on the title it would cover the logo)
        // glyph cell is 10 tall; the cap-height part sits on the last 7 rows (the system font's codes are ASCII: contract 70 dropped the copy through sysfont_from_ascii)
        sysfont_draw_str((const uint8_t*)t->heap, 1, SCREEN_H - 9);
    }
#endif
}

const SceneOps scene_title_ops = {
    .enter = t_enter,
    .leave = t_leave,
    .input = t_input,
    .update = t_update,
    .draw_world = t_draw_world,
    .draw_overlay = t_draw_overlay,
    .pauses_below = false,
};

bool scene_title_open(SceneCtx* c) {
    return scene_push(c, &scene_title_ops, NULL);
}
