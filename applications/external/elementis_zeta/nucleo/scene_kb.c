// Naming keyboard and nickname prompt (task K, contract 11.6). Part of the UI plugin (ui_host.h).
//
//  * kb.bin (names.h; mkfont.py, assets/lang/keyboard.toml): per language the grid pages as font codes, the non-ASCII characters of the
//    names as (code, Unicode) pairs and the name / nickname limits. The whole file (<= 1.2 KB) is read into one heap block while a keyboard is on.
//  * scene_kb_open: the keyboard. D-pad moves,
//    OK types, Back deletes (on an empty line: cancel), long OK ends. The result is stored in PartyState (party_name_set /
//    party_nick_set), as Unicode.
//  * scene_nick_open(): "Do you want to give a nickname to X?" YES / NO, then the keyboard for that party slot.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "assets.h"
#include "party.h"
#include "ui_host.h"
#include "names.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "gen/langs_gen.h"

_Static_assert(TXT_GL_KB_PG3 == TXT_GL_KB_PG0 + 3, "gl.kb.pg0..pg3 must be consecutive (es.txt)");

#define KB_LAB_CAP 8
#define PFX_CAP 96
#define BG_LEVEL 1

// ---------------------------------------------------------------- the keyboard scene
typedef struct {
    Kb kb;
    uint8_t kind, slot;     // 0 player, 1 rival, 2 nickname of party slot `slot`
    uint8_t len, max;       // typed characters / limit
    uint8_t page, vrow, col, ci; // page, cursor row (visual order, the command row included), grid column, command (0 page, 1 end)
    bool press_typed;       // the last OK press typed a character (a long press then removes it and ends)
    int16_t curw, px, py, head, gx, gy; // cursor width, key pitch x / y, height of the first line, grid origin
    int16_t sx, kw;         // name slots: x of the first slot, slot pitch
    int16_t cmd_x[2], cmd_y;
    tchar txt[PARTY_NICK_CAP];
    uint16_t uni[PARTY_NICK_CAP];
    tchar lab_end[KB_LAB_CAP], lab_pg[KB_MAX_PAGES][KB_LAB_CAP], prompt[16];
    tchar pfx[PFX_CAP]; // glyphs to keep resident (SD-backed fonts): the page, the typed characters, the labels
} KbSc;

static bool g_done; // the last keyboard ended with a stored name

bool scene_kb_done(void) {
    return g_done;
}

static inline bool cmd_row(const KbSc* s, uint8_t vrow) {
    return vrow == (s->kb.top ? 0 : s->kb.rows);
}
static inline uint8_t grid_row(const KbSc* s, uint8_t vrow) {
    return s->kb.top ? (uint8_t)(vrow - 1) : vrow;
}
static inline uint8_t n_vrows(const KbSc* s) {
    return (uint8_t)(s->kb.rows + 1);
}

static void prefetch(KbSc* s) {
    int n = 0;
    for(uint8_t r = 0; r < s->kb.rows; r++)
        for(uint8_t c = 0; c < s->kb.cols; c++) {
            tchar t = kb_cell(&s->kb, s->page, r, c);
            if(t >= 0x20 && n < PFX_CAP - 1) s->pfx[n++] = t;
        }
    for(uint8_t i = 0; i < s->len && n < PFX_CAP - 1; i++) s->pfx[n++] = s->txt[i];
    if(n < PFX_CAP - 1) s->pfx[n++] = '_';
    const tchar* lab[2] = {s->lab_pg[(s->page + 1) % s->kb.pages], s->lab_end}; // the labels of the two command keys
    for(int k = 0; k < 2; k++)
        for(int i = 0; lab[k][i] && n < PFX_CAP - 1; i++) s->pfx[n++] = lab[k][i];
    s->pfx[n] = 0;
    text_prefetch(s->pfx);
}

