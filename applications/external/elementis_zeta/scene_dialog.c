// DIALOG scene: text box (typewriter, OK fills the page / advances, blinking more-arrow).
#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-inline,no-gcse,no-reorder-blocks,no-tree-sink") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "assets.h"
#include <stdlib.h>

#define ARROW_BLINK_MS 400
#define TYPE_MAX_DT    100 // ms: longest step the typewriter accounts for in one update

typedef struct {
    // contract 77: fields ordered by how often the code touches them (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access, above it a 32-bit one): hot small fields first, big blocks last
    int page; // index of the current page's first code
    int l0, l1; // line starts of the current page (l1 = -1: one line)
    int n0, total; // glyphs in line 0 / in the page
    int shown; // glyphs revealed so far
    bool more; // another page follows
    bool arrow; // blink phase of the more-arrow
    uint32_t acc; // blink accumulator
    uint32_t tacc; // typewriter accumulator (ms since the last glyph)
    int8_t push_back; // facing for the forced step on close, -1 none
    uint8_t*
        ask_out; // scripts (ASK): YES/NO box over the last page, result stored here; NULL = plain text
    bool asked; // the YES/NO box was pushed: when this dialog updates again it has closed -> close too
    tchar buf[TB_BUF];
} Dialog;

#ifndef NUCLEO_DEVICE
const tchar* scene_dialog_text(
    const void* state) { // host tests (tools/host_world/*_check.c): the text of a dialog scene
    return ((const Dialog*)state)->buf;
}
#endif

// The dialog state (0.45 KB) lives on the heap only while a dialog is open (freed by leave), not in .bss: contract 5, Memory.

static void page_set(Dialog* d, int page) {
    d->page = page;
    d->l0 = page;
    d->l1 = text_line_start(d->buf, page, 1);
    d->n0 = text_line_glyphs(d->buf, d->l0);
    d->total = d->n0 + (d->l1 >= 0 ? text_line_glyphs(d->buf, d->l1) : 0);
    d->shown = 0;
    d->more = text_next_page(d->buf, page) >= 0;
    d->arrow = true;
    d->acc = 0;
    d->tacc = 0;
    text_prefetch_page(d->buf, page); // SD-backed fonts: make the page's glyphs resident now
}

static void close_dialog(SceneCtx* c, Dialog* d) {
    int8_t pb = d->push_back;
    scene_pop(c);
    scene_wake(c, 1);
    if(pb >= 0) world_start_step(c->world, (uint8_t)pb, true);
}

bool scene_dialog_open(SceneCtx* c, uint16_t text_id, int8_t push_back) {
    if(c->n >= MAX_SCENES) return false;
    if(!heap_can_alloc(sizeof(Dialog))) return false;
    Dialog* d = malloc(sizeof(Dialog));
    if(!d) return false;
    if(text_load(text_id, d->buf, TB_BUF) < 0) {
        free(d);
        return false;
    }
    d->push_back = push_back;
    d->ask_out = NULL;
    d->asked = false;
    page_set(d, 0);
    if(!scene_push(c, &scene_dialog_ops, d)) {
        free(d);
        return false;
    }
    return true;
}

bool scene_ask_open(SceneCtx* c, uint16_t text_id, uint8_t* out) {
    if(c->n + 1 >= MAX_SCENES) return false; // room for the choice box above the dialog
    if(!scene_dialog_open(c, text_id, -1)) return false;
    ((Dialog*)c->st[c->n - 1].data)->ask_out = out; // the dialog just pushed
    return true;
}

// Last page fully shown in an ask dialog: put the YES/NO box on top (once).
static __attribute__((always_inline)) inline void maybe_ask(SceneCtx* c, Dialog* d) {
    if(!d->ask_out || d->asked) return;
    d->asked = true;
    if(!scene_choice_open(c, d->ask_out)) {
        *d->ask_out = 0;
        close_dialog(c, d);
    }
}

static void d_enter(SceneCtx* c, void* data) {
    (void)data;
    scene_dirty(c, D_COMPOSE);
    scene_wake(c, 1);
}

