#include "view_menu.h"
#include "ghost_icon.h"

#include <furi.h>
#include <furi_hal.h>

#define MENU_ITEM_COUNT 4

static const char* kMenuItems[MENU_ITEM_COUNT] = {
    "EMF Meter",
    "Spirit Box",
    "REM Pod",
    "About",
};

typedef struct {
    int selected_index;
} MenuViewModel;

struct MenuView {
    View* view;
    MenuSelectCallback select_callback;
    void* select_callback_context;
};

static void menu_view_draw_callback(Canvas* canvas, void* model) {
    MenuViewModel* m = model;
    canvas_clear(canvas);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, w / 2, 1, AlignCenter, AlignTop, "GHOST");
    canvas_draw_str_aligned(canvas, w / 2, 13, AlignCenter, AlignTop, "TRACKER");

    canvas_set_font(canvas, FontSecondary);
    const int item_top = 30;
    const int item_h = 14;
    for(int i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = item_top + i * item_h;
        bool selected = (i == m->selected_index);
        if(selected) {
            canvas_draw_box(canvas, 4, y, (int)w - 8, item_h - 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(
            canvas, w / 2, y + (item_h - 2) / 2, AlignCenter, AlignCenter, kMenuItems[i]);
        if(selected) canvas_set_color(canvas, ColorBlack);
    }

    ghost_icon_draw(canvas, (int)w / 2, (int)h - 32, 24, 26);
}

static bool menu_view_input_callback(InputEvent* event, void* context) {
    MenuView* instance = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyUp) {
        with_view_model(
            instance->view,
            MenuViewModel * model,
            {
                model->selected_index =
                    (model->selected_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
            },
            true);
        return true;
    }

    if(event->key == InputKeyDown) {
        with_view_model(
            instance->view,
            MenuViewModel * model,
            { model->selected_index = (model->selected_index + 1) % MENU_ITEM_COUNT; },
            true);
        return true;
    }

    if(event->key == InputKeyOk) {
        int index;
        with_view_model(instance->view, MenuViewModel * model, { index = model->selected_index; }, false);
        if(instance->select_callback) {
            instance->select_callback(instance->select_callback_context, (uint32_t)index);
        }
        return true;
    }

    /* Back is intentionally not consumed: it bubbles up to the app's
     * navigation callback, which exits the app from the top-level menu. */
    return false;
}

void menu_view_set_select_callback(MenuView* instance, MenuSelectCallback callback, void* context) {
    instance->select_callback = callback;
    instance->select_callback_context = context;
}

MenuView* menu_view_alloc(void) {
    MenuView* instance = malloc(sizeof(MenuView));
    instance->view = view_alloc();
    instance->select_callback = NULL;
    instance->select_callback_context = NULL;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(MenuViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, menu_view_draw_callback);
    view_set_input_callback(instance->view, menu_view_input_callback);

    return instance;
}

void menu_view_free(MenuView* instance) {
    view_free(instance->view);
    free(instance);
}

View* menu_view_get_view(MenuView* instance) {
    return instance->view;
}