static void layout(KbSc* s) {
    const int cw = font_cell_w();
    s->head = (int16_t)font_pitch();
    if(cw <= 5) {         // Latin 5 x 11 cells: 6 px cursor + 6 px key; rows 10 px apart (the cell is 11 high, the grid is not)
        s->curw = 6;
        s->px = 12;
        s->py = 10;
    } else if(cw <= 8) {  // kana: the cursor's ink is 4 px wide
        s->curw = 4;
        s->px = 12;
        s->py = (int16_t)font_pitch();
    } else {              // hanzi: 5 px cursor + 11 px of ink
        s->curw = 5;
        s->px = 16;
        s->py = (int16_t)font_pitch();
    }
    s->gx = (int16_t)((SCREEN_W - s->kb.cols * s->px) / 2);
    s->gy = s->head;
    s->kw = (int16_t)(cw <= 5 ? 6 : cw);
    const int wp = font_text_width(s->lab_pg[(s->page + 1) % s->kb.pages]), we = font_text_width(s->lab_end);
    if(s->kb.top) { // commands on the first line, right-aligned; the slots on its left
        s->sx = 2;
        s->cmd_y = 0;
        s->cmd_x[1] = (int16_t)(SCREEN_W - 2 - we);
        s->cmd_x[0] = (int16_t)(s->cmd_x[1] - s->curw - 6 - wp);
    } else {        // a last row under the grid; the prompt and the slots on the first line
        s->sx = (int16_t)(SCREEN_W - 3 - s->max * s->kw);
        s->cmd_y = (int16_t)(s->gy + s->kb.rows * s->py);
        s->cmd_x[0] = (int16_t)(s->gx + s->curw);
        s->cmd_x[1] = (int16_t)(SCREEN_W - 4 - we);
    }
}

bool scene_kb_open(SceneCtx* c, uint8_t kind, uint8_t slot) {
    if(c->n >= MAX_SCENES || !heap_can_alloc(sizeof(KbSc))) return false;
    KbSc* s = malloc(sizeof(KbSc));
    if(!s) return false;
    memset(s, 0, sizeof(*s));
    if(!kb_load(&s->kb, text_language())) {
        free(s);
        return false;
    }
    s->kind = kind;
    s->slot = slot;
    s->max = kind == 2 ? s->kb.max_nick : s->kb.max_name;
    const tchar* all[KB_MAX_PAGES + 2];
    int n = 0;
    for(int i = 0; i < KB_MAX_PAGES; i++) {
        if(text_load((uint16_t)(TXT_GL_KB_PG0 + i), s->lab_pg[i], KB_LAB_CAP) < 0) s->lab_pg[i][0] = 0;
        all[n++] = s->lab_pg[i];
    }
    if(text_load(TXT_GL_KB_END, s->lab_end, KB_LAB_CAP) < 0) s->lab_end[0] = 0;
    all[n++] = s->lab_end;
    if(text_load((uint16_t)(kind == 2 ? TXT_GL_KB_T_NICK : kind == 1 ? TXT_GL_KB_T_RIVAL : TXT_GL_KB_T_PLAYER), s->prompt, 16) < 0) s->prompt[0] = 0;
    all[n++] = s->prompt;
    text_prefetch_many(all, n);
    s->vrow = s->kb.top ? 1 : 0;
    if(!scene_push(c, &scene_kb_ops, s)) {
        kb_free(&s->kb);
        free(s);
        return false;
    }
    return true;
}

static void k_enter(SceneCtx* c, void* d) {
    KbSc* s = d;
    ui_live_enter();
    g_done = false;
    layout(s);
    prefetch(s);
    scene_dirty(c, D_WORLD);
}

static void k_leave(SceneCtx* c, void* d) {
    (void)c;
    KbSc* s = d;
    kb_free(&s->kb);
    free(s);
    ui_live_leave();
}

static void kb_finish(SceneCtx* c, KbSc* s, bool ok) {
    if(ok) { // store before the scene goes (the plugin must still be live for the table lookups of party_apply_names)
        s->uni[s->len] = 0;
        if(s->kind == 2) {
            if(s->slot == NICK_PENDING) party_pending_nick_set(s->uni); // M5-B: the creature waiting for the box (contract 24)
            else party_nick_set(s->slot, s->uni);
        } else party_name_set(s->kind == 1, s->uni);
    }
    g_done = ok;
    scene_pop(c);
    scene_wake(c, 1);
}

// The cell under the cursor (a grid cell): its code, 0 = empty.
static tchar cur_cell(const KbSc* s) {
    return kb_cell(&s->kb, s->page, grid_row(s, s->vrow), s->col);
}

static bool row_has(const KbSc* s, uint8_t vrow, uint8_t col) {
    return cmd_row(s, vrow) || kb_cell(&s->kb, s->page, grid_row(s, vrow), col) != 0;
}

