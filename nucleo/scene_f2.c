// F2 (contract 74, casino plugin): the screens and checks of the batch that are not worth resident code.
//   * the Buscatesoros: WACT_DETECT, set by the bag's Usar; one line of text by the nearest HIDDEN item of the map (mapc legend `hidden=<item>`: a face-mode script interaction whose
//     `flag` is its pick-up flag, still clear). It says nothing about where: "yes, there is one near" / "no, nothing".
//   * the Atlas: WACT_REGMAP, set by the bag's Usar; a pre-rendered 128 x 64 region map read straight into the level buffer from files/regmap.bin (tools/regmapc.py) with a blinking mark on the
//     hero's place and the place's name in the bar at the bottom. The file also maps every map id to the area it belongs to (an interior = the town around it).
//   * the diploma (the original studio diploma role): `bond diploma`, a code-drawn frame with the player's name, shown by the lab assistant once the Registro is complete.
//   * `bond ot`: whether a party creature is the player's own (the name changer refuses the IR-traded ones).
// Nothing is saved, nothing is resident; the texts are gl.f2.*.
#include <stddef.h>
#include <string.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "party.h"
#include "assets.h"
#include "hud_gfx.h"
#include "casino_host.h"
#include "gen/maps_gen.h"

// ---------------------------------------------------------------------------------------------------- the Buscatesoros
#define DETECT_NEAR 5 // tiles (the larger of |di|, |dj|) within which a hidden item makes it respond
#define DETECT_HERE 1 // ... and within which it says the item is right here

#ifndef NUCLEO_DEVICE
uint16_t g_f2_last_msg; // host tests: the text id of the last dialog the Buscatesoros opened
#endif
static void detect(SceneCtx* c) {
    const World* w = c->world;
    int best = 99;
    for(uint8_t k = 0; k < w->map.n_interacts; k++) {
        const InteractDef* d = &w->map.interacts[k];
        const uint16_t f = interact_flag(d);
        if(interact_kind(d) != INT_SCRIPT || d->mode != INT_MODE_FACE || !f || ((w->flags[f >> 3] >> (f & 7)) & 1)) continue;
        int di = d->i - w->hero.ti, dj = d->j - w->hero.tj;
        if(di < 0) di = -di;
        if(dj < 0) dj = -dj;
        if(dj > di) di = dj;
        if(di < best) best = di;
    }
    const uint16_t id = best <= DETECT_HERE ? TXT_GL_F2_DET_HERE : best <= DETECT_NEAR ? TXT_GL_F2_DET_NEAR : TXT_GL_F2_DET_NONE;
#ifndef NUCLEO_DEVICE
    g_f2_last_msg = id;
#endif
    scene_dialog_open(c, id, -1);
}

// ---------------------------------------------------------------------------------------------------- the Atlas
#define RM_FILE "regmap.bin"
#define RM_IMG 16u    // file offset of the 128 x 64 picture (2,048 B, the level buffer's own packing)
#define RM_TAB 2064u  // area table: n_area x (x, y) of the place's mark; then one byte per map id: the area it belongs to (0 = none)
#define RM_BLINK_MS 330

typedef struct {
    uint8_t x, y;     // the mark's centre (255 = unknown place)
    uint8_t on;
    uint16_t t;       // ms since the last blink toggle
    tchar lab[20];    // the place's name
} RegMap;
static RegMap g_rm;
static uint8_t g_f2_live;

bool scene_f2_active(void) {
    return g_f2_live != 0;
}

static void rm_enter(SceneCtx* c, void* d) {
    RegMap* r = d;
    uint8_t h[8], a = 0, xy[2] = {255, 255};
    g_f2_live++;
    r->lab[0] = '?';
    r->lab[1] = '?';
    r->lab[2] = '?';
    r->lab[3] = 0;
    if(read_at(RM_FILE, 0, h, sizeof(h)) && !memcmp(h, "NRMP", 4) && c->world->map.id < (uint16_t)(h[6] | (h[7] << 8)) &&
       read_at(RM_FILE, RM_TAB + 2u * h[5] + c->world->map.id, &a, 1) && a && a <= h[5]) {
        read_at(RM_FILE, RM_TAB + 2u * (a - 1u), xy, 2);
        if(text_load((uint16_t)(TXT_GL_AREA_VILLA_ALISO + a - 1), r->lab, sizeof(r->lab) / sizeof(r->lab[0])) < 0) r->lab[0] = 0;
    }
    r->x = xy[0];
    r->y = xy[1];
    r->on = 1;
    text_prefetch(r->lab);
    scene_dirty(c, D_WORLD | D_COMPOSE);
    scene_wake(c, RM_BLINK_MS);
}

static void rm_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    g_f2_live--;
}

