#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>
#define TAG "VGMGameRemote"

typedef enum {
    VGMGameRemoteSubmenuIndexRun,
    VGMGameRemoteSubmenuIndexAbout,
    VGMGameRemoteSubmenuIndexSettings,
} VGMGameRemoteSubmenuIndex;

typedef enum {
    VGMGameRemoteViewMain, // The main screen
    VGMGameRemoteViewSubmenu, // The submenu
    VGMGameRemoteViewAbout, // The about screen
    VGMGameRemoteViewSettings, // The settings screen
} VGMGameRemoteView;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    View* view_main; // The main screen that displays the button
    Submenu* submenu; // The submenu
    Widget* widget; // The widget
    VariableItemList* settings; // The settings
    VariableItem* send_choice; // The send choice
    FlipperHTTP* fhttp; // The FlipperHTTP context
    char last_input[2]; // The last input
    uint8_t choices_index; // The current choice index
    char* choices[2]; // The choices
    FuriTimer* timer;
} VGMGameRemote;

typedef enum {
    VGMGameRemoteCustomEventSend
} VGMGameRemoteCustomEvent;

static bool send_data(VGMGameRemote* app) {
    furi_check(app);
    return strlen(app->last_input) == 0 || flipper_http_send_data(app->fhttp, app->last_input);
}
static void timer_callback(void* context) {
    furi_check(context, "timer_callback: Context is NULL");
    VGMGameRemote* app = (VGMGameRemote*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, VGMGameRemoteCustomEventSend);
}
static void loader_process_callback(void* context) {
    VGMGameRemote* app = (VGMGameRemote*)context;
    furi_check(app, "VGMGameRemote is NULL");
    send_data(app);
}
static bool custom_event_callback(void* context, uint32_t index) {
    furi_check(context, "custom_event_callback: Context is NULL");
    UNUSED(index);
    loader_process_callback(context);
    return true;
}
static void draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Press any button");
}
static bool input_callback(InputEvent* event, void* context) {
    VGMGameRemote* app = (VGMGameRemote*)context;
    furi_check(app);
    if(event->key == InputKeyUp) {
        app->last_input[0] = '0';
        app->last_input[1] = '\0';
    } else if(event->key == InputKeyDown) {
        app->last_input[0] = '1';
        app->last_input[1] = '\0';
    } else if(event->key == InputKeyLeft) {
        app->last_input[0] = '2';
        app->last_input[1] = '\0';
    } else if(event->key == InputKeyRight) {
        app->last_input[0] = '3';
        app->last_input[1] = '\0';
    } else if(event->key == InputKeyOk) {
        app->last_input[0] = '4';
        app->last_input[1] = '\0';
    }
    // if held back button
    else if(event->key == InputKeyBack) {
        if(event->type == InputTypeLong)
            view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewSubmenu);
        else {
            app->last_input[0] = '5';
            app->last_input[1] = '\0';
        }
    }
    return app->choices_index == 0 ? true : send_data(app);
}

static void free_fhttp(VGMGameRemote* app) {
    furi_check(app);
    if(app->fhttp) {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }
}
static void free_timer(VGMGameRemote* app) {
    furi_check(app);
    if(app->timer) {
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}
static void callback_submenu_choices(void* context, uint32_t index) {
    VGMGameRemote* app = (VGMGameRemote*)context;
    furi_check(app);
    switch(index) {
    case VGMGameRemoteSubmenuIndexRun:
        free_fhttp(app);
        app->fhttp = flipper_http_alloc();
        if(!app->fhttp) {
            easy_flipper_dialog(
                "Error",
                "Failed to allocate FlipperHTTP\nUART likely busy.Restart\nyour Flipper Zero.");
            return;
        }
        if(app->choices_index == 0) {
            free_timer(app);
            app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(app->timer, 100);
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewMain);
        break;
    case VGMGameRemoteSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewAbout);
        break;
    case VGMGameRemoteSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewSettings);
        break;
    default:
        break;
    }
}
static uint32_t callback_to_submenu(void* context) {
    VGMGameRemote* app = (VGMGameRemote*)context;
    furi_check(app);
    return VGMGameRemoteViewSubmenu;
}

