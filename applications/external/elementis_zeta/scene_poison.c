// (contract 81, casino plugin): the poison of the overworld. World.c counts the hero's steps (poison_step) and, on every 4th one with a poisoned creature on its feet,
// asks for the act WACT_POISON (menu_open_daycare -> the casino plugin's `bond` operation -> casino.c -> poison_act): every poisoned creature that still stands loses 1 HP (its status is cleared when that was
// its last HP; a later version keeps it at 1 HP, the first ones do not), the screen flashes (code-drawn: black / clear / black / clear, no art) with a sound, then one "{v0} fainted!" box for each creature that
// fainted and, when nobody is left standing, the normal blackout (World.bo = BO_SAY1: the world runs the lines, the half-money loss and the fade home on the first update after this scene pops).
// The scene sits BELOW the dialogs it opens (like scene_follow.c) and closes itself the first time it is on top again.
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "party.h"
#include "frame.h"
#include "sound.h"
#include "casino_host.h"
#include "gen/maps_gen.h"

#define PSN_FLASH_MS 80 // one phase of the flash
#define PSN_PHASES   4 // black, clear, black, clear

typedef struct {
    uint16_t t; // ms into the flash
    uint8_t nf, done; // creatures that fainted just now, lines shown so far
    uint8_t slot[BC_MAX_PARTY];
} Psn;
static Psn g_psn;
static uint8_t g_psn_live; // the scene is on the stack (keeps the plugin loaded: casino_active)

bool scene_poison_active(void) {
    return g_psn_live != 0;
}

static void black_out_if_none(
    SceneCtx*
        c) { // nobody left standing: the blackout (World.bo; the world runs it on its first update after the scenes above have closed)
    const Party* p = party_player();
    for(uint8_t k = 0; k < p->count; k++)
        if(p->mon[k].hp) return;
    c->world->bo = BO_SAY1;
}

// The act: damage first (the party is changed even if the scene cannot open), then the scene. True = pushed.
bool poison_act(SceneCtx* c) {
    Party* p = party_player();
    Psn* s = &g_psn;
    s->t = s->nf = s->done = 0;
    for(uint8_t k = 0; k < p->count; k++) {
        Mon* m = &p->mon[k];
        if(m->hp && (m->status & BS_PSN) && !--m->hp) { //
            m->status = 0;
            s->slot[s->nf++] = k;
        }
    }
    if(c->n + 2 > MAX_SCENES || !scene_push(c, &scene_poison_ops, s)) {
        black_out_if_none(c); // (no room for a scene: the blackout still happens)
        return false;
    }
    return true;
}

static void g_enter(SceneCtx* c, void* d) {
    (void)d;
    g_psn_live++;
    sound_play(SND_FX_LOWHP);
    scene_dirty(c, D_COMPOSE);
    scene_wake(c, 1);
}

static void g_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    g_psn_live--;
}

static void g_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
}

static void g_update(SceneCtx* c, void* d, uint32_t dt) {
    Psn* s = d;
    if(s->t < PSN_PHASES * PSN_FLASH_MS) { // the flash
        s->t = (uint16_t)(s->t + (dt < PSN_FLASH_MS ? dt : PSN_FLASH_MS));
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, PSN_FLASH_MS - s->t % PSN_FLASH_MS);
        return;
    }
    if(s->done < s->nf) { // one box for each creature that fainted
        tchar nm[16];
        party_mon_name(s->slot[s->done++], nm, 16);
        text_set_var(0, nm);
        if(!scene_dialog_open(c, TXT_GL_BT_FAINTED, -1))
            scene_wake(c, 1); // (the box could not open: the next line, or the end)
        return;
    }
    black_out_if_none(c); // on top again with every line shown
    scene_pop(c);
    scene_wake(c, 1);
}

static void g_draw_world(SceneCtx* c, void* d) {
    (void)d;
    world_draw_frozen(c);
}

static void g_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Psn* s = d;
    if(s->t < PSN_PHASES * PSN_FLASH_MS && !(s->t / PSN_FLASH_MS & 1))
        ov_fill(0, 0, SCREEN_W, SCREEN_H, 1);
}

const SceneOps scene_poison_ops = {
    .enter = g_enter,
    .leave = g_leave,
    .input = g_input,
    .update = g_update,
    .draw_world = g_draw_world,
    .draw_overlay = g_draw_overlay,
    .pauses_below = true,
};
