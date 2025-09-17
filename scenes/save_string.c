#include "../fire_string.h"
#include "storage/storage.h"

void text_input_callback(void* context) {
    FURI_LOG_T(TAG, "text_input_callback");
    furi_check(context);

    FireString* app = context;

    // Open storage
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Allocate file
    File* file = storage_file_alloc(storage);

    FuriString* file_name = furi_string_alloc();
    furi_string_printf(file_name, "%s%s%s", DEFAULT_PATH, app->text_buffer, FILE_EXT);

    // Open file, write data and close it
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    if(!storage_file_open(file, furi_string_get_cstr(file_name), FSAM_WRITE, FSOM_CREATE_NEW)) {
        FURI_LOG_E(TAG, "File Exists");
        dialog_message_show_storage_error(dialogs, "File Exists");
    } else {
        if(!storage_file_write(
               file, furi_string_get_cstr(app->fire_string), app->settings->str_len + 1)) {
            FURI_LOG_E(TAG, "Failed to write to file");
            dialog_message_show_storage_error(dialogs, "Failed to write to file");

        } else {
            DialogMessage* message = dialog_message_alloc();
            FuriString* text;

            dialog_message_set_header(message, "Success!", 98, 17, AlignCenter, AlignTop);
            dialog_message_set_icon(message, &I_DolphinMafia_119x62, 0, 2);
            text = furi_string_alloc_printf("FireString Saved");
            dialog_message_set_buttons(message, NULL, "ok!", NULL);

            dialog_message_show(dialogs, message);

            dialog_message_free(message);
            furi_string_free(text);
        }
    }

    storage_file_close(file);
    furi_string_free(file_name);

    // Deallocate file
    storage_file_free(file);

    // Close storage
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, FireStringScene_GenerateStepTwo);
}

void fire_string_scene_on_enter_save_string(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_save_string");
    furi_check(context);

    FireString* app = context;

    text_input_set_result_callback(
        app->text_input, text_input_callback, app, app->text_buffer, TEXT_INPUT_BUF_SIZE, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_TextInput);
}

bool fire_string_scene_on_event_save_string(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void fire_string_scene_on_exit_save_string(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_save_string");
    furi_check(context);

    FireString* app = context;

    text_input_reset(app->text_input);
}
