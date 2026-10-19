// Trainer card (contract section 25.4), a screen of the PC plugin again since contract 76 (the casino plugin from 74 to 76, while the PC plugin was the tight one; PC_OPEN_CARD,
// menu_open_bond(MB_CARD), scene_box.c scene_pc_open): pause menu -> <NAME> (START -> <NAME>). One framed white screen: the player's
// name, money, play time and the eight badge slots in two rows of four. The badges are CODE-DRAWN 10 x 10 icons (no art files): a slot is filled (black
// outline, white ring, black core, like a medal) when flag FLAG_BADGE_FIRST + k is set, otherwise it is the same shape as a dotted outline. Badge 1
// (Medalla Cantera) is a stone hexagon. Any of OK / Back closes it; Back long exits the app.
#include <stdlib.h>
#include <string.h>
#include "scene_card.h"
#include "party.h"
#include "bag.h"
#include "save.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "sound.h"
#include "world.h"
#include "gen/maps_gen.h"

#define CAP      16
#define BADGES   8
#define BADGE_PX 10
#define CUR_SYM  '$' // the currency sign (the shop's: scene_shop.c)

typedef struct {
    tchar who[8]; // the "{player}" token string
    tchar l_money[CAP], l_time[CAP];
    tchar money[12], time[10];
    tchar aff[CAP]; // contract 66: "Solar" / "Lunar" (empty = not chosen), at the right end of the name row
    uint8_t have; // bit k: badge k earned
} CS;

static CS* g_cd;

bool scene_card_active(void) {
    return g_cd != NULL;
}

// 10 x 10 silhouettes, one u16 per row, the 10 low bits, bit 9 = the leftmost pixel.
#define TCR(a, b, c, d, e, f, g_, h, i, j)                                                       \
    (uint16_t)(                                                                                  \
        (a << 9) | (b << 8) | (c << 7) | (d << 6) | (e << 5) | (f << 4) | (g_ << 3) | (h << 2) | \
        (i << 1) | (j))
