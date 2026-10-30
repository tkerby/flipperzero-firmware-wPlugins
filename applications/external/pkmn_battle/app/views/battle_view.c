/* Battle status screen: link phase, both active mons with HP bars, status
 * and party, the Flipper's last move, counters; a long Back asks to quit. */
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>

#include "battle_view.h"

#define BAR_W   70
#define BALLS_X 95 /* first party ball's centre; the sixth ends at x 127 */

struct PkbBattleView {
    View* view;
    ViewDispatcher* dispatcher;
};

/* name row at baseline y, HP row below it */
static void draw_side(Canvas* canvas, uint8_t y, const char* who, const PkbBattleViewSide* s) {
    char line[16];

    canvas_draw_str(canvas, 0, y, who);
    canvas_draw_str(canvas, 16, y, s->name);
    /* ends 2px left of the first ball */
    canvas_draw_str_aligned(canvas, BALLS_X - 4, y, AlignRight, AlignBottom, s->status);
    for(uint8_t i = 0; i < s->count; i++) {
        uint8_t x = BALLS_X + 6 * i;
        if(s->alive & (1 << i))
            canvas_draw_disc(canvas, x, y - 3, 2);
        else
            canvas_draw_circle(canvas, x, y - 3, 2);
    }

    canvas_draw_frame(canvas, 0, y + 2, BAR_W, 5);
    if(s->max_hp) {
        uint32_t w = ((uint32_t)s->hp * (BAR_W - 2) + s->max_hp - 1) / s->max_hp;
        if(w > BAR_W - 2) w = BAR_W - 2;
        canvas_draw_box(canvas, 1, y + 3, w, 3);
    }
    snprintf(line, sizeof(line), "%u/%u", s->hp, s->max_hp);
    canvas_draw_str_aligned(canvas, 127, y + 8, AlignRight, AlignBottom, line);
}

static void draw_confirm(Canvas* canvas, const PkbBattleViewModel* m) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 6, 14, 116, 36);
    canvas_draw_box(canvas, 0, 51, 128, 13); /* under the buttons */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 6, 14, 116, 36, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Quit link battle?");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        64,
        36,
        AlignCenter,
        AlignBottom,
        m->battle_running ? "The Game Boy will hang" : "Leave the room on the GB");
    canvas_draw_str_aligned(
        canvas,
        64,
        45,
        AlignCenter,
        AlignBottom,
        m->battle_running ? "until it is reset." : "before you unplug.");
    elements_button_left(canvas, "Stay");
    elements_button_right(canvas, "Quit");
}

static void battle_view_draw(Canvas* canvas, void* model_ptr) {
    PkbBattleViewModel* m = model_ptr;
    char line[40];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Link Battle");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignBottom, m->phase);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->show_mons) {
        draw_side(canvas, 21, "Me", &m->me);
        draw_side(canvas, 39, "GB", &m->gb);
        canvas_draw_str(canvas, 0, 55, m->event);
        if(m->desync) {
            snprintf(line, sizeof(line), "! Desync risk: %s", m->desync);
        } else if(m->rematch) {
            snprintf(line, sizeof(line), "Console = rematch");
        } else {
            snprintf(
                line,
                sizeof(line),
                "Turn %lu  Battle %lu",
                (unsigned long)m->turns,
                (unsigned long)m->battles + 1);
        }
        canvas_draw_str(canvas, 0, 63, line);
    } else {
        if(canvas_string_width(canvas, m->status) <= 128) {
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, m->status);
        } else {
            /* "Opponent: Rival Feraligatr Lv100" is wider than the screen */
            FuriString* fit = furi_string_alloc_set(m->status);
            elements_string_fit_width(canvas, fit, 128);
            canvas_draw_str(canvas, 0, 30, furi_string_get_cstr(fit));
            furi_string_free(fit);
        }
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, m->hint);
        snprintf(line, sizeof(line), "%lu bytes   Hold Back: quit", (unsigned long)m->bytes);
        canvas_draw_str(canvas, 0, 63, line);
    }

    if(m->confirm_quit) draw_confirm(canvas, m);
}

static bool battle_view_input(InputEvent* event, void* context) {
    PkbBattleView* bv = context;
    bool confirm = false;
    with_view_model(bv->view, PkbBattleViewModel * m, { confirm = m->confirm_quit; }, false);

    if(confirm) {
        if(event->type != InputTypeShort) return true;
        if(event->key == InputKeyRight || event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(bv->dispatcher, PKB_BATTLE_VIEW_EVENT_ABORT);
        } else if(event->key == InputKeyLeft || event->key == InputKeyBack) {
            with_view_model(bv->view, PkbBattleViewModel * m, { m->confirm_quit = false; }, true);
        }
        return true;
    }
    if(event->key != InputKeyBack) return false;
    if(event->type == InputTypeLong) {
        with_view_model(bv->view, PkbBattleViewModel * m, { m->confirm_quit = true; }, true);
    }
    /* swallow short presses so a stray tap cannot strand the Game Boy */
    return true;
}

PkbBattleView* pkb_battle_view_alloc(ViewDispatcher* dispatcher) {
    PkbBattleView* bv = malloc(sizeof(PkbBattleView));
    bv->dispatcher = dispatcher;
    bv->view = view_alloc();
    view_allocate_model(bv->view, ViewModelTypeLocking, sizeof(PkbBattleViewModel));
    view_set_context(bv->view, bv);
    view_set_draw_callback(bv->view, battle_view_draw);
    view_set_input_callback(bv->view, battle_view_input);
    return bv;
}

void pkb_battle_view_free(PkbBattleView* bv) {
    view_free(bv->view);
    free(bv);
}

View* pkb_battle_view_get_view(PkbBattleView* bv) {
    return bv->view;
}

void pkb_battle_view_reset(PkbBattleView* bv) {
    with_view_model(bv->view, PkbBattleViewModel * m, { memset(m, 0, sizeof(*m)); }, false);
}

void pkb_battle_view_update(PkbBattleView* bv, const PkbBattleViewModel* model) {
    with_view_model(
        bv->view,
        PkbBattleViewModel * m,
        {
            bool confirm = m->confirm_quit;
            *m = *model;
            m->confirm_quit = confirm;
        },
        true);
}
