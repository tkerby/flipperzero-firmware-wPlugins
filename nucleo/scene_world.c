// WORLD scene: owns the hero input -> world_update path, NPC drawing and the debug overlay.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-thread-jumps,no-optimize-sibling-calls") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "scene.h"
#include "world.h"
#include "render.h"
#include "font.h"
#include "frame.h"
#include "debug.h"
#include "assets.h"
#include "bag.h"
#include <stdio.h>
#include <string.h>

#ifdef NUCLEO_DEBUG
#define DBG_LINES 4
#define DBG_CAP 24
static uint16_t dbg_hint_ms; // the debug bag combo's hint (line 0) stays this long (contract 70: the V1 A/B switch, its free-text hint and the change hash of the overlay are gone)
static bool dbg_hint_ok;

// map / hero, timings (R render ms, C compose ms, F flips/s, Q us per flip), heap (F free, M largest block, m minimum
// since boot, KB), game-thread stack headroom (bytes never used) + battle plugin loaded
static void dbg_format(const World* w, char t[DBG_LINES][DBG_CAP]) {
    if(dbg_hint_ms) snprintf(t[0], DBG_CAP, "%s", dbg_hint_ok ? "BAG +5C +2P" : "BAG FULL");
    else
        snprintf(
            t[0], DBG_CAP, "M%u %u,%u f%u", (unsigned)w->map.id, (unsigned)w->hero.ti, (unsigned)w->hero.tj,
            (unsigned)w->hero.facing);
    debug_format(t[1], DBG_CAP);
    debug_format_mem(t[2], DBG_CAP);
    debug_format_stack(t[3], DBG_CAP);
}

static void plate(const char* s, int y) {
    ov_fill(0, y, (int)strlen(s) * FONT_ADV + 3, 11, 0);
    font_draw_str((const uint8_t*)s, 1, y); // (the system font's codes are ASCII)
}
#endif

// Debug overlay shared with the battle plugin (the world scene is not drawn while a battle is on): call update from the
// scene's update (marks the frame dirty and wakes every 250 ms while the overlay is on) and draw from its overlay.
void scene_debug_overlay_update(SceneCtx* c) {
#ifdef NUCLEO_DEBUG
    World* w = c->world;
    if(!w || !w->dbg_on) return;
    scene_dirty(c, D_COMPOSE); // the numbers are redrawn 4 times a second (contract 70: no hash of the text any more)
    scene_wake(c, 250);
#else
    (void)c;
#endif
}

void scene_debug_overlay_draw(SceneCtx* c) {
#ifdef NUCLEO_DEBUG
    World* w = c->world;
    if(!w || !w->dbg_on) return;
    char t[DBG_LINES][DBG_CAP];
    dbg_format(w, t);
    for(int k = 0; k < DBG_LINES; k++) plate(t[k], k * 10);
#else
    (void)c;
#endif
}

void world_draw_frozen(SceneCtx* c) {
    scene_world_ops.draw_world(c, NULL);
}

static void on_back_short(SceneCtx* c) {
    c->world->area_on = 0; // the menu can change the language: the banner's codes would be stale
    scene_menu_open(c); // the world stays frozen (even mid-step) until the menu closes
}

static void w_enter(SceneCtx* c, void* d) {
    (void)d;
    c->world->sc = c;
    c->world->area_last = 0; // a game just started or continued: the zone banner shows once for the map it starts in
    c->world->area_on = 0;
    c->fade = 0;
    c->hide_overlay = false;
    scene_dirty(c, D_WORLD);
    scene_wake(c, 1);
}

static void w_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    World* w = c->world;
    const bool locked = script_input_locked(&w->scr); // a cutscene owns the controls (Back long still exits)
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        else if(ev->type == EV_SHORT && !locked) on_back_short(c);
        return;
    }
    if(locked) return;