static void rm_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
    else if((ev->key == KEY_OK || ev->key == KEY_BACK) && ev->type == EV_SHORT) {
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static void rm_update(SceneCtx* c, void* d, uint32_t dt) {
    RegMap* r = d;
    r->t = (uint16_t)(r->t + (dt > RM_BLINK_MS ? RM_BLINK_MS : dt));
    if(r->t >= RM_BLINK_MS) {
        r->t = 0;
        r->on ^= 1;
        scene_dirty(c, D_COMPOSE);
    }
    scene_wake(c, (uint32_t)(RM_BLINK_MS - r->t));
}

static void rm_draw_world(SceneCtx* c, void* d) {
    (void)d;
    if(!read_at(RM_FILE, RM_IMG, c->frame->lvl, (uint32_t)SCREEN_H * LVL_STRIDE)) lvl_fill(c->frame->lvl, 0);
}

static void rm_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const RegMap* r = d;
    if(r->x != 255 && r->on) { // the mark: a white 9 x 9 pad with a black outline and a black centre, over whatever the picture has there
        ov_fill(r->x - 4, r->y - 4, 9, 9, 0);
        ov_frame(r->x - 4, r->y - 4, 9, 9, 1);
        ov_fill(r->x - 1, r->y - 1, 3, 3, 1);
    }
    ov_fill(0, SCREEN_H - 12, SCREEN_W, 12, 0); // the name bar (the picture reserves the same rows)
    ov_fill(0, SCREEN_H - 13, SCREEN_W, 1, 1);
    text_draw_line(r->lab, 0, 4, SCREEN_H - font_pitch() - (font_pitch() < 12), 99);
}

static const SceneOps rm_ops = {
    .enter = rm_enter,
    .leave = rm_leave,
    .input = rm_input,
    .update = rm_update,
    .draw_world = rm_draw_world,
    .draw_overlay = rm_draw_overlay,
    .pauses_below = true,
};

// ---------------------------------------------------------------------------------------------------- the diploma
enum { DP_TITLE, DP_WHO, DP_L1, DP_L2, DP_N };
typedef struct {
    tchar s[DP_N][24];
} Diploma;
static Diploma g_dp;

static void dp_enter(SceneCtx* c, void* d) {
    Diploma* p = d;
    static const uint16_t ID[DP_N] = {TXT_GL_F2_DIP_T, TXT_GL_F2_DIP_WHO, TXT_GL_F2_DIP_L1, TXT_GL_F2_DIP_L2};
    const tchar* all[DP_N];
    g_f2_live++;
    for(int k = 0; k < DP_N; k++) {
        if(text_load(ID[k], p->s[k], 24) < 0) p->s[k][0] = 0;
        all[k] = p->s[k];
    }
    text_prefetch_many(all, DP_N);
    scene_dirty(c, D_WORLD | D_COMPOSE);
}

static void dp_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static void centred(const tchar* s, int y) {
    text_draw_line(s, 0, (SCREEN_W - text_line_px(s, 0)) / 2, y, 99);
}

static void dp_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Diploma* p = d;
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1); // the double border, with a notch of 3 px at every corner
    ov_frame(3, 3, SCREEN_W - 6, SCREEN_H - 6, 1);
    static const int8_t CX[4] = {1, SCREEN_W - 3, 1, SCREEN_W - 3}, CY[4] = {1, 1, SCREEN_H - 3, SCREEN_H - 3};
    for(int k = 0; k < 4; k++) ov_fill(CX[k], CY[k], 2, 2, 1);
    const int pitch = font_pitch(), step = pitch + 1, y0 = (SCREEN_H - (DP_N * step - 1)) / 2;
    const int tw = text_line_px(p->s[DP_TITLE], 0), ty = y0;
    centred(p->s[DP_TITLE], ty);
    star_draw((SCREEN_W - tw) / 2 - 10, ty + pitch / 2 - 2); // a star on each side of the title
    star_draw((SCREEN_W + tw) / 2 + 6, ty + pitch / 2 - 2);
    ov_fill(12, ty + pitch + 1, SCREEN_W - 24, 1, 1);
    centred(p->s[DP_WHO], y0 + step + 2);
    centred(p->s[DP_L1], y0 + 2 * step + 2);
    centred(p->s[DP_L2], y0 + 3 * step + 2);
}

static void dp_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
    else if((ev->key == KEY_OK || ev->key == KEY_BACK) && ev->type == EV_SHORT) {
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static const SceneOps dp_ops = {
    .enter = dp_enter,
    .leave = rm_leave, // (the same live counter)
    .input = dp_input,
    .update = NULL,
    .draw_world = dp_draw_world,
    .draw_overlay = dp_draw_overlay,
    .pauses_below = true,
};

// ---------------------------------------------------------------------------------------------------- entry points (casino.c)
// The world acts WACT_DETECT / WACT_REGMAP (menu_open_daycare -> CAS_BOND, b >= BOND_ACT). True = a scene of the plugin was pushed (it keeps the plugin loaded).
bool f2_feat(SceneCtx* c, uint8_t act) {
    if(act == WACT_DETECT) {
        detect(c);
        return false;
    }
    return c->n + 1 <= MAX_SCENES && scene_push(c, &rm_ops, &g_rm);
}

// `bond ot SLOT` (the result goes to SLOT + 1, so `out[-1]` is the slot): 1 = the creature is the player's own, 0 = it arrived over the IR link, 255 = no such creature.
uint8_t f2_ot(const uint8_t* out) {
    const Party* p = party_player();
    return out[-1] < p->count ? bc_mon_is_ot(&p->mon[out[-1]]) : 255;
}

bool f2_diploma(SceneCtx* c) {
    return c->n + 1 <= MAX_SCENES && scene_push(c, &dp_ops, &g_dp);
}

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only): 0 none, 1 the region map, 2 the diploma is the top scene; the region map's mark (255 = unknown place) and phase, and the place's name as codes.
int scene_f2_kind(SceneCtx* c) {
    const SceneOps* o = scene_top_ops(c);
    return o == &rm_ops ? 1 : o == &dp_ops ? 2 : 0;
}
void scene_f2_mark(int* x, int* y, int* on, const tchar** label) {
    *x = g_rm.x;
    *y = g_rm.y;
    *on = g_rm.on;
    *label = g_rm.lab;
}
#endif
