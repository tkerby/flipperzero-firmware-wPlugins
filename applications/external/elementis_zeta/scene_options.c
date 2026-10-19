// OPTIONS scene: opened from the pause menu ("Opciones"). Title + five rows (options and ours; contract 75):
//   Idioma   the current language name (pre-rendered in its own font, gen/lang_names.h); Left / Right switches it live (text_set_language + reload of this screen's and the menu's labels)
//   Sonido   Alto / Bajo / No (contract 28)
//   Texto    the text speed Rapido / Medio / Lento (stored 2 / 1 / 0 in settings.bin, applied live)
//   Efectos  the battle animations Si / No (No skips the moves' effect animations and the focus shots; the HP bar and the messages stay)
//   Estilo the battle style Cambio / Fijo (SHIFT / SET: Cambio asks "change Elementi?" before a trainer's next one)
// Three rows show at a time, the list scrolls (arrows at the right). Up / Down moves (wraps), Left / Right changes the value (wraps) and writes settings.bin on every change.
// OK / Back closes.
#include <stddef.h>
#include "scene.h"
#include "settings.h"
#include "sound.h"
#include "ui_host.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "gen/maps_gen.h"
#include "gen/langs_gen.h"
#include "gen/lang_names.h"

#define OPT_LABEL_CODES 16
#define OPT_X           10 // title / row label x (the cursor sits at 3)
#define OPT_VALUE_R     114 // right edge of the value slot (the right arrow sits after it)
#define OPT_ROWS        5 // Idioma, Sonido, Texto, Efectos, Estilo
#define OPT_VIS         3 // rows on the screen; the list scrolls

_Static_assert(
    TXT_GL_OPT_SOUND_LO == TXT_GL_OPT_SOUND_HI + 1 &&
        TXT_GL_OPT_SOUND_OFF == TXT_GL_OPT_SOUND_HI + 2,
    "the sound values are consecutive text ids");
_Static_assert(
    TXT_GL_OPT_SPEED_MID == TXT_GL_OPT_SPEED_FAST + 1 &&
        TXT_GL_OPT_SPEED_SLOW == TXT_GL_OPT_SPEED_FAST + 2 &&
        TXT_GL_OPT_ANIM_OFF == TXT_GL_OPT_ANIM_ON + 1 &&
        TXT_GL_OPT_STYLE_SET == TXT_GL_OPT_STYLE_SHIFT + 1,
    "the F3 values are consecutive text ids");

// Rows 1..4: the label, the first value's text id (the others follow) and how many values the row has.
static const struct {
    uint16_t lab, val;
    uint8_t n;
} OPT_ROW[OPT_ROWS - 1] = {
    {TXT_GL_OPT_SOUND, TXT_GL_OPT_SOUND_HI, 3},
    {TXT_GL_OPT_SPEED, TXT_GL_OPT_SPEED_FAST, 3},
    {TXT_GL_OPT_ANIM, TXT_GL_OPT_ANIM_ON, 2},
    {TXT_GL_OPT_STYLE, TXT_GL_OPT_STYLE_SHIFT, 2},
};

typedef struct {
    tchar title[OPT_LABEL_CODES];
    tchar lab[OPT_VIS][OPT_LABEL_CODES]; // the labels of the visible rows ("Idioma" for row 0)
    tchar val[OPT_VIS]
             [OPT_LABEL_CODES]; // their current values (row 0: the language name is a bitmap)
    uint8_t row, top; // the cursor row 0..4, the first visible row
} Options;

static Options g_opt;

// 3 x 5 arrows (bit 7 = left column)
static const uint8_t ARROW_L[5] = {0x20, 0x60, 0xE0, 0x60, 0x20};
static const uint8_t ARROW_R[5] = {0x80, 0xC0, 0xE0, 0xC0, 0x80};

// The index (into the row's values) of the setting a row shows: Sonido Alto / Bajo / No; Texto Rapido / Medio / Lento (stored 2 / 1 / 0; the old "instant" reads as Rapido);
// Efectos Si / No and Estilo Cambio / Fijo (Settings.battle bit row - 3).
static int opt_get(int row) {
    const uint8_t s = g_settings.text_speed;
    return row == 1 ? g_settings.sound :
           row == 2 ? 2 - (s > TEXT_SPEED_FAST ? TEXT_SPEED_FAST : s) :
                      (g_settings.battle >> (row - 3)) & 1;
}

static void opt_set(int row, int i) {
    if(row == 1)
        g_settings.sound = (uint8_t)i;
    else if(row == 2) {
        g_settings.text_speed = (uint8_t)(2 - i);
        text_set_speed(g_settings.text_speed); // live: the next text types at the new speed
    } else {
        const uint8_t bit = (uint8_t)(1 << (row - 3));
        g_settings.battle = (uint8_t)(i ? g_settings.battle | bit : g_settings.battle & ~bit);
    }
}

static void load_labels(Options* o) {
    if(text_load(TXT_GL_OPTIONS_TITLE, o->title, OPT_LABEL_CODES) < 0) o->title[0] = 0;
    text_prefetch(o->title);
    for(int v = 0; v < OPT_VIS; v++) {
        const int r = o->top + v;
        o->lab[v][0] = o->val[v][0] = 0;
        if(r >= OPT_ROWS) continue;
        if(text_load(
               r ? OPT_ROW[r - 1].lab : TXT_GL_OPTIONS_LANGUAGE, o->lab[v], OPT_LABEL_CODES) < 0)
            o->lab[v][0] = 0;
        if(r &&
           text_load((uint16_t)(OPT_ROW[r - 1].val + opt_get(r)), o->val[v], OPT_LABEL_CODES) < 0)
            o->val[v][0] = 0;
        text_prefetch(o->lab[v]);
        text_prefetch(o->val[v]);
    }
}

