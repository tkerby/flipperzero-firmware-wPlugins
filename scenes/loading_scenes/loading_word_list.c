#include "../../fire_string.h"

static int32_t word_list_worker(void* context) {
    FURI_LOG_T(TAG, "word_list_worker");
    furi_check(context);

    FireString* app = context;

    File* word_file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    FuriString* word_buffer = furi_string_alloc();
    size_t word_count = 0;
    size_t buf_size = 0;

    // Open wordlist file
    if(storage_file_open( // TODO: add proper error handling
           word_file,
           APP_ASSETS_PATH(DICT_FILE),
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        buf_size = storage_file_size(word_file);
        if(buf_size > memmgr_get_free_heap()) { // Check if memory is available to read file
            FURI_LOG_E(TAG, "File too large");
        } else { // read file and build string
            uint8_t* file_buf = malloc(sizeof(char) * buf_size + 1);
            size_t read_count = storage_file_read(word_file, file_buf, buf_size);

            size_t i = 0;
            while(i < read_count) { // Get word count for word_list malloc
                if(file_buf[i] == '\n') {
                    word_count++;
                }
                i++;
            }

            // malloc word_list using word_count
            app->hid->word_list = (FuriString**)malloc(sizeof(FuriString*) * word_count);
            if(app->hid->word_list == NULL) { // TODO: proper error handling
                FURI_LOG_E(TAG, "Failed to allocate word list");
            }

            i = 0;
            size_t j = 0;
            while(i < read_count) {
                if(file_buf[i] == '\n' && read_count > 0) {
                    app->hid->word_list[j] = furi_string_alloc_set(word_buffer);
                    furi_string_reset(word_buffer);
                    j++;
                } else {
                    furi_string_push_back(word_buffer, file_buf[i]);
                }
                i++;
            }
            app->hid->word_list[j] = furi_string_alloc_set(word_buffer);
            app->hid->word_list[j + 1] = furi_string_alloc();
            free(file_buf);
        }
    } else {
        FURI_LOG_E(TAG, "File open error");
    }
    storage_file_close(word_file);
    free(word_file);
    furi_record_close(RECORD_STORAGE);
    if(word_buffer != NULL) {
        furi_string_free(word_buffer);
    }

    return 0;
}

void fire_string_scene_on_enter_loading_word_list(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_loading_word_list");
    furi_check(context);

    FireString* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Loading);

    app->thread = furi_thread_alloc_ex("WordListWorker", 2048, word_list_worker, app);
    furi_thread_start(app->thread);
}

bool fire_string_scene_on_event_loading_word_list(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_loading_word_list");

    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        default:
            break;
        }
        break;

    case SceneManagerEventTypeTick:
        if(furi_thread_get_state(app->thread) == FuriThreadStateStopped) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FireStringScene_Generate);
        }
        break;
    case SceneManagerEventTypeBack:
        consumed = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    default:
        break;
    }
    return consumed;
}

void fire_string_scene_on_exit_loading_word_list(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_loading_word_list");
    furi_check(context);

    FireString* app = context;

    furi_thread_free(app->thread);
}
