// LANG scene: first-run language picker, shown instead of the title while settings.bin is missing or
// invalid. Four entries drawn from the pre-rendered native names (gen/lang_names.h: each name is
// drawn with its own language font, so it needs no font to be loaded), a cursor, Up / Down, OK.
#include <stddef.h>
#include <string.h>
#include "scene.h"
#include "settings.h"
#include "ui_host.h"
#include "text.h"
#include "frame.h"
#include "gen/langs_gen.h"
#define LANG_NAMES_DEFINE // the one definition of the native-name bitmaps of the UI plugin (contract 38.5); scene_options.c reads the extern
#include "gen/lang_names.h"

#define ROW_PITCH 13
#define PLATE_PAD 4
#define NAME_X    14 // name left edge inside the plate (the cursor sits at +4)

static uint8_t g_cur;

// right-pointing cursor, 4 x 7 (bit 7 = left column)
static const uint8_t CURSOR_ROWS[7] = {0x80, 0xC0, 0xE0, 0xF0, 0xE0, 0xC0, 0x80};

// band levels, 8 rows each (same as the title)
static const uint8_t BANDS[8] = {3, 3, 2, 2, 1, 1, 0, 0};

static void geom(int* px, int* py, int* pw, int* ph) {
    int maxw = 0;
    for(int i = 0; i < LANG_COUNT; i++)
        if(LANG_NAME_BMP[i].w > maxw) maxw = LANG_NAME_BMP[i].w;
    *pw = NAME_X + maxw + 10;
    *ph = LANG_COUNT * ROW_PITCH + 2 * PLATE_PAD - 2;
    *px = (SCREEN_W - *pw) / 2;
    *py = (SCREEN_H - *ph) / 2;
}

static void l_enter(SceneCtx* c, void* d) {
    (void)d;
    ui_live_enter();
    int idx = settings_current_lang_index();
    g_cur = (uint8_t)(idx >= 0 ? idx : LANG_IDX_EN); // first run: the cursor starts on English
    scene_dirty(c, D_WORLD);
}

static void l_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    ui_live_leave();
}

static void l_choose(SceneCtx* c) {
    text_set_language(g_cur);
    settings_store_language(g_cur); // a failed write only means the picker shows again next launch
    scene_fade_to_title(c);
}

static void l_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP) {
        g_cur = (uint8_t)((g_cur + LANG_COUNT - 1) % LANG_COUNT);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_DOWN) {
        g_cur = (uint8_t)((g_cur + 1) % LANG_COUNT);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        l_choose(c);
    }
}

static void l_draw_world(SceneCtx* c, void* d) {
    (void)d;
    for(int y = 0; y < SCREEN_H; y++)
        memset(c->frame->lvl[y], lvl_pat(BANDS[y >> 3]), LVL_STRIDE);
}

static void l_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    int px, py, pw, ph;
    geom(&px, &py, &pw, &ph);
    ov_fill(px, py, pw, ph, 0);
    ov_frame(px, py, pw, ph, 1);
    for(int i = 0; i < LANG_COUNT; i++) {
        int cy = py + PLATE_PAD + i * ROW_PITCH + ROW_PITCH / 2 - 1; // row centre
        const LangNameBmp* b = &LANG_NAME_BMP[i];
        ov_bitmap(px + NAME_X, cy - b->h / 2, b->w, b->h, b->rows, b->bpr, 1);
        if(i == g_cur) ov_bitmap(px + 5, cy - 3, 4, 7, CURSOR_ROWS, 1, 1);
    }
}

const SceneOps scene_lang_ops = {
    .enter = l_enter,
    .leave = l_leave,
    .input = l_input,
    .update = NULL,
    .draw_world = l_draw_world,
    .draw_overlay = l_draw_overlay,
    .pauses_below = false,
};

bool scene_lang_open(SceneCtx* c) {
    return scene_push(c, &scene_lang_ops, NULL);
}