static void switch_value(
    SceneCtx* c,
    Options* o,
    int dir) { // the value of the cursor row (wraps), stored at once; Sonido blips with the new volume (silent on No)
    const int n = OPT_ROW[o->row - 1].n;
    opt_set(o->row, (opt_get(o->row) + dir + n) % n);
    settings_save(&g_settings);
    load_labels(o);
    if(o->row == 1) sound_play(SND_FX_CONFIRM); // (contract 28)
    scene_dirty(c, D_COMPOSE);
}

static void o_enter(SceneCtx* c, void* d) {
    ((Options*)d)->row = ((Options*)d)->top = 0;
    ui_live_enter();
    load_labels(d);
    scene_dirty(c, D_COMPOSE);
}

// The menu below shows its labels again when this screen closes: keep them in the active language.
static void o_leave(SceneCtx* c, void* d) {
    (void)d;
    ui_live_leave();
    scene_menu_reload(c);
}

static void switch_language(SceneCtx* c, Options* o, int dir) {
    int prev = text_language();
    int next = (prev + dir + LANG_COUNT) % LANG_COUNT;
    if(next == prev) return;
    if(!text_set_language((uint8_t)next)) { // font missing: stay on the previous language
        text_set_language((uint8_t)prev);
        return;
    }
    settings_store_language((uint8_t)next);
    load_labels(o);
    scene_menu_reload(c);
    scene_dirty(c, D_COMPOSE);
}

static void o_input(SceneCtx* c, void* d, const InputEv* ev) {
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT) {
            scene_pop(c);
            scene_wake(c, 1);
        }
        return;
    }
    if(ev->type != EV_PRESS) return; // one step per press: every change writes the file
    Options* o = d;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        o->row = (uint8_t)((o->row + (ev->key == KEY_DOWN ? 1 : OPT_ROWS - 1)) % OPT_ROWS);
        if(o->row < o->top) o->top = o->row;
        if(o->row >= o->top + OPT_VIS) o->top = (uint8_t)(o->row - OPT_VIS + 1);
        load_labels(o);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_LEFT || ev->key == KEY_RIGHT) {
        int dir = ev->key == KEY_LEFT ? -1 : 1;
        if(o->row)
            switch_value(c, o, dir);
        else
            switch_language(c, o, dir);
    } else if(ev->key == KEY_OK) {
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static void o_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Options* o = d;
    int pitch = font_pitch();
    ov_fill(0, 0, SCREEN_W, SCREEN_H, 0);
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    font_draw_text(o->title, OPT_X, 3);
    ov_fill(4, 3 + pitch + 1, SCREEN_W - 8, 1, 1);
    int maxw = 0;
    for(int i = 0; i < LANG_COUNT; i++)
        if(LANG_NAME_BMP[i].w > maxw) maxw = LANG_NAME_BMP[i].w;
    const int slot = OPT_VALUE_R - maxw, ry = 3 + pitch + 5;
    for(int v = 0; v < OPT_VIS && o->top + v < OPT_ROWS; v++) {
        const int r = o->top + v, y = ry + v * (pitch + 3), cy = y + pitch / 2;
        if(r == o->row) font_draw_glyph(GLYPH_CURSOR, 3, y, 1);
        font_draw_text(o->lab[v], OPT_X, y);
        if(r)
            font_draw_text(o->val[v], OPT_VALUE_R - maxw / 2 - font_text_width(o->val[v]) / 2, y);
        else {
            const LangNameBmp* b = &LANG_NAME_BMP[text_language()];
            ov_bitmap(slot + (maxw - b->w) / 2, cy - b->h / 2, b->w, b->h, b->rows, b->bpr, 1);
        }
        if(r || LANG_COUNT > 1) {
            ov_bitmap(slot - 7, cy - 2, 3, 5, ARROW_L, 1, 1);
            ov_bitmap(OPT_VALUE_R + 4, cy - 2, 3, 5, ARROW_R, 1, 1);
        }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8},
                         DN[3] = {0xF8, 0x70, 0x20}; // 5 x 3 scroll arrows
    if(o->top > 0) ov_bitmap(122, ry, 5, 3, UP, 1, 1);
    if(o->top + OPT_VIS < OPT_ROWS) ov_bitmap(122, SCREEN_H - 5, 5, 3, DN, 1, 1);
}

const SceneOps scene_options_ops = {
    .enter = o_enter,
    .leave = o_leave,
    .input = o_input,
    .update = NULL,
    .draw_world = NULL,
    .draw_overlay = o_draw_overlay,
    .pauses_below = true,
};

bool scene_options_open(SceneCtx* c) {
    return scene_push(c, &scene_options_ops, &g_opt);
}

#ifndef NUCLEO_DEVICE
void scene_options_probe(
    int* row,
    int* top) { // host tests: the cursor row and the first visible row
    *row = g_opt.row;
    *top = g_opt.top;
}
#endif