static void move_v(KbSc* s, int dir) {
    const uint8_t n = n_vrows(s);
    uint8_t r = s->vrow;
    for(uint8_t i = 0; i < n; i++) {
        r = (uint8_t)((r + (dir > 0 ? 1 : n - 1)) % n);
        if(row_has(s, r, s->col)) break;
    }
    if(cmd_row(s, r) && !cmd_row(s, s->vrow)) s->ci = (uint8_t)(s->col * 2 / s->kb.cols);
    s->vrow = r;
}

static void move_h(KbSc* s, int dir) {
    if(cmd_row(s, s->vrow)) {
        s->ci ^= 1;
        return;
    }
    uint8_t c = s->col;
    for(uint8_t i = 0; i < s->kb.cols; i++) {
        c = (uint8_t)((c + (dir > 0 ? 1 : s->kb.cols - 1)) % s->kb.cols);
        if(row_has(s, s->vrow, c)) break;
    }
    s->col = c;
}

static void type_cell(KbSc* s) {
    tchar t = cur_cell(s);
    s->press_typed = false;
    if(!t || s->len >= s->max) return;
    s->txt[s->len] = t;
    s->uni[s->len] = kb_c2u(&s->kb, t);
    s->len++;
    s->txt[s->len] = 0;
    s->press_typed = true;
}

static void next_page(KbSc* s) {
    s->page = (uint8_t)((s->page + 1) % s->kb.pages);
    layout(s);
    prefetch(s);
    // keep the cursor on a key: the same cell, else the first one of the row
    if(!cmd_row(s, s->vrow) && !cur_cell(s)) move_h(s, 1);
}

static void k_input(SceneCtx* c, void* d, const InputEv* ev) {
    KbSc* s = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) {
            scene_clear(c);
        } else if(ev->type == EV_SHORT) {
            if(s->len == 0) kb_finish(c, s, false); // nothing to delete: leave the keyboard
            else {
                s->len--;
                s->txt[s->len] = 0;
                prefetch(s);
                scene_dirty(c, D_COMPOSE);
            }
        }
        return;
    }
    if(ev->key == KEY_OK) {
        if(ev->type == EV_PRESS) {
            if(cmd_row(s, s->vrow)) {
                s->press_typed = false;
                if(s->ci == 0) {
                    next_page(s);
                    scene_dirty(c, D_COMPOSE);
                } else if(s->len > 0 || s->kind == 2) {
                    kb_finish(c, s, true);
                }
            } else {
                type_cell(s);
                prefetch(s);
                scene_dirty(c, D_COMPOSE);
            }
        } else if(ev->type == EV_LONG) { // long OK = the end key, wherever the cursor is (the press that started it typed one more character: take it back)
            if(s->press_typed && s->len) s->len--;
            if(s->len > 0 || s->kind == 2) kb_finish(c, s, true);
        }
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) move_v(s, ev->key == KEY_DOWN ? 1 : -1);
    else if(ev->key == KEY_LEFT || ev->key == KEY_RIGHT) move_h(s, ev->key == KEY_RIGHT ? 1 : -1);
    else return;
    scene_dirty(c, D_COMPOSE);
}

static void k_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, BG_LEVEL);
}

static void draw_space(const KbSc* s, int x, int y) { // the space key: an open box under the baseline
    const int cw = font_cell_w(), w = cw >= 12 ? 8 : (cw >= 8 ? 6 : 5), b = y + font_baseline() - 1;
    ov_fill(x, b - 3, 1, 4, 1);
    ov_fill(x + w - 1, b - 3, 1, 4, 1);
    ov_fill(x, b, w, 1, 1);
    (void)s;
}

static void k_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    KbSc* s = d;
    // the name slots: typed characters, then "_"
    for(int i = 0; i < s->max; i++) {
        const int x = s->sx + i * s->kw;
        if(i < s->len) {
            if(s->txt[i] == ' ') draw_space(s, x, 0);
            else font_draw_glyph(s->txt[i], x, 0, 1);
        } else {
            font_draw_glyph('_', x, 0, 1);
        }
    }
    if(!s->kb.top && s->prompt[0] && 2 + font_text_width(s->prompt) + 4 <= s->sx) font_draw_text(s->prompt, 2, 0);
    // the grid
    for(uint8_t r = 0; r < s->kb.rows; r++)
        for(uint8_t k = 0; k < s->kb.cols; k++) {
            tchar t = kb_cell(&s->kb, s->page, r, k);
            if(!t) continue;
            const int x = s->gx + k * s->px + s->curw, y = s->gy + r * s->py;
            if(t == ' ') draw_space(s, x, y);
            else font_draw_glyph(t, x, y, 1);
        }
    // the commands: the label of the page the page key leads to, and the end key
    const tchar* lp = s->lab_pg[(s->page + 1) % s->kb.pages];
    font_draw_text(lp, s->cmd_x[0], s->cmd_y);
    font_draw_text(s->lab_end, s->cmd_x[1], s->cmd_y);
    // the cursor
    if(cmd_row(s, s->vrow)) font_draw_glyph(GLYPH_CURSOR, s->cmd_x[s->ci] - s->curw, s->cmd_y, 1);
    else font_draw_glyph(GLYPH_CURSOR, s->gx + s->col * s->px, s->gy + grid_row(s, s->vrow) * s->py, 1);
}

