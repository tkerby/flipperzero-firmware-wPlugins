// NAME scene (+ contract 66: the AFFINITY picker, who = 2): classic name picker (new game: your name, then the rival's), reached through the script native
// name_select (natives.c). Layout: a plain light-gray screen with the hero on the right; first the question as an
// ordinary dialog (gl.name.ask_player / ask_rival, over this scene), then the question's last line as a heading and
// the list in a framed box: "NUEVO NOMBRE" (gl.kb.new) and the first three suggestions (gl.name.p0..p2 / r0..r2, so the
// lists follow the language). Up / Down move (wraps). OK on a suggestion stores it and pops (the script resumes); OK on
// "NUEVO NOMBRE" opens the keyboard (scene_kb.c) above this scene, whose result (a stored name) pops this scene too, and
// whose cancel (Back on an empty line) shows the list again. Back short does nothing (a name is mandatory), Back long
// exits the app like everywhere.
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "party.h"
#include "ui_host.h"
#include "render.h"
#include "gen/maps_gen.h"

#define N_NAMES 4 // "NUEVO NOMBRE" + 3 suggestions
#define WHO_AFF 2 // contract 66: the Solar / Lunar picker (2 rows: gl.aff.solar, gl.aff.lunar) reuses this scene: a heading, a framed list, no keyboard
#define NAME_CAP 16
#define HEAD_CAP 32
#define BG_LEVEL 1
#define BOX_X 2
#define BOX_PAD_X 3
#define CURSOR_GAP 2
#define HERO_X 108 // the hero's picture (player name only), top-right (the heading, up to ~106 px, ends before it)

typedef struct {
    tchar names[N_NAMES][NAME_CAP];
    tchar head[HEAD_CAP]; // the question's last line
    uint8_t who;          // 0 player, 1 rival, WHO_AFF the affinity
    uint8_t sel;
    uint8_t cnt;          // rows of the list: N_NAMES, or 2 for the affinity
    bool list;            // false: the question dialog is (or was just) up, true: the list is shown
    bool kb;              // the keyboard is (or was just) on top
    int16_t box_y, box_w, box_h, head_y;
} NameScene;

static NameScene g_nm;
uint8_t g_peek_aff;   // contract 66: set by save_io.c's validate (the plugin's globals live in its own .bss)
uint8_t g_aff_choice; // contract 66: the affinity picked in the title's Continuar prompt (the loaded save does not have one yet); save_io.c applies it after the file is read

static uint16_t ask_id(const NameScene* n) {
    return n->who == WHO_AFF ? TXT_GL_AFF_ASK : n->who ? TXT_GL_NAME_ASK_RIVAL : TXT_GL_NAME_ASK_PLAYER;
}

bool scene_name_open(SceneCtx* c, uint8_t who) {
    if(c->n + 1 >= MAX_SCENES) return false; // room for the dialog above (and later the keyboard)
    NameScene* n = &g_nm;
    n->who = who > WHO_AFF ? 1 : who;
    n->cnt = n->who == WHO_AFF ? 2 : N_NAMES;
    n->sel = 0;
    n->list = false;
    n->kb = false;
    if(!scene_push(c, &scene_name_ops, n)) return false;
    // the question goes over this scene as a normal dialog; when it closes this scene is on top: the list
    scene_dialog_open(c, ask_id(n), -1);
    return true;
}