#ifdef NUCLEO_DEBUG
    // debug combo (contract 14.6): with the overlay on, Left + Right held and Up tapped = +5 Crystals, +2 Salves in the bag
    // (the only way to reach the capture code in M3). input_flush keeps the held Up from walking the hero.
    if(ev->key == KEY_UP && ev->type == EV_PRESS && w->dbg_on && (input_held() & (KEY_LEFT | KEY_RIGHT)) == (KEY_LEFT | KEY_RIGHT)) {
        dbg_hint_ok = bag_debug_give();
        dbg_hint_ms = 1600;
        input_flush();
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, 250);
        return;
    }
#endif
    if(ev->key == KEY_OK && ev->type == EV_PRESS) {
#ifdef NUCLEO_DEBUG
        // debug overlay toggle: OK pressed while two opposite d-pad keys are held (they cancel,
        // so the hero does not move)
        uint8_t h = input_held();
        if((h & (KEY_LEFT | KEY_RIGHT)) == (KEY_LEFT | KEY_RIGHT) ||
           (h & (KEY_UP | KEY_DOWN)) == (KEY_UP | KEY_DOWN)) {
            w->dbg_on = !w->dbg_on;
            scene_dirty(c, D_COMPOSE);
            return;
        }
#endif
        if(w->map.w == 0) return;
        if(world_is_stepping(w)) w->ok_buf = true; // handled at arrival
        else if(on_interact_pressed(w)) scene_dirty(c, D_WORLD);
    }
}

static void w_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)d;
    World* w = c->world;
    uint32_t wake = 0;
#ifdef NUCLEO_DEBUG
    if(dbg_hint_ms) dbg_hint_ms = dt >= dbg_hint_ms ? 0 : (uint16_t)(dbg_hint_ms - dt);
#endif
    uint8_t dirty = world_update(w, dt, c->held, &wake);
    scene_dirty(c, dirty);
    if(wake) scene_wake(c, wake);
    scene_debug_overlay_update(c);
}

static void w_draw_world(SceneCtx* c, void* d) {
    (void)d;
    World* w = c->world;
    if(w->map.w == 0) {
        lvl_fill(c->frame->lvl, 0);
        return;
    }
    RenderHero rh;
    world_render_hero(w, &rh);
    int cx, cy;
    world_camera(w, &cx, &cy);
    render_anim_step = w->anim_step;
    world_fol_pose(w, &w->npc[w->map.n_npcs]); // contract 69: the follower is drawn as one NPC more (render_world loops to n_npcs)
    w->map.n_npcs++;
    render_world(c->frame->lvl, &w->map, cx, cy, &rh, w->npc);
    w->map.n_npcs--;
    frame_set_dither(c->frame, cx, cy); // the gray dither follows the world (and the hero, who stays put on the screen, keeps his own phase)
    frame_set_pins(c->frame, render_pin());
}

static void w_draw_overlay(SceneCtx* c, void* d) {
    (void)d;
    World* w = c->world;
#ifdef NUCLEO_DEBUG
    if(w->map.w == 0) {
        char e[24];
        snprintf(e, sizeof(e), "map load: E%d", (int)w->err); // (MAP_E_* of map.h)
        plate(e, 46);
    }
#endif
    int off = world_banner_off(w);
    if(off) { // zone-name banner (contract 22): a white plate with a 1 px frame, top-left, sliding down from above the screen
        int ph = font_pitch() + 4, pw = font_text_width(w->area_txt) + 7, y = (off < ph ? off : ph) - ph;
        ov_fill(0, y, pw, ph, 0);
        ov_frame(0, y, pw, ph, 1);
        font_draw_text(w->area_txt, 3, y + 2);
    }
    scene_debug_overlay_draw(c);
}

const SceneOps scene_world_ops = {
    .enter = w_enter,
    .input = w_input,
    .update = w_update,
    .draw_world = w_draw_world,
    .draw_overlay = w_draw_overlay,
    .pauses_below = false,
};
