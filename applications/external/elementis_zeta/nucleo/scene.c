#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-inline,no-tree-sink") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "scene.h"
#include "debug.h"
#include <string.h>

void scene_ctx_init(SceneCtx* c, Game* g) {
    memset(c, 0, sizeof(*c));
    c->game = g;
    c->frame = g->frame;
    c->world = g->world;
    c->wake_ms = SCENE_NO_WAKE;
    c->dirty = D_WORLD;
}

static void changed(SceneCtx* c) {
    input_flush();
    c->dirty |= D_WORLD;
}

bool scene_push(SceneCtx* c, const SceneOps* ops, void* data) {
    if(c->n >= MAX_SCENES) return false;
    c->st[c->n].ops = ops;
    c->st[c->n].data = data;
    c->n++;
    changed(c);
    if(ops->enter) ops->enter(c, data);
    return true;
}

void scene_pop(SceneCtx* c) {
    if(!c->n) return;
    SceneEntry e = c->st[--c->n];
    if(e.ops->leave) e.ops->leave(c, e.data);
    changed(c);
}

void scene_replace(SceneCtx* c, const SceneOps* ops, void* data) {
    if(c->n) {
        SceneEntry e = c->st[--c->n];
        if(e.ops->leave) e.ops->leave(c, e.data);
    }
    scene_push(c, ops, data);
}

bool scene_insert_below_top(SceneCtx* c, const SceneOps* ops, void* data) {
    if(c->n < 1 || c->n >= MAX_SCENES) return false;
    c->st[c->n] = c->st[c->n - 1];
    c->st[c->n - 1].ops = ops;
    c->st[c->n - 1].data = data;
    c->n++;
    changed(c);
    if(ops->enter) ops->enter(c, data);
    return true;
}

void scene_clear(SceneCtx* c) {
    while(c->n) scene_pop(c);
}

void scene_input(SceneCtx* c, const InputEv* ev) {
    if(!c->n) return;
    const SceneEntry e = c->st[c->n - 1];
    if(e.ops->input) e.ops->input(c, e.data, ev);
}

void scene_update(SceneCtx* c, uint32_t dt_ms) {
    c->wake_ms = SCENE_NO_WAKE;
    uint8_t n0 = c->n;
    for(int k = c->n - 1; k >= 0; k--) {
        const SceneEntry e = c->st[k];
        if(e.ops->update) e.ops->update(c, e.data, dt_ms);
        if(c->n != n0 || c->st[k].ops != e.ops) break; // the stack changed under us
        if(e.ops->pauses_below) break;
    }
}

bool scene_render(SceneCtx* c) {
    if(!c->dirty || !c->n) return false;
    uint8_t d = c->dirty;
    c->dirty = 0;
    int s0 = c->n - 1;
    while(s0 > 0 && !c->st[s0].ops->draw_world) s0--;
    if(d & D_WORLD) {
        debug_render_begin();
        frame_dither_reset(c->frame); // only the world scene (w_draw_world) scrolls the dither with its camera
        if(c->st[s0].ops->draw_world) c->st[s0].ops->draw_world(c, c->st[s0].data);
        debug_render_end();
    }
    game_wait_ready(c->frame);
    debug_compose_begin();
    frame_compose(c->frame, c->fade);
    debug_compose_end();
    if(!c->hide_overlay) {
        for(int k = s0; k < c->n; k++)
            if(c->st[k].ops->draw_overlay) c->st[k].ops->draw_overlay(c, c->st[k].data);
    }
    frame_publish(c->frame);
    return true;
}