// Loads the names and the heading (the active language) and measures the box.
static void nm_load_all(NameScene* n) {
    const uint16_t base = n->who ? TXT_GL_NAME_R0 : TXT_GL_NAME_P0;
    int wmax = 0;
    const tchar* all[N_NAMES + 1];
    for(int k = 0; k < n->cnt; k++) {
        if(text_load(n->who == WHO_AFF ? (uint16_t)(TXT_GL_AFF_SOLAR + k) : k == 0 ? TXT_GL_KB_NEW : (uint16_t)(base + k - 1), n->names[k], NAME_CAP) < 0) n->names[k][0] = 0;
        all[k] = n->names[k];
        int w = font_text_width(n->names[k]);
        if(w > wmax) wmax = w;
    }
    // heading = first line of the question's LAST page
    tchar q[TB_BUF];
    n->head[0] = 0;
    if(text_load(ask_id(n), q, TB_BUF) >= 0) {
        int pos = 0, nx;
        while((nx = text_next_page(q, pos)) >= 0) pos = nx;
        int len = text_line_len(q, pos);
        if(len >= HEAD_CAP) len = HEAD_CAP - 1;
        for(int i = 0; i < len; i++) n->head[i] = q[pos + i];
        n->head[len] = 0;
    }
    all[n->cnt] = n->head;
    text_prefetch_many(all, n->cnt + 1); // SD-backed (hanzi) fonts: make the glyphs resident now
    const int pitch = font_pitch();
    n->head_y = 0;
    n->box_y = (int16_t)pitch;
    n->box_w = (int16_t)(BOX_PAD_X + font_cell_w() + CURSOR_GAP + wmax + BOX_PAD_X + 1);
    n->box_h = (int16_t)(n->cnt * pitch + 2 * TB_PAD_Y);
}

static void n_enter(SceneCtx* c, void* d) {
    (void)d;
    ui_live_enter();
    scene_dirty(c, D_WORLD);
}

static void n_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    ui_live_leave();
}

static void n_input(SceneCtx* c, void* d, const InputEv* ev) {
    NameScene* n = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        return;
    }
    if(!n->list) return;
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        n->sel = (uint8_t)((n->sel + (ev->key == KEY_DOWN ? 1 : n->cnt - 1)) % n->cnt);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        if(n->who == WHO_AFF) { // contract 66: Solar / Lunar; a new game has it in the state at once, a save being continued gets it from g_aff_choice (save_io.c)
            party_state()->aff = g_aff_choice = (uint8_t)(n->sel + AFF_SOLAR);
            scene_pop(c);
            scene_wake(c, 1);
        } else if(n->sel == 0) {
            n->kb = scene_kb_open(c, n->who, 0); // (a keyboard that cannot open leaves the list as it is)
        } else {
            party_set_name(n->who != 0, (uint8_t)(n->sel - 1));
            scene_pop(c);
            scene_wake(c, 1); // the world resumes the script
        }
    }
}

static void n_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    NameScene* n = d;
    if(n->kb) { // the keyboard above has closed
        n->kb = false;
        if(scene_kb_done()) {
            scene_pop(c);
            scene_wake(c, 1); // the world resumes the script
        } else {
            scene_dirty(c, D_COMPOSE);
        }
        return;
    }
    if(!n->list) { // first time we are on top: the question dialog above us has closed
        n->list = true;
        nm_load_all(n);
        scene_dirty(c, D_COMPOSE);
    }
}

static void n_draw_world(SceneCtx* c, void* d) {
    NameScene* n = d;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, BG_LEVEL);
    if(n->who == 0) { // the hero's picture
        Blit b;
        hero_view(0, 0, &b);
        blit_auto(lvl, &b, HERO_X, 2);
    }
}

static void n_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    NameScene* n = d;
    if(!n->list) return;
    const int pitch = font_pitch();
    text_draw_line(n->head, 0, 4, n->head_y, 99);
    ov_fill(BOX_X, n->box_y, n->box_w, n->box_h, 0);
    ov_frame(BOX_X, n->box_y, n->box_w, n->box_h, 1);
    const int tx = BOX_X + BOX_PAD_X + font_cell_w() + CURSOR_GAP;
    for(int k = 0; k < n->cnt; k++) {
        int ly = n->box_y + TB_PAD_Y + k * pitch;
        text_draw_line(n->names[k], 0, tx, ly, 99);
        if(n->sel == k) font_draw_glyph(GLYPH_CURSOR, BOX_X + BOX_PAD_X, ly, 1);
    }
}

const SceneOps scene_name_ops = {
    .enter = n_enter,
    .leave = n_leave,
    .input = n_input,
    .update = n_update,
    .draw_world = n_draw_world,
    .draw_overlay = n_draw_overlay,
    .pauses_below = true,
};
