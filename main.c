#include "main.h"
#include "views/saved_passwords.h"

static void render_main_menu(Canvas* canvas, void* model) {

    if (!model) {
        FURI_LOG_E("Password Manager", "render_main_menu: model is NULL!");
        return;
    }

    AppContext** model_ = model;
    AppContext* app = *model_;

    FURI_LOG_I("Password Manager", "Context in render: %p", app);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);


    for(size_t i = 0; i < MENU_ITEMS; i++) {
        int y = 15 + i * 12;

        if(i == app->selected) {
            // Highlight selected item with black box
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 10, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 5, y, app->items[i]);
    }

}

static bool handle_main_menu_input(InputEvent* event, void* context) {

    AppContext* app = context;

    if (!context) {
        FURI_LOG_E("Password Manager", "handle_main_menu_input: context is NULL!");
        return false;
    }

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(app->selected > 0) app->selected--;
            return true;
        } else if(event->key == InputKeyDown) {
            if(app->selected + 1 < MENU_ITEMS) app->selected++;
            return true;
        } else if(event->key == InputKeyBack) {
            app->running = false;
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        } else if(event->key == InputKeyOk) {
            if(app->selected == 0) {
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewSavedPasswords);
                // FURI_LOG_I("Password Manager", "Saved passwords view not implemented yet");
                return true;
            } else if(app->selected == 1) {
                // Add password flow - disabled for now
                FURI_LOG_I("Password Manager", "Add password flow not implemented yet");
                // app->selected = 0;
                return true;
            } else if(app->selected == 2) {
                // Delete password view - disabled for now
                FURI_LOG_I("Password Manager", "Delete password view not implemented yet");
                // app->selected = 0;
                return true;
            }
        }
    }
    
    return false;
}

// Custom callback for the view dispatcher
static bool view_dispatcher_navigation_event_callback(void* context) {
    AppContext* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMainMenu);
    return true;
}

int32_t password_manager_app(void* p) {
    UNUSED(p);

    // Initialize the app context
    AppContext* app = malloc(sizeof(AppContext));

    if (!app) {
        // Handle allocation failure
        return -1;
    }
    
    // Initialize app context fields to avoid nullptr issues
    memset(app, 0, sizeof(AppContext));

    app->selected = 0;
    app->running = true;

    app->items[0] = "Saved";
    app->items[1] = "Add";
    app->items[2] = "Delete";

    // TODO: remove hardcoded credentials to read file
    app->credentials_number = 2;
    Credential first = {"Github", "User1", "Password"};
    Credential second = {"Gmail", "User1@gmail.com", "Password2"};
    app->credentials[0] = first;
    app->credentials[1] = second;
    // END TODO

    // Initialize GUI and View Dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    if (!app->gui) {
        free(app);
        return -2;
    }
    
    app->view_dispatcher = view_dispatcher_alloc();
    if (!app->view_dispatcher) {
        furi_record_close(RECORD_GUI);
        free(app);
        return -3;
    }
    
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, view_dispatcher_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize Main Menu View
    app->main_menu_view = view_alloc();
    if (!app->main_menu_view) {
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_GUI);
        free(app);
        return -4;
    }

    view_set_context(app->main_menu_view, app);
    view_allocate_model(app->main_menu_view, ViewModelTypeLockFree, sizeof(AppContext*));
    AppContext** app_view = view_get_model(app->main_menu_view);
    *app_view = app;

    view_set_draw_callback(app->main_menu_view, render_main_menu);
    view_set_input_callback(app->main_menu_view, handle_main_menu_input);

    view_dispatcher_add_view(app->view_dispatcher, ViewMainMenu, app->main_menu_view);

    app->saved_passwords_view = saved_passwords_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewSavedPasswords, app->saved_passwords_view);

    // Start with main menu
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMainMenu);

    // Main loop - just run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free all resources when app exits
    view_dispatcher_remove_view(app->view_dispatcher, ViewMainMenu);
    view_free(app->main_menu_view);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSavedPasswords);
    view_free(app->saved_passwords_view);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    
    free(app);

    return 0;
}