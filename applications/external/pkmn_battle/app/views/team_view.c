/* Team page: one mon with its sprite, level, max HP, held item and moves;
 * Left/Right pages, OK rerolls a random team. */
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>

#include "team_view.h"

#define TEXT_X 58 /* right of the sprite */

struct PkbTeamView {
    View* view;
    ViewDispatcher* dispatcher;
};

/* left-aligned at TEXT_X, moved left if it would run off the screen */
static void draw_right_str(Canvas* canvas, uint8_t y, const char* str) {
    uint16_t w = canvas_string_width(canvas, str);
    uint8_t x = w > 128 - TEXT_X ? 128 - w : TEXT_X;
    canvas_draw_str(canvas, x, y, str);
}

static void team_view_draw(Canvas* canvas, void* model_ptr) {
    PkbTeamViewModel* m = model_ptr;
    char line[16];

    canvas_clear(canvas);
    if(!m->count) return;

    if(m->has_sprite) {
        canvas_draw_xbm(canvas, 0, 0, PKB_SPRITE_SIZE, PKB_SPRITE_SIZE, m->sprite);
    } else {
        canvas_draw_rframe(canvas, 4, 4, PKB_SPRITE_SIZE - 8, PKB_SPRITE_SIZE - 8, 3);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 28, 28, AlignCenter, AlignCenter, "?");
    }

    canvas_set_font(canvas, FontPrimary);
    draw_right_str(canvas, 9, m->name);
    canvas_set_font(canvas, FontSecondary);
    snprintf(line, sizeof(line), "Lv%u  HP %u", m->level, m->max_hp);
    draw_right_str(canvas, 18, line);
    if(m->item[0]) {
        snprintf(line, sizeof(line), "@%s", m->item);
        draw_right_str(canvas, 27, line);
    }
    for(uint8_t k = 0; k < PKB_NUM_MOVES; k++) {
        draw_right_str(canvas, 36 + 9 * k, m->moves[k]);
    }

    if(m->can_reroll)
        snprintf(line, sizeof(line), "%u/%u OK:new", m->page + 1, m->count);
    else
        snprintf(line, sizeof(line), "< %u/%u >", m->page + 1, m->count);
    canvas_draw_str_aligned(canvas, 28, 64, AlignCenter, AlignBottom, line);
}

static bool team_view_input(InputEvent* event, void* context) {
    PkbTeamView* tv = context;
    uint32_t ev;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    switch(event->key) {
    case InputKeyLeft:
        ev = PKB_TEAM_VIEW_EVENT_PREV;
        break;
    case InputKeyRight:
        ev = PKB_TEAM_VIEW_EVENT_NEXT;
        break;
    case InputKeyOk:
        if(event->type != InputTypeShort) return true;
        /* the scene ignores it for boss teams */
        ev = PKB_TEAM_VIEW_EVENT_REROLL;
        break;
    default:
        return false;
    }
    view_dispatcher_send_custom_event(tv->dispatcher, ev);
    return true;
}

PkbTeamView* pkb_team_view_alloc(ViewDispatcher* dispatcher) {
    PkbTeamView* tv = malloc(sizeof(PkbTeamView));
    tv->dispatcher = dispatcher;
    tv->view = view_alloc();
    view_allocate_model(tv->view, ViewModelTypeLocking, sizeof(PkbTeamViewModel));
    view_set_context(tv->view, tv);
    view_set_draw_callback(tv->view, team_view_draw);
    view_set_input_callback(tv->view, team_view_input);
    return tv;
}

void pkb_team_view_free(PkbTeamView* tv) {
    view_free(tv->view);
    free(tv);
}

View* pkb_team_view_get_view(PkbTeamView* tv) {
    return tv->view;
}

void pkb_team_view_update(PkbTeamView* tv, const PkbTeamViewModel* model) {
    with_view_model(tv->view, PkbTeamViewModel * m, { *m = *model; }, true);
}
