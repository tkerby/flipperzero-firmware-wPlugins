/*
Selects a code from a file picker

Then goes to send code screen
*/
#include "flipper.h"
#include "app_state.h"
#include "scenes.h"
#include "scene_select_code.h"

static void file_browser_callback(void* context) {
    App* app = context;
    furi_assert(app);

    FlipperFormat* file = flipper_format_file_alloc(app->storage);

    uint32_t version = 1;
    FuriString* file_type;
    FuriString* string_value;
    file_type = furi_string_alloc();
    string_value = furi_string_alloc();
    do {
        //FURI_LOG_I(TAG, "opening %s", furi_string_get_cstr(app->file_path));
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(app->file_path))) break;
        //FURI_LOG_I(TAG, "reading header");
        if(!flipper_format_read_header(file, file_type, &version)) break;
        //FURI_LOG_I(TAG, "read header %s %d", furi_string_get_cstr(file_type), (int)version);
        if(!flipper_format_read_string(file, "Code", string_value)) break;
        FURI_LOG_I(TAG, "read code %s", furi_string_get_cstr(string_value));

        // signal that the file was read successfully
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);

    strncpy(app->state->current_code, furi_string_get_cstr(string_value), MAX_DIGIROM_LEN);

    FURI_LOG_I(TAG, "file_browser_callback read %s", app->state->current_code);

    scene_manager_next_scene(app->scene_manager, FcomSendCodeScene);
}

void fcom_select_code_scene_on_enter(void* context) {
    FURI_LOG_I(TAG, "fcom_select_code_scene_on_enter");
    App* app = context;

    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    file_browser_start(app->file_browser, app->file_path);

    view_dispatcher_switch_to_view(app->view_dispatcher, FcomFileSelectView);
}

bool fcom_select_code_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_I(TAG, "fcom_select_code_scene_on_event");
    UNUSED(context);
    UNUSED(event);

    return false; //consumed event
}

void fcom_select_code_scene_on_exit(void* context) {
    FURI_LOG_I(TAG, "fcom_select_code_scene_on_exit");
    App* app = context;

    file_browser_stop(app->file_browser);
}