static const uint16_t SHAPE[BADGES][BADGE_PX] = {
    {// 1 Cantera: a stone hexagon (pointed top and bottom, upright sides)
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0)},
    {// 2 Oleaje (C2): a breaking wave, a crest curling over to the right on a rounded body
     TCR(0, 0, 0, 0, 0, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 0, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 0, 0, 1, 1),
     TCR(0, 1, 1, 1, 1, 0, 0, 0, 0, 1),
     TCR(1, 1, 1, 1, 1, 1, 0, 0, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0)},
    {// 3 Voltio (C3): a lightning bolt, the top bar leaning down-left, a step, then the point
     TCR(0, 0, 0, 0, 1, 1, 1, 1, 1, 1),
     TCR(0, 0, 0, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 0, 0, 0),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 0, 0, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 1, 1, 1, 0, 0, 0, 0, 0),
     TCR(0, 0, 1, 1, 0, 0, 0, 0, 0, 0)},
    {// 4 Raiz (C4a): a leaf on its stem, the tip up-right (the plant gym's root badge)
     TCR(0, 0, 0, 0, 0, 0, 1, 1, 1, 1),
     TCR(0, 0, 0, 0, 0, 1, 1, 1, 1, 1),
     TCR(0, 0, 0, 0, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 0, 0, 0, 0),
     TCR(0, 1, 1, 1, 0, 0, 0, 0, 0, 0),
     TCR(1, 1, 0, 0, 0, 0, 0, 0, 0, 0),
     TCR(1, 0, 0, 0, 0, 0, 0, 0, 0, 0)},
    {// 5 Toxina (C5a): a poison drop, pointed at the top and round at the bottom (the poison gym's badge)
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0)},
    {// 6 Mente (C6): an eye, a round almond with a hollow pupil (the psychic gym's badge)
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(1, 1, 1, 1, 0, 0, 1, 1, 1, 1),
     TCR(1, 1, 1, 0, 0, 0, 0, 1, 1, 1),
     TCR(1, 1, 1, 0, 0, 0, 0, 1, 1, 1),
     TCR(1, 1, 1, 1, 0, 0, 1, 1, 1, 1),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0)},
    {// 7 Brasa (C7, contract 53): a flame, a tongue up-right of the main point (the fire gym's badge)
     TCR(0, 0, 0, 0, 1, 0, 0, 0, 0, 0),
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 0, 1, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 1, 0, 0)},
    {// 8 Nucleo (C8, contract 51): a cut diamond, the core of the League's last badge (the ground gym's)
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
     TCR(0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
     TCR(0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
     TCR(0, 0, 0, 1, 1, 1, 1, 0, 0, 0),
     TCR(0, 0, 0, 0, 1, 1, 0, 0, 0, 0)},
};

static bool tc_px(const uint16_t* r, int x, int y) {
    return x >= 0 && x < BADGE_PX && y >= 0 && y < BADGE_PX && ((r[y] >> (BADGE_PX - 1 - x)) & 1);
}

static bool tc_edge(
    const uint16_t* r,
    int x,
    int y) { // a pixel of the shape with a blank 4-neighbour: the outline
    return tc_px(r, x, y) && (!tc_px(r, x - 1, y) || !tc_px(r, x + 1, y) || !tc_px(r, x, y - 1) ||
                              !tc_px(r, x, y + 1));
}

// One badge at (x0, y0): filled = black outline, white ring one pixel inside it, black core; empty = the outline as dots.
static void tc_badge(int x0, int y0, int k, bool have) {
    const uint16_t* r = SHAPE[k];
    for(int y = 0; y < BADGE_PX; y++)
        for(int x = 0; x < BADGE_PX; x++) {
            if(!tc_px(r, x, y)) continue;
            bool on;
            if(tc_edge(r, x, y))
                on = have || ((x + (y >> 1)) & 1) == 0; // (x + y would drop whole diagonal edges)
            else
                on = have && !(tc_edge(r, x - 1, y) || tc_edge(r, x + 1, y) ||
                               tc_edge(r, x, y - 1) || tc_edge(r, x, y + 1));
            if(on) ov_fill(x0 + x, y0 + y, 1, 1, 1);
        }
}

static int tc_num(tchar* o, uint32_t v, int min_digits) {
    char t[11];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while(v);
    int len = 0;
    for(int i = n; i < min_digits; i++)
        o[len++] = '0';
    while(n)
        o[len++] = (tchar)t[--n];
    o[len] = 0;
    return len;
}

static void ld_cd(tchar* d, uint16_t id) {
    if(text_load(id, d, CAP) < 0) d[0] = 0;
}

static void c_enter(SceneCtx* c, void* d) {
    CS* s = d;
    if(text_load(TXT_GL_MENU_CARD, s->who, 8) < 0) s->who[0] = 0;
    ld_cd(s->l_money, TXT_GL_CARD_MONEY);
    ld_cd(s->l_time, TXT_GL_CARD_TIME);
    s->money[0] = CUR_SYM;
    tc_num(s->money + 1, money_get(), 1);
    const uint32_t secs = save_time_secs();
    int n = tc_num(s->time, secs / 3600, 2);
    s->time[n++] = ':';
    tc_num(s->time + n, (secs / 60) % 60, 2);
    s->aff[0] = 0;
    if(party_state()->aff)
        ld_cd(s->aff, party_state()->aff == AFF_LUNAR ? TXT_GL_AFF_LUNAR : TXT_GL_AFF_SOLAR);
    s->have = 0;
    for(int k = 0; k < BADGES; k++) {
        const uint8_t id = (uint8_t)(FLAG_BADGE_FIRST + k);
        if((c->world->flags[id >> 3] >> (id & 7)) & 1) s->have |= (uint8_t)(1u << k);
    }
    const tchar* all[5] = {s->who, s->l_money, s->l_time, s->money, s->aff};
    text_prefetch_many(all, 5);
    text_prefetch(s->time);
    scene_dirty(c, D_WORLD);
}

static void c_leave(SceneCtx* c, void* d) {
    (void)c;
    if(g_cd == d) g_cd = NULL;
    free(d);
}

static void c_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) {
        scene_clear(c);
    } else if(
        (ev->key == KEY_BACK && ev->type == EV_SHORT) ||
        (ev->key == KEY_OK && ev->type == EV_PRESS)) {
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static void c_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static void c_draw(SceneCtx* c, void* d) {
    (void)c;
    const CS* s = d;
    const int p = font_pitch(), r = SCREEN_W - 5;
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    text_draw_line(s->who, 0, 5, 2, 99);
    font_draw_text(s->aff, r - font_text_width(s->aff), 2);
    font_draw_text(s->l_money, 5, 2 + p);
    font_draw_text(s->money, r - font_text_width(s->money), 2 + p);
    font_draw_text(s->l_time, 5, 2 + 2 * p);
    font_draw_text(s->time, r - font_text_width(s->time), 2 + 2 * p);
    const int y0 = 2 + 3 * p + 2, gap = 2, pitch = 28,
              x0 = (SCREEN_W - (3 * pitch + BADGE_PX)) / 2;
    for(int k = 0; k < BADGES; k++)
        tc_badge(x0 + (k & 3) * pitch, y0 + (k >> 2) * (BADGE_PX + gap), k, (s->have >> k) & 1);
}

const SceneOps scene_card_ops = {
    .enter = c_enter,
    .leave = c_leave,
    .input = c_input,
    .draw_world = c_draw_world,
    .draw_overlay = c_draw,
    .pauses_below = true,
};

bool scene_card_open(SceneCtx* c) {
    if(g_cd || !c->world) return false;
    CS* s = calloc(1, sizeof(CS));
    if(!s) return false;
    g_cd = s;
    if(!scene_push(c, &scene_card_ops, s)) {
        g_cd = NULL;
        free(s);
        return false;
    }
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_card_badges(void) {
    return g_cd ? g_cd->have : -1;
}
#endif
