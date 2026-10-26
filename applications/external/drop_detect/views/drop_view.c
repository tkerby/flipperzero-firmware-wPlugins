#include "drop_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/icon_animation.h>
#include <gui/view.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <notification/notification_messages.h>

static NotificationApp* drop_notif = NULL;

void drop_view_set_notification(NotificationApp* notif) {
    drop_notif = notif;
}

typedef enum {
    DropPhaseArming,
    DropPhaseCountdown,
    DropPhaseFalling,
    DropPhaseDone,
} DropPhase;

#define COUNTDOWN_FRAMES (4 * 30)
#define FALLING_FRAMES   (75)

extern const Icon A_drop_animation_128x64;

typedef struct {
    DropPhase phase;
    uint32_t phase_start_frame;
    uint32_t now_frame;
    IconAnimation* anim;
} DropModel;

/* ---------- DRAW ---------- */

static void drop_view_draw(Canvas* canvas, void* model) {
    DropModel* m = model;
    if(!m) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(m->phase) {
    case DropPhaseArming:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 20, 10, "DROP MODE");
        break;
    case DropPhaseCountdown:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 18, 10, "READY");
        break;
    case DropPhaseFalling:
        if(m->anim) {
            canvas_draw_icon_animation(canvas, 0, 0, m->anim);
        }
        break;
    case DropPhaseDone:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 25, 20, "DONE");
        break;
    }
}

/* ---------- INPUT ---------- */

static bool drop_view_input(InputEvent* event, void* context) {
    View* view = context;

    if(event->type != InputTypeShort && event->type != InputTypePress) {
        return false;
    }
    if(event->key != InputKeyOk) {
        return false;
    }

    with_view_model(view, DropModel* m, {
        if(m->phase == DropPhaseArming) {
            m->phase = DropPhaseCountdown;
            m->phase_start_frame = m->now_frame;
        } else if(m->phase == DropPhaseDone) {
            m->phase = DropPhaseArming;
            m->phase_start_frame = m->now_frame;
        }
    }, true);

    return true;
}

/* ---------- TICK ---------- */

void drop_view_tick(View* view, uint32_t frame) {
    if(!view) return;

    IconAnimation* restart_anim = NULL;

    with_view_model(view, DropModel* m, {
        m->now_frame = frame;
        uint32_t local = frame - m->phase_start_frame;

        if(m->phase == DropPhaseCountdown && local >= COUNTDOWN_FRAMES) {
            m->phase = DropPhaseFalling;
            m->phase_start_frame = frame;
            restart_anim = m->anim;
        } else if(m->phase == DropPhaseFalling && local >= FALLING_FRAMES) {
            m->phase = DropPhaseDone;
        }
    }, true);

    if(restart_anim) {
        icon_animation_start(restart_anim);
    }
}

void drop_view_reset(View* view) {
    if(!view) return;

    IconAnimation* anim_to_restart = NULL;

    with_view_model(view, DropModel* m, {
        m->phase = DropPhaseArming;
        m->phase_start_frame = 0;
        m->now_frame = 0;
        anim_to_restart = m->anim;
    }, true);

    if(anim_to_restart) {
        icon_animation_start(anim_to_restart);
    }
}

/* ---------- ALLOC ---------- */

View* drop_view_alloc(void) {
    View* view = view_alloc();
    if(!view) return NULL;

    view_allocate_model(view, ViewModelTypeLocking, sizeof(DropModel));
    view_set_context(view, view);

    IconAnimation* anim = icon_animation_alloc(&A_drop_animation_128x64);
    view_tie_icon_animation(view, anim);
    icon_animation_start(anim);

    with_view_model(view, DropModel* m, {
        m->anim = anim;
    }, false);

    view_set_draw_callback(view, drop_view_draw);
    view_set_input_callback(view, drop_view_input);

    return view;
}

void drop_view_free(View* view) {
    if(!view) return;

    IconAnimation* anim = NULL;
    with_view_model(view, DropModel* m, {
        anim = m->anim;
        m->anim = NULL;
    }, false);

    if(anim) {
        icon_animation_stop(anim);
        icon_animation_free(anim);
    }

    view_free(view);
}
