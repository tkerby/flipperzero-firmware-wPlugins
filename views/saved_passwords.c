#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#include "saved_passwords.h"
#include "../badusb/badusb.h"

static void saved_passwords_draw_callback(Canvas* canvas, void* model) {

    AppContext** model_ = model;
    AppContext* app = *model_;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    // Draw header
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 30, 10, "Saved Passwords");
    canvas_draw_line(canvas, 0, 12, 128, 12);

    for(size_t i = 0; i < app->credentials_number; i++) {
        int y = 25 + i * 12;
        if(i == app->selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 10, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 5, y, app->credentials[i].name);
    }
}

static bool saved_passwords_input_callback(InputEvent* event, void* context) {
    AppContext* app = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(app->selected > 0) app->selected--;
            return true;
        } else if(event->key == InputKeyDown) {
            if(app->selected + 1 < app->credentials_number) app->selected++;
            return true;
        } else if(event->key == InputKeyBack) {
            app->selected = 0;
            // Will be handled by view_dispatcher_navigation_event_callback
            return false;
        } else if(event->key == InputKeyOk) {
            // Write Username, tab, Password !!!
            initialize_hid();
            type_string(app->credentials[app->selected].username);
            press_key(HID_KEYBOARD_TAB);
            type_string(app->credentials[app->selected].password);
            press_key(HID_KEYBOARD_RETURN); 
            release_all_keys();
            return true;
        }
    }
    
    return false;
}

View* saved_passwords_view_alloc(AppContext* app_context) {

    View* view = view_alloc();

    AppContext* app = app_context;
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(AppContext*));
    AppContext** app_view = view_get_model(view);
    *app_view = app;
    view_set_draw_callback(view, saved_passwords_draw_callback);
    view_set_input_callback(view, saved_passwords_input_callback);
    
    return view;
}