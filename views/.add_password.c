#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include "saved_passwords.h"

void add_passwords_draw_callback(Canvas* canvas, void* model) {
    const char* items[SAVED_ITEMS] = {"Email - user1", "GitHub - dev99"};

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    // Draw header
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 30, 10, "Saved Passwords");
    canvas_draw_line(canvas, 0, 12, 128, 12);

    for(size_t i = 0; i < SAVED_ITEMS; i++) {
        int y = 25 + i * 12;
        if(i == model->saved_passwords_selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 10, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 5, y, items[i]);
    }
}

void add_passwords_input_callback(InputEvent* event, void* context) {
    if(event->key == InputKeyUp) {
        if(model->saved_passwords_selected > 0) model->saved_passwords_selected--;
    } else if(event->key == InputKeyDown) {
        if(model->saved_passwords_selected + 1 < SAVED_ITEMS) model->saved_passwords_selected++;
    } else if(event->key == InputKeyBack) {
        model->current_state = AppStateMainMenu; // Return to main menu
    } else if(event->key == InputKeyOk) {
        // Handle OK for selected saved password
        // You could show details in a new state
    }
}

View* add_passwords_view_alloc(AppContext* app_context) {

    View* view = view_alloc();

    AppContext* app = app_context;
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(AppContext*));
    AppContext** app_view = view_get_model(view);
    *app_view = app;
    view_set_draw_callback(view, add_passwords_draw_callback);
    view_set_input_callback(view, add_passwords_input_callback);
    
    return view;
}