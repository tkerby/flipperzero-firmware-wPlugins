#include "idle_view.h"

#include <furi.h>
#include <gui/icon_animation.h>
#include <gui/view.h>
#include <input/input.h>

extern const Icon A_idle_128x64;

typedef struct {
    IconAnimation* anim;
    uint32_t frame;
} IdleModel;

static void idle_view_draw(Canvas* canvas, void* model) {
    IdleModel* m = model;

    canvas_clear(canvas);

    if(m->anim) {
        canvas_draw_icon_animation(canvas, 0, 0, m->anim);
    }

    canvas_set_font(canvas, FontSecondary);

    char buf[24];
    uint32_t sec = m->frame / 30;

    snprintf(buf, sizeof(buf), "REC %02lu:%02lu",
             (unsigned long)(sec / 60),
             (unsigned long)(sec % 60));

    canvas_draw_str(canvas, 4, 8, buf);
}

static bool idle_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void idle_view_tick(View* view, uint32_t frame) {
    with_view_model(view, IdleModel* m, {
        m->frame = frame;
    }, true);
}

void idle_view_reset(View* view) {
    if(!view) return;

    IconAnimation* anim_to_restart = NULL;

    with_view_model(view, IdleModel* m, {
        m->frame = 0;
        anim_to_restart = m->anim;
    }, true);

    if(anim_to_restart) {
        icon_animation_start(anim_to_restart);
    }
}

View* idle_view_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(IdleModel));

    IconAnimation* anim = icon_animation_alloc(&A_idle_128x64);
    view_tie_icon_animation(view, anim);
    icon_animation_start(anim);

    with_view_model(view, IdleModel* m, {
        m->anim = anim;
    }, false);

    view_set_draw_callback(view, idle_view_draw);
    view_set_input_callback(view, idle_view_input);

    return view;
}

void idle_view_free(View* view) {
    IconAnimation* anim = NULL;
    with_view_model(view, IdleModel* m, {
        anim = m->anim;
        m->anim = NULL;
    }, false);

    if(anim) {
        icon_animation_stop(anim);
        icon_animation_free(anim);
    }

    view_free(view);
}
