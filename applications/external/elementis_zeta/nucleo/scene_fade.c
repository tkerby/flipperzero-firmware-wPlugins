// FADE scene: OUT (fade 1..3, overlays hidden) -> LOAD (commit; the screen is already white) ->
// IN (fade 3..0) -> pop. Used for warps, title -> world and language picker -> title.
// Script jobs: FJ_OUT = OUT only, then pop leaving the screen white (World.fade_hold, c->fade stay at
// FADE_STEPS, overlays hidden); FJ_IN = IN only (from the held white). A warp started while the
// screen is held skips the fade-out steps (it starts white).
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "save.h"
#include "ui_host.h"
#include "sound.h"

typedef enum { FJ_WARP, FJ_NEW_GAME, FJ_CONTINUE, FJ_TITLE, FJ_OUT, FJ_IN } FadeJobKind;
typedef enum { PH_OUT, PH_IN } FadePhase;

typedef struct {
    FadeJobKind kind;
    FadePhase phase;
    uint8_t map, ti, tj, facing; // FJ_WARP target
    uint32_t acc;                // ms spent in the current step
} Fade;

static Fade g_fade; // only one fade exists at a time

// Replaces the scene just below the FADE scene (title -> world) without touching the FADE state.
static void swap_below(SceneCtx* c, const SceneOps* ops) {
    if(c->n < 2) return;
    SceneEntry* e = &c->st[c->n - 2];
    if(e->ops->leave) e->ops->leave(c, e->data);
    e->ops = ops;
    e->data = NULL;
    if(ops->enter) ops->enter(c, NULL);
}

// Runs while the screen is white (the OUT phase's last frame was published before this update).
static void commit(SceneCtx* c, Fade* f) {
    World* w = c->world;
    w->fade_hold = 0; // from here the fade-in is driven by this scene
    if(f->kind == FJ_NEW_GAME || f->kind == FJ_CONTINUE) {
        if(f->kind == FJ_NEW_GAME || !save_continue(w)) {
            save_time_set(0);
            world_new_game(w); // an error is shown by the world scene
        }
        swap_below(c, &scene_world_ops);
    } else if(f->kind == FJ_TITLE) {
        const SceneOps* t = ui_title_ops(); // the picker (UI plugin) is on, so the plugin is loaded already
        if(t) swap_below(c, t);
    } else if(w->map.w && w->map.id == f->map) {
        world_place(w, f->ti, f->tj, f->facing);
    } else {
        world_load_map(w, f->map, f->ti, f->tj, f->facing);
    }
    w->sc = c;
    plugin_log_heap("after map load (title plugin and old map gone or not yet unloaded)"); // serial `log`: where the heap goes on the way into a map
    // the scene enter hooks reset these
    c->fade = 3;
    c->hide_overlay = true;
}

static bool fade_start(SceneCtx* c) {
    g_fade.acc = 0;
    g_fade.phase = g_fade.kind == FJ_IN ? PH_IN : PH_OUT;
    return scene_push(c, &scene_fade_ops, &g_fade);
}

bool scene_fade_out(SceneCtx* c) {
    if(c->n >= MAX_SCENES) return false;
    g_fade.kind = FJ_OUT;
    return fade_start(c);
}

bool scene_fade_in(SceneCtx* c) {
    if(c->n >= MAX_SCENES) return false;
    g_fade.kind = FJ_IN;
    return fade_start(c);
}

bool scene_fade_warp(SceneCtx* c, uint8_t map, uint8_t ti, uint8_t tj, uint8_t facing) {
    if(c->n >= MAX_SCENES) return false;
    world_stop(c->world);
    sound_play(SND_FX_DOOR); // door / warp (contract 28)
    g_fade.kind = FJ_WARP;
    g_fade.map = map;
    g_fade.ti = ti;
    g_fade.tj = tj;
    g_fade.facing = facing & 7;
    return fade_start(c);
}

bool scene_fade_new_game(SceneCtx* c) {
    g_fade.kind = FJ_NEW_GAME;
    return fade_start(c);
}

bool scene_fade_continue(SceneCtx* c) {
    g_fade.kind = FJ_CONTINUE;
    return fade_start(c);
}

bool scene_fade_to_title(SceneCtx* c) {
    g_fade.kind = FJ_TITLE;
    return fade_start(c);
}

static void f_enter(SceneCtx* c, void* d) {
    (void)d;
    c->fade = (c->world->fade_hold || g_fade.kind == FJ_IN) ? FADE_STEPS : 1;
    c->hide_overlay = true;
    scene_dirty(c, D_COMPOSE);
    scene_wake(c, FADE_STEP_MS);
}

static void f_leave(SceneCtx* c, void* d) {
    (void)d;
    c->fade = c->world->fade_hold; // 0, or the held white of a script FADE_OUT
    c->hide_overlay = c->world->fade_hold != 0;
}

static void f_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
}

static void f_update(SceneCtx* c, void* d, uint32_t dt) {
    Fade* f = d;
    f->acc += dt < WCLOCK_MAX_DT ? dt : WCLOCK_MAX_DT;
    if(f->acc < FADE_STEP_MS) {
        scene_wake(c, FADE_STEP_MS - f->acc);
        return;
    }
    f->acc = 0;
    if(f->phase == PH_OUT) {
        if(c->fade < FADE_STEPS) {
            c->fade++;
        } else if(f->kind == FJ_OUT) {
            c->world->fade_hold = FADE_STEPS;
            scene_pop(c); // leave() keeps the screen white
            scene_wake(c, 1);
            return;
        } else {
            commit(c, f); // white frame is already on screen
            f->phase = PH_IN;
            scene_dirty(c, D_WORLD);
        }
        scene_dirty(c, D_COMPOSE);
    } else {
        if(c->fade > 1) {
            c->fade--;
            scene_dirty(c, D_COMPOSE);
        } else {
            c->world->fade_hold = 0;
            scene_pop(c); // leave() resets fade / overlay
            scene_wake(c, 1); // let the world run its own update right away
            return;
        }
    }
    scene_wake(c, FADE_STEP_MS);
}

const SceneOps scene_fade_ops = {
    .enter = f_enter,
    .leave = f_leave,
    .input = f_input,
    .update = f_update,
    .draw_world = NULL,
    .draw_overlay = NULL,
    .pauses_below = true,
};
