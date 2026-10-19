// FOLLOW scene (contract 69, casino plugin): OK pressed while facing the follower (world.c talk_npc -> nat_pc `bond follow`, casino.c). A small code-drawn emote bubble (a 15 x 15 balloon with a tail and a 9 x 9 sign) pops up above the
// follower and, after a moment, ONE line of text: by the state of the creature that follows (asleep, frozen, paralysed, burned, poisoned, tired = HP under a third) and, when it is well, by its friendship tier (bc_friend_tier).
// Like scene_mon.c it sits BELOW the dialog it opens and closes itself the first time it is on top again. Nothing is saved, nothing is resident: the bubble, the states and the texts (gl.fol.*) live in the plugin.
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "party.h"
#include "frame.h"
#include "assets.h"
#include "casino_host.h"
#include "gen/maps_gen.h"

#define FOLLOW_BUBBLE_MS 650 // the bubble alone, before the text (OK skips the wait)
#define BUB_W            15
#define BUB_H            15 // the balloon with its tail
// the sign / line index: the order of the texts gl.fol.t0 .. t4 (friendship tiers 0 .. 4), tired, psn, slp, brn, frz, par
enum {
    F_T0 = 0,
    F_TIRED = 5,
    F_PSN,
    F_SLP,
    F_BRN,
    F_FRZ,
    F_PAR,
    F_COUNT
};
_Static_assert(
    TXT_GL_FOL_PAR - TXT_GL_FOL_T0 == F_COUNT - 1,
    "gl.fol.* texts must be consecutive (assets/text, block C1 follower)");

// The balloon's outline, 15 x 15 (two bytes a row, the leftmost pixel is the top bit): cut corners, a flat bottom, a three-pixel tail. Drawn over a white 13 x 11 fill.
#define B2(v) (uint8_t)((v) >> 8), (uint8_t)(v)
static const uint8_t BUBBLE[BUB_H * 2] = {
    B2(0b0011111111111000),
    B2(0b0100000000000100),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b1000000000000010),
    B2(0b0100000000000100),
    B2(0b0011111111111000),
    B2(0b0000001110000000),
    B2(0b0000000100000000)};
// The 9 x 9 signs, one 9-bit row each (the leftmost pixel is bit 8), black on the white balloon
#define R(a) (uint8_t)((a) >> 1), (uint8_t)(((a) & 1) << 7)
static const uint8_t SIGN[F_COUNT][18] = {
    {R(0b000000000),
     R(0b000000000),
     R(0b000000000),
     R(0b000000000),
     R(0b110110110),
     R(0b110110110),
     R(0b000000000),
     R(0b000000000),
     R(0b000000000)}, // t0: ...
    {R(0b001111100),
     R(0b011000110),
     R(0b011000110),
     R(0b000001100),
     R(0b000011000),
     R(0b000011000),
     R(0b000000000),
     R(0b000011000),
     R(0b000011000)}, // t1: ?
    {R(0b000011110),
     R(0b000010010),
     R(0b000010010),
     R(0b000010000),
     R(0b000010000),
     R(0b011110000),
     R(0b111110000),
     R(0b111110000),
     R(0b011100000)}, // t2: a note
    {R(0b011000110),
     R(0b111101111),
     R(0b111111111),
     R(0b111111111),
     R(0b011111110),
     R(0b001111100),
     R(0b000111000),
     R(0b000010000),
     R(0b000000000)}, // t3: a heart
    {R(0b000010000),
     R(0b000010000),
     R(0b000111000),
     R(0b111111111),
     R(0b000111000),
     R(0b000010000),
     R(0b000010000),
     R(0b000000000),
     R(0b000000000)}, // t4: a sparkle
    {R(0b000010000),
     R(0b000010000),
     R(0b000111000),
     R(0b000111000),
     R(0b001111100),
     R(0b001111100),
     R(0b001111100),
     R(0b000111000),
     R(0b000000000)}, // tired: a drop of sweat
    {R(0b011111110),
     R(0b111111111),
     R(0b110111011),
     R(0b110111011),
     R(0b111111111),
     R(0b011111110),
     R(0b001010100),
     R(0b001111100),
     R(0b000000000)}, // psn: a skull
    {R(0b111111000),
     R(0b000110000),
     R(0b001100111),
     R(0b011000010),
     R(0b111111100),
     R(0b000000111),
     R(0b000000010),
     R(0b000000111),
     R(0b000000000)}, // slp: Zz
    {R(0b001000100),
     R(0b001100100),
     R(0b011101100),
     R(0b011111110),
     R(0b111111111),
     R(0b111111111),
     R(0b111111111),
     R(0b011111110),
     R(0b001111100)}, // brn: a flame
    {R(0b000010000),
     R(0b010010010),
     R(0b001010100),
     R(0b000111000),
     R(0b111111111),
     R(0b000111000),
     R(0b001010100),
     R(0b010010010),
     R(0b000010000)}, // frz: a snowflake
    {R(0b000001100),
     R(0b000011000),
     R(0b000110000),
     R(0b001111100),
     R(0b000011000),
     R(0b000110000),
     R(0b001100000),
     R(0b001000000),
     R(0b000000000)}, // par: a bolt
};

