#include "fidget_view.h"

#include <furi.h>
#include <gui/icon_animation.h>
#include <gui/view.h>
#include <input/input.h>

extern const Icon A_fidgeting_128x64;

typedef struct {
    IconAnimation* anim;
    uint32_t frame;
} FidgetModel;

static void fidget_view_draw(Canvas* canvas, void* model) {
    FidgetModel* m = model;

    canvas_clear(canvas);

    if(m->anim) {
        canvas_draw_icon_animation(canvas, 0, 0, m->anim);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "REC");
}

static bool fidget_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void fidget_view_tick(View* view, uint32_t frame) {
    with_view_model(view, FidgetModel* m, {
        m->frame = frame;
    }, true);
}

void fidget_view_reset(View* view) {
    if(!view) return;

    IconAnimation* anim_to_restart = NULL;

    with_view_model(view, FidgetModel* m, {
        m->frame = 0;
        anim_to_restart = m->anim;
    }, true);

    if(anim_to_restart) {
        icon_animation_start(anim_to_restart);
    }
}

View* fidget_view_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FidgetModel));

    IconAnimation* anim = icon_animation_alloc(&A_fidgeting_128x64);
    view_tie_icon_animation(view, anim);
    icon_animation_start(anim);

    with_view_model(view, FidgetModel* m, {
        m->anim = anim;
    }, false);

    view_set_draw_callback(view, fidget_view_draw);
    view_set_input_callback(view, fidget_view_input);

    return view;
}

void fidget_view_free(View* view) {
    IconAnimation* anim = NULL;
    with_view_model(view, FidgetModel* m, {
        anim = m->anim;
        m->anim = NULL;
    }, false);

    if(anim) {
        icon_animation_stop(anim);
        icon_animation_free(anim);
    }

    view_free(view);
}
