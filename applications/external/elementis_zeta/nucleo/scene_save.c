// "Guardar" screen of the pause menu (contract section 12.3), the SAVE scene: info box, question, YES / NO, the write, the
// confirmation. Part of the UI plugin (contract 11.5, ui_host.h); the file and play-time code stay resident in save.c.
#include "save.h"
#include "ui_host.h"
#include "bag.h"
#include "party.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "gen/maps_gen.h"
#include <stdio.h>

// ---------------------------------------------------------------- "Guardar" screen
// Stack: WORLD / MENU / SAVE / DIALOG (question) / CHOICE (YES / NO); then the write, then DIALOG (done / fail).
enum { P_ASK, P_ANS, P_OVR, P_WRITE, P_DONE }; // P_OVR (contract 81): the file holds ANOTHER game: the overwrite question is open
#define SV_LAB_CAP 12
#define VAL_CAP 8
#define BOX_PAD 4
#define SAVING_CAP 40

typedef struct {
    uint8_t phase, ans, ok;
    SaveInfo old;                 // the file on the card (the overwrite test)
    int16_t x, w, h;
    tchar who[8];                 // "{player}"
    tchar lab_reg[SV_LAB_CAP], lab_time[SV_LAB_CAP];
    tchar val_reg[VAL_CAP], val_time[VAL_CAP];
    tchar saving[SAVING_CAP];     // "Guardando... / No apagues" (two lines), shown while the file is written
    int16_t saving_l1;
} SaveUi;

static SaveUi g_ui;

static int put_num(tchar* o, unsigned v, int pad2) {
    char t[8];
    int n = snprintf(t, sizeof(t), pad2 ? "%02u" : "%u", v);
    for(int i = 0; i < n; i++) o[i] = (tchar)t[i];
    o[n] = 0;
    return n;
}

static void format_time(tchar* o, uint32_t secs) {
    int n = put_num(o, secs / 3600, 0);
    o[n++] = ':';
    put_num(o + n, (secs / 60) % 60, 1);
}

bool scene_save_open(SceneCtx* c) {
    if(c->n + 2 >= MAX_SCENES) return false; // room for the question and the YES / NO box above this scene
    return scene_push(c, &scene_save_ops, &g_ui);
}

static void s_enter(SceneCtx* c, void* d) {
    SaveUi* u = d;
    ui_live_enter();
    scene_menu_set_hidden(true); // the save screen replaces the pause menu box
    u->phase = P_ASK;
    u->ans = 0;
    u->ok = 0;
    if(text_load(TXT_GL_SAVE_WHO, u->who, 8) < 0) u->who[0] = 0;
    if(text_load(TXT_GL_SAVE_REG, u->lab_reg, SV_LAB_CAP) < 0) u->lab_reg[0] = 0;
    if(text_load(TXT_GL_SAVE_TIME, u->lab_time, SV_LAB_CAP) < 0) u->lab_time[0] = 0;
    if(text_load(TXT_GL_SAVE_SAVING, u->saving, SAVING_CAP) < 0) u->saving[0] = 0;
    u->saving_l1 = (int16_t)text_line_start(u->saving, 0, 1);
    put_num(u->val_reg, (unsigned)reg_caught_count(), 0);
    format_time(u->val_time, save_time_secs());
    const tchar* all[5] = {u->who, u->lab_reg, u->lab_time, u->val_time, u->saving};
    text_prefetch_many(all, 5);
    // frame: the widest of the name row / "label  value" rows
    int wl = font_text_width(u->lab_reg), wt = font_text_width(u->lab_time);
    int wv = font_text_width(u->val_time), wd = font_text_width(u->val_reg);
    int row = (wl > wt ? wl : wt) + BOX_PAD + (wv > wd ? wv : wd);
    int wn = text_line_px(u->who, 0);
    u->w = (int16_t)(2 * BOX_PAD + (row > wn ? row : wn));
    u->h = (int16_t)(3 * font_pitch() + 2);
    u->x = 0; // top left: the YES / NO box (right) and the question (bottom) stay clear of it (zh: the last 2 px sit under the question)
    scene_dirty(c, D_COMPOSE);
    scene_wake(c, 1);
}

static void sv_finish(SceneCtx* c, bool close_menu) {
    scene_pop(c); // this scene
    if(close_menu) scene_pop(c); // the menu below
    scene_wake(c, 1);
}

static void s_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    SaveUi* u = d;
    switch(u->phase) {
    case P_ASK:
        u->phase = P_ANS;
        if(!scene_ask_open(c, TXT_GL_SAVE_ASK, &u->ans)) sv_finish(c, false);
        break;
    case P_ANS: // the question closed (P_OVR: the overwrite question closed)
    case P_OVR:
        if(!u->ans) {
            sv_finish(c, false);
        } else if(u->phase == P_ANS && save_peek(&u->old) && u->old.gid != party_state()->gid) { // "There is already a saved game": another game's file asks first (the same game's, an older build's with the same 0, and a missing or damaged file do not)
            u->phase = P_OVR;
            if(!scene_ask_open(c, TXT_GL_MISC_OVERWRITE, &u->ans)) sv_finish(c, false);
        } else {
            u->phase = P_WRITE;
            scene_dirty(c, D_COMPOSE); // "Guardando..." is drawn before the (slow) SD write
            scene_wake(c, 40);
        }
        break;
    case P_WRITE:
        u->ok = save_write(c->world);
        u->phase = P_DONE;
        if(!scene_dialog_open(c, u->ok ? TXT_GL_SAVE_DONE : TXT_GL_SAVE_FAIL, -1)) sv_finish(c, u->ok);
        else scene_dirty(c, D_COMPOSE);
        break;
    default: // the confirmation closed
        sv_finish(c, u->ok);
        break;
    }
}

static void s_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const SaveUi* u = d;
    const int pitch = font_pitch();
    ov_fill(u->x, 0, u->w, u->h, 0);
    ov_frame(u->x, 0, u->w, u->h, 1);
    text_draw_line(u->who, 0, u->x + BOX_PAD, 1, 99);
    const int right = u->x + u->w - BOX_PAD;
    font_draw_text(u->lab_reg, u->x + BOX_PAD, 1 + pitch);
    font_draw_text(u->val_reg, right - font_text_width(u->val_reg), 1 + pitch);
    font_draw_text(u->lab_time, u->x + BOX_PAD, 1 + 2 * pitch);
    font_draw_text(u->val_time, right - font_text_width(u->val_time), 1 + 2 * pitch);
    if(u->phase == P_WRITE) { // "Guardando..." in a text-box sized plate over the question's place
        const TbGeom* g = tb_geom();
        ov_fill(TB_X, g->box_y, TB_W, g->box_h, 0);
        ov_frame(TB_X, g->box_y, TB_W, g->box_h, 1);
        text_draw_line(u->saving, 0, g->text_x, g->line_y[0], 99);
        if(u->saving_l1 >= 0) text_draw_line(u->saving, u->saving_l1, g->text_x, g->line_y[1], 99);
    }
}

static void s_leave(SceneCtx* c, void* d) {
    (void)d;
    scene_menu_set_hidden(false);
    ui_live_leave();
    (void)c;
}

static void s_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c); // everything else waits for the dialogs above
}

const SceneOps scene_save_ops = {
    .enter = s_enter,
    .leave = s_leave,
    .input = s_input,
    .update = s_update,
    .draw_world = NULL,
    .draw_overlay = s_draw_overlay,
    .pauses_below = true,
};
