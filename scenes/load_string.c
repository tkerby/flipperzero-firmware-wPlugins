#include "../firestring.h"
#include "storage/storage.h"

void fire_string_scene_on_enter_load_string(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_load_string");
    furi_check(context);

    FireString* app = context;

    File* rnd_file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* file_path = furi_string_alloc();

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, FILE_EXT, NULL);
    browser_options.base_path = DEFAULT_PATH;
    furi_string_set_str(file_path, browser_options.base_path);

    storage_simply_mkdir(storage, furi_string_get_cstr(file_path));

    bool res = dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options);

    if(res) {
        if(storage_file_open(
               rnd_file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint32_t buf_size = storage_file_size(
                rnd_file); // TOOD: consider checking available memory to avoid OOM errors on large files
            char* file_buf = malloc(sizeof(char) * buf_size);
            res = storage_file_read(rnd_file, file_buf, buf_size - 1);
            furi_string_set_str(app->fire_string, file_buf);
            app->settings->str_len = furi_string_size(app->fire_string);
            free(file_buf);
        } else {
            FURI_LOG_E(TAG, "File open error");
        }
        storage_file_close(rnd_file);
    }

    storage_file_free(rnd_file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(file_path);

    if(res) {
        scene_manager_next_scene(app->scene_manager, FireStringScene_Generate);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool fire_string_scene_on_event_load_string(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void fire_string_scene_on_exit_load_string(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_load_string");
    furi_check(context);

    FireString* app = context;

    scene_manager_set_scene_state(app->scene_manager, FireStringScene_LoadString, 0);
}