#ifndef NUCLEO_DEVICE
bool scene_kb_probe(const SceneCtx* c, int* page, int* vrow, int* col, int* ci, int* len, const tchar** txt) {
    if(scene_top_ops(c) != &scene_kb_ops) return false;
    const KbSc* s = c->st[c->n - 1].data;
    *page = s->page;
    *vrow = s->vrow;
    *col = s->col;
    *ci = s->ci;
    *len = s->len;
    *txt = s->txt;
    return true;
}

void scene_kb_goto(SceneCtx* c, int page, int grow, int col) {
    if(scene_top_ops(c) != &scene_kb_ops) return;
    KbSc* s = c->st[c->n - 1].data;
    if(page != s->page) {
        s->page = (uint8_t)page;
        layout(s);
        prefetch(s);
    }
    if(grow < 0) {
        s->vrow = s->kb.top ? 0 : s->kb.rows;
        s->ci = (uint8_t)col;
    } else {
        s->vrow = (uint8_t)(s->kb.top ? grow + 1 : grow);
        s->col = (uint8_t)col;
    }
    scene_dirty(c, D_COMPOSE);
}
#endif

const SceneOps scene_kb_ops = {
    .enter = k_enter,
    .leave = k_leave,
    .input = k_input,
    .update = NULL,
    .draw_world = k_draw_world,
    .draw_overlay = k_draw_overlay,
    .pauses_below = true,
};

// ---------------------------------------------------------------- nickname prompt
// Stack: WORLD / NICK / DIALOG (question) / CHOICE (YES / NO), then WORLD / NICK / KB. Draws nothing itself (the world shows through).
enum { NP_ASK, NP_ANS, NP_KB, NP_DONE };
typedef struct {
    uint8_t slot, phase, ans;
} NickSc;
static NickSc g_nk;

static void set_v0(uint8_t slot) {
    tchar nm[TEXT_VAR_LEN];
    if(slot == NICK_PENDING) { // M5-B: the pending creature has no party slot: its species name (the box message shows the nickname)
        Mon m;
        if(!party_pending_get(&m) || !text_set_var_id(0, reg_sp_txt(m.species))) text_set_var(0, NULL);
        return;
    }
    party_mon_name(slot, nm, TEXT_VAR_LEN);
    text_set_var(0, nm);
}

bool scene_nick_open(SceneCtx* c, uint8_t slot) {
    if(c->n + 3 >= MAX_SCENES) return false; // room for the question and the YES / NO box above this scene
    g_nk.slot = slot;
    g_nk.phase = NP_ASK;
    g_nk.ans = 0;
    return scene_push(c, &scene_nick_ops, &g_nk);
}

static void nk_enter(SceneCtx* c, void* d) {
    (void)d;
    ui_live_enter();
    scene_wake(c, 1);
}

static void nk_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    ui_live_leave();
}

static void nk_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    NickSc* n = d;
    switch(n->phase) {
    case NP_ASK:
        set_v0(n->slot);
        n->phase = NP_ANS;
        if(!scene_ask_open(c, TXT_GL_KB_ASK_NICK, &n->ans)) n->phase = NP_DONE;
        break;
    case NP_ANS: // the question closed
        n->phase = n->ans && scene_kb_open(c, 2, n->slot) ? NP_KB : NP_DONE;
        break;
    case NP_KB: // the keyboard closed
        n->phase = NP_DONE;
        break;
    default:
        break;
    }
    if(n->phase == NP_DONE) {
        set_v0(n->slot); // the final display name stays in {v0} for the script / dialogs that follow
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static void nk_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
}

const SceneOps scene_nick_ops = {
    .enter = nk_enter,
    .leave = nk_leave,
    .input = nk_input,
    .update = nk_update,
    .draw_world = NULL,
    .draw_overlay = NULL,
    .pauses_below = true,
};