typedef struct {
    int16_t x, y; // the balloon's top-left corner
    uint16_t t; // ms the balloon has been up alone
    uint8_t what; // F_*
    uint8_t phase; // 0 the balloon alone, 1 the text box is above us
    uint8_t slot; // the party slot that follows (its name goes into {v0})
} Follow;
static Follow g_follow;
static uint8_t g_live; // the scene is on the stack (keeps the plugin loaded: casino_active)
bool scene_follow_active(void) {
    return g_live != 0;
}

#ifndef NUCLEO_DEVICE
extern int g_fol_on; // world.c (host only): the follower is off in the older harnesses
#endif
bool scene_follow_open(SceneCtx* c) {
    const World* w = c->world;
    const Party* p = party_player();
    uint8_t k = 0;
    while(k < p->count && !p->mon[k].hp)
        k++;
#ifndef NUCLEO_DEVICE
    if(!g_fol_on) return false;
#endif
    if(k >= p->count || c->n + 2 > MAX_SCENES || world_on_bike() || world_on_surf() ||
       (w->map.bg_level & MAP_BG_NOFOL) ||
       (cflags_at(&w->map, w->fol_i, w->fol_j) & (C_SOLID | C_WATER)))
        return false; // (not shown: world_fol_pose)
    const Mon* m = &p->mon[k];
    // what it says: a status first, then HP under a third, else the friendship tier
    const uint8_t st = m->status;
    const uint8_t what = (st & BS_SLP_MASK) ? F_SLP :
                         (st & BS_FRZ)      ? F_FRZ :
                         (st & BS_PAR)      ? F_PAR :
                         (st & BS_BRN)      ? F_BRN :
                         (st & BS_PSN)      ? F_PSN :
                         (uint32_t)m->hp * 3 < m->max_hp ?
                                         F_TIRED :
                                         (uint8_t)(F_T0 + bc_friend_tier(bc_friend(m)));
    // where: the hero's feet are at (64, 33) and a tile is 28 x 14 px (render.c); the balloon sits above the top row of its picture (the table at offset 16 of files/follow.bin, one byte a species)
    const int di = w->fol_i - w->hero.ti, dj = w->fol_j - w->hero.tj;
    uint8_t top = 0;
    read_at(FOL_FILE, 15u + m->species, &top, 1);
    int x = 64 + (di - dj) * 14 - BUB_W / 2,
        y = 33 + (di + dj) * 7 - (FOL_H - 1) + top - BUB_H - 1;
    g_follow = (Follow){
        .x = (int16_t)(x < 0           ? 0 :
                       x > 128 - BUB_W ? 128 - BUB_W :
                                         x),
        .y = (int16_t)(y < 0 ? 0 : y),
        .what = what,
        .slot = k};
    return scene_push(c, &scene_follow_ops, &g_follow);
}

static void f_enter(SceneCtx* c, void* d) {
    (void)d;
    g_live++;
    scene_dirty(c, D_COMPOSE);
    scene_wake(c, 1);
}

static void f_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    g_live--;
}

static void f_input(SceneCtx* c, void* d, const InputEv* ev) {
    if(ev->key == KEY_BACK && ev->type == EV_LONG)
        scene_clear(c);
    else if(ev->key == KEY_OK && ev->type == EV_PRESS) { // skip the wait
        ((Follow*)d)->t = FOLLOW_BUBBLE_MS;
        scene_wake(c, 1);
    }
}

static void f_update(SceneCtx* c, void* d, uint32_t dt) {
    Follow* f = d;
    if(!f->phase && (f->t = (uint16_t)(f->t + (dt < FOLLOW_BUBBLE_MS ? dt : FOLLOW_BUBBLE_MS))) <
                        FOLLOW_BUBBLE_MS) {
        scene_wake(c, FOLLOW_BUBBLE_MS - f->t);
        return;
    }
    if(!f->phase) { // the wait is over: the line
        tchar nm[16];
        party_mon_name(f->slot, nm, 16);
        text_set_var(0, nm);
        f->phase = 1;
        if(scene_dialog_open(c, (uint16_t)(TXT_GL_FOL_T0 + f->what), -1)) return;
    }
    scene_pop(c); // on top again (the text box closed), or it could not open
    scene_wake(c, 1);
}

static void f_draw_world(SceneCtx* c, void* d) {
    (void)d;
    world_draw_frozen(c);
}

static void f_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Follow* f = d;
    ov_fill(f->x + 1, f->y + 1, BUB_W - 2, 11, 0);
    ov_bitmap(f->x, f->y, BUB_W, BUB_H, BUBBLE, 2, 1);
    ov_bitmap(f->x + 3, f->y + 2, 9, 9, SIGN[f->what], 2, 1);
}

const SceneOps scene_follow_ops = {
    .enter = f_enter,
    .leave = f_leave,
    .input = f_input,
    .update = f_update,
    .draw_world = f_draw_world,
    .draw_overlay = f_draw_overlay,
    .pauses_below = true,
};