static uint32_t callback_exit_app(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void settings_item_selected(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static bool save_char(const char* path_name, const char* value) {
    if(!value) {
        return false;
    }
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(
        directory_path,
        sizeof(directory_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/vgm_game_remote");

    // Create the directory
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    snprintf(
        directory_path,
        sizeof(directory_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/vgm_game_remote/data");
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File* file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(
        file_path,
        sizeof(file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/vgm_game_remote/data/%s.txt",
        path_name);

    // Open the file in write mode
    if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(HTTP_TAG, "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write the data to the file
    size_t data_size = strlen(value) + 1; // Include null terminator
    if(storage_file_write(file, value, data_size) != data_size) {
        FURI_LOG_E(HTTP_TAG, "Failed to append data to file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

static bool load_char(const char* path_name, char* value, size_t value_size) {
    if(!value) {
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    char file_path[256];
    snprintf(
        file_path,
        sizeof(file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/vgm_game_remote/data/%s.txt",
        path_name);

    // Open the file for reading
    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, value, value_size);
    if(storage_file_get_error(file) != FSE_OK) {
        FURI_LOG_E(HTTP_TAG, "Error reading from file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Ensure null-termination
    value[read_count - 1] = '\0';

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return strlen(value) > 0;
}
static void choices_on_change(VariableItem* item) {
    VGMGameRemote* app = (VGMGameRemote*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->choices_index = index;
    variable_item_set_current_value_text(item, app->choices[app->choices_index]);
    variable_item_set_current_value_index(item, app->choices_index);
    save_char("button-choices", app->choices[app->choices_index]);
}
static VGMGameRemote* app_alloc() {
    VGMGameRemote* app = (VGMGameRemote*)malloc(sizeof(VGMGameRemote));

    Gui* gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if(!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app)) {
        return NULL;
    }
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    // Main view
    if(!easy_flipper_set_view(
           &app->view_main,
           VGMGameRemoteViewMain,
           draw_callback,
           input_callback,
           callback_to_submenu,
           &app->view_dispatcher,
           app)) {
        return NULL;
    }

    // Widget
    if(!easy_flipper_set_widget(
           &app->widget,
           VGMGameRemoteViewAbout,
           "VGM Game Engine Remote\n-----\nCompanion app for the VGM\nGame Engine.\n-----\nwww.github.com/jblanked",
           callback_to_submenu,
           &app->view_dispatcher)) {
        return NULL;
    }

    // Choices
    app->choices[0] = "Spam";
    app->choices[1] = "Once";

    // Settings
    if(!easy_flipper_set_variable_item_list(
           &app->settings,
           VGMGameRemoteViewSettings,
           settings_item_selected,
           callback_to_submenu,
           &app->view_dispatcher,
           app)) {
        return NULL;
    }
    app->send_choice =
        variable_item_list_add(app->settings, "Frequency", 2, choices_on_change, app);
    char choice[64];
    if(!load_char("button-choices", choice, sizeof(choice))) {
        variable_item_set_current_value_index(app->send_choice, 1);
        variable_item_set_current_value_text(app->send_choice, app->choices[1]);
        app->choices_index = 1;
        save_char("button-choices", app->choices[1]);
    } else {
        app->choices_index = strcmp(choice, "Spam") == 0 ? 0 : 1;
        variable_item_set_current_value_index(app->send_choice, app->choices_index);
        variable_item_set_current_value_text(app->send_choice, app->choices[app->choices_index]);
    }

    // Submenu
    if(!easy_flipper_set_submenu(
           &app->submenu,
           VGMGameRemoteViewSubmenu,
           "VGM Game Remote",
           callback_exit_app,
           &app->view_dispatcher)) {
        return NULL;
    }
    submenu_add_item(
        app->submenu, "Run", VGMGameRemoteSubmenuIndexRun, callback_submenu_choices, app);
    submenu_add_item(
        app->submenu, "About", VGMGameRemoteSubmenuIndexAbout, callback_submenu_choices, app);
    submenu_add_item(
        app->submenu, "Settings", VGMGameRemoteSubmenuIndexSettings, callback_submenu_choices, app);

    app->timer = NULL;
    app->fhttp = NULL;

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewSubmenu);

    return app;
}

// Function to free the resources used by VGMGameRemote
static void app_free(VGMGameRemote* app) {
    furi_check(app);

    // Free View(s)
    if(app->view_main) {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewMain);
        view_free(app->view_main);
    }

    // Free Submenu(s)
    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewSubmenu);
        submenu_free(app->submenu);
    }

    // Free Widget(s)
    if(app->widget) {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewAbout);
        widget_free(app->widget);
    }

    // Free Variable Item List(s)
    if(app->settings) {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewSettings);
        variable_item_list_free(app->settings);
    }

    // Free the timer
    free_timer(app);

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    free_fhttp(app);

    // free the app
    free(app);
    app = NULL;
}

// Entry point for the [VGM] Game Remote application
int32_t vgm_game_remote_main(void* p) {
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the [VGM] Game Remote application
    VGMGameRemote* app = app_alloc();
    furi_check(app);

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the [VGM] Game Remote application
    app_free(app);

    // Return 0 to indicate success
    return 0;
}