static void d_leave(SceneCtx* c, void* data) {
    (void)c;
    free(data);
}

static void d_input(SceneCtx* c, void* data, const InputEv* ev) {
    Dialog* d = data;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT) {
            if(d->ask_out && !d->asked)
                *d->ask_out = 0; // Back before the question is complete = No
            close_dialog(c, d);
        }
        return;
    }
    if(ev->key != KEY_OK || ev->type != EV_PRESS) return;
    if(d->shown < d->total) {
        d->shown = d->total; // fill the page instantly
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, ARROW_BLINK_MS);
    } else if(d->more) {
        page_set(d, text_next_page(d->buf, d->page));
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, 1);
    } else {
        close_dialog(c, d);
    }
}

static void d_update(SceneCtx* c, void* data, uint32_t dt) {
    Dialog* d = data;
    if(d->asked) { // the YES/NO box above us closed: so do we
        close_dialog(c, d);
        return;
    }
    if(d->shown < d->total) {
        // time-based typewriter: the active language sets the ms per glyph (text_glyph_ms()); a slow
        // frame reveals several glyphs at once instead of slowing the text down
        uint32_t ms = text_glyph_ms();
        d->tacc += dt < TYPE_MAX_DT ? dt : TYPE_MAX_DT;
        int n = ms ? (int)(d->tacc / ms) : d->total;
        if(n > 0) {
            d->tacc = ms ? d->tacc % ms : 0;
            d->shown = n < d->total - d->shown ? d->shown + n : d->total;
            scene_dirty(c, D_COMPOSE);
        }
        if(d->shown < d->total) {
            scene_wake(c, ms > d->tacc ? ms - d->tacc : 1);
            return;
        }
        d->acc = 0; // typed: start the arrow blink from "visible"
        d->arrow = true;
        if(!d->more) {
            maybe_ask(c, d);
            return;
        }
        scene_wake(c, ARROW_BLINK_MS);
        return;
    }
    if(!d->more) { // static page, nothing to animate
        maybe_ask(c, d);
        return;
    }
    d->acc += dt;
    if(d->acc >= ARROW_BLINK_MS) {
        d->acc %= ARROW_BLINK_MS;
        d->arrow = !d->arrow;
        scene_dirty(c, D_COMPOSE);
    }
    scene_wake(c, ARROW_BLINK_MS - d->acc);
}

static void d_draw_overlay(SceneCtx* c, void* data) {
    (void)c;
    Dialog* d = data;
    const TbGeom* g = tb_geom();
    ov_fill(TB_X, g->box_y, TB_W, g->box_h, 0);
    ov_frame(TB_X, g->box_y, TB_W, g->box_h, 1);
    int s0 = d->shown < d->n0 ? d->shown : d->n0;
    text_draw_line(d->buf, d->l0, g->text_x, g->line_y[0], s0);
    if(d->l1 >= 0 && d->shown > d->n0)
        text_draw_line(d->buf, d->l1, g->text_x, g->line_y[1], d->shown - d->n0);
    if(d->more && d->shown >= d->total && d->arrow)
        font_draw_glyph(GLYPH_MORE, g->more_x, g->more_y, 1);
}

const SceneOps scene_dialog_ops = {
    .enter = d_enter,
    .leave = d_leave,
    .input = d_input,
    .update = d_update,
    .draw_world = NULL,
    .draw_overlay = d_draw_overlay,
    .pauses_below = true,
};

#ifdef NUCLEO_PC // read-only view for the PC version's native painter (pc_views.h); not compiled on the Flipper
#include "pc_views.h"
bool scene_dialog_pc_view(const void* state, PcDialogView* out) {
    const Dialog* d = state;
    if(!d) return false;
    out->buf = d->buf;
    out->l0 = d->l0;
    out->l1 = d->l1;
    out->n0 = d->n0;
    out->total = d->total;
    out->shown = d->shown;
    out->more = d->more;
    out->arrow = d->arrow;
    out->asking = d->ask_out != NULL;
    return true;
}
#endif
