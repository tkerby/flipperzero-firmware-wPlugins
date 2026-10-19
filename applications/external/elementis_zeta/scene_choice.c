// CHOICE scene: YES / NO box pushed above a dialog's last page (right side, sitting on the text
// box). Up / Down move the cursor, OK selects, Back = No. The result (1 yes, 0 no) is written to the
// byte given to scene_choice_open() when the scene closes; the dialog below closes by itself.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-tree-loop-optimize,no-inline-functions-called-once,no-forward-propagate") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include <stddef.h>
#include "scene.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "sound.h"
#include "gen/maps_gen.h"

// Labels are the global texts gl.choice.yes / gl.choice.no. TEMPORARY FALLBACK until another task adds
// them to assets/text (then maps_gen.h defines TXT_GL_CHOICE_YES / _NO and the fallback is unused).
#ifndef TXT_GL_CHOICE_YES
#define TXT_GL_CHOICE_YES 0xFFFF
#endif
#ifndef TXT_GL_CHOICE_NO
#define TXT_GL_CHOICE_NO 0xFFFF
#endif

#define LABEL_CAP \
    6 // (the longest label is 4 codes, "いいえ" / "Yes" ...: contract 69 took 24 B of .bss out of the resident app; was 12)
#define BOX_PAD_X  3 // inside the frame, left of the cursor
#define CURSOR_GAP 2

typedef struct {
    tchar yes[LABEL_CAP], no[LABEL_CAP];
    uint8_t* out;
    uint8_t sel; // 0 = YES, 1 = NO
    int16_t x, y, w, h;
} Choice;

static Choice g_ch;

static void load_label(uint16_t id, tchar* dst, const char* fallback) {
    if(id != 0xFFFF && text_load(id, dst, LABEL_CAP) > 0) return;
    int n = 0; // TEMP fallback: ASCII straight into codes
    while(fallback[n] && n < LABEL_CAP - 1) {
        dst[n] = (tchar)fallback[n];
        n++;
    }
    dst[n] = 0;
}

bool scene_choice_open(SceneCtx* c, uint8_t* out) {
    if(c->n >= MAX_SCENES) return false;
    Choice* ch = &g_ch;
    ch->out = out;
    ch->sel = 0;
    load_label(TXT_GL_CHOICE_YES, ch->yes, "Si");
    load_label(TXT_GL_CHOICE_NO, ch->no, "No");
    const tchar* both[2] = {ch->yes, ch->no};
    text_prefetch_many(both, 2); // SD-backed (hanzi) fonts: make the glyphs resident now
    return scene_push(c, &scene_choice_ops, ch);
}

static void c_enter(SceneCtx* c, void* data) {
    Choice* ch = data;
    const TbGeom* g = tb_geom();
    int tw = font_text_width(ch->yes);
    int nw = font_text_width(ch->no);
    if(nw > tw) tw = nw;
    ch->w = BOX_PAD_X + font_cell_w() + CURSOR_GAP + tw + BOX_PAD_X + 1;
    ch->h = 2 * font_pitch() + 2 * TB_PAD_Y;
    ch->x = SCREEN_W - ch->w;
    ch->y = g->box_y - ch->h + 1; // its bottom frame row shares the text box's top row
    scene_dirty(c, D_COMPOSE);
}

static void finish(SceneCtx* c, Choice* ch, uint8_t yes) {
    sound_play(yes ? SND_FX_CONFIRM : SND_FX_CANCEL); // (contract 28)
    if(ch->out) *ch->out = yes;
    scene_pop(c);
    scene_wake(c, 1);
}

static void c_input(SceneCtx* c, void* data, const InputEv* ev) {
    Choice* ch = data;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT)
            finish(c, ch, 0); // Back = No
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        uint8_t ns = ev->key == KEY_UP ? 0 : 1;
        if(ns != ch->sel) {
            ch->sel = ns;
            scene_dirty(c, D_COMPOSE);
        }
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        finish(c, ch, ch->sel == 0);
    }
}

static void c_draw_overlay(SceneCtx* c, void* data) {
    (void)c;
    Choice* ch = data;
    const int pitch = font_pitch();
    ov_fill(ch->x, ch->y, ch->w, ch->h, 0);
    ov_frame(ch->x, ch->y, ch->w, ch->h, 1);
    const int tx = ch->x + BOX_PAD_X + font_cell_w() + CURSOR_GAP;
    for(int k = 0; k < 2; k++) {
        int ly = ch->y + TB_PAD_Y + k * pitch;
        text_draw_line(k == 0 ? ch->yes : ch->no, 0, tx, ly, 99);
        if(ch->sel == k) font_draw_glyph(GLYPH_CURSOR, ch->x + BOX_PAD_X, ly, 1);
    }
}

const SceneOps scene_choice_ops = {
    .enter = c_enter,
    .input = c_input,
    .draw_world = NULL,
    .draw_overlay = c_draw_overlay,
    .pauses_below = true,
};

#ifdef NUCLEO_PC // read-only view for the PC version's native painter (pc_views.h); not compiled on the Flipper
#include "pc_views.h"
bool scene_choice_pc_view(const void* state, PcChoiceView* out) {
    const Choice* ch = state;
    if(!ch) return false;
    out->yes = ch->yes;
    out->no = ch->no;
    out->sel = ch->sel;
    return true;
}
#endif
