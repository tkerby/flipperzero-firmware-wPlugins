#include "../lib/bunnyconnect_views.h"
#include <furi.h>
#include <gui/view.h>
#include <gui/gui.h>
#include <input/input.h>

struct BunnyConnectCustomView {
    View* view;
    BunnyConnectViewCallback callback;
    void* context;
};

typedef struct {
    int selected_item;
    int item_count;
} BunnyConnectViewModel;

static void bunnyconnect_view_draw_callback(Canvas* canvas, void* model) {
    BunnyConnectViewModel* view_model = model;
    UNUSED(view_model);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 15, "BunnyConnect");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 30, "Virtual Keyboard");
    canvas_draw_str(canvas, 2, 45, "Text Input");
    canvas_draw_str(canvas, 2, 60, "Exit");
}

static bool bunnyconnect_view_input_callback(InputEvent* event, void* context) {
    BunnyConnectCustomView* view = context;
    bool consumed = false;

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyOk:
            if(view->callback) {
                view->callback(view->context);
            }
            consumed = true;
            break;
        case InputKeyBack:
            if(view->callback) {
                view->callback(view->context);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

BunnyConnectCustomView* bunnyconnect_view_alloc(void) {
    BunnyConnectCustomView* view = malloc(sizeof(BunnyConnectCustomView));

    view->view = view_alloc();
    view->callback = NULL;
    view->context = NULL;

    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(BunnyConnectViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, bunnyconnect_view_draw_callback);
    view_set_input_callback(view->view, bunnyconnect_view_input_callback);

    return view;
}

void bunnyconnect_view_free(BunnyConnectCustomView* view) {
    furi_assert(view);

    view_free(view->view);
    free(view);
}

View* bunnyconnect_view_get_view(BunnyConnectCustomView* view) {
    furi_assert(view);
    return view->view;
}

void bunnyconnect_view_set_callback(
    BunnyConnectCustomView* view,
    BunnyConnectViewCallback callback,
    void* context) {
    furi_assert(view);

    view->callback = callback;
    view->context = context;
}
