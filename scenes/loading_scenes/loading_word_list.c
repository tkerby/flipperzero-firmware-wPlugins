#include "../../fire_string.h"

static int32_t word_list_worker(void* context) {
    FURI_LOG_I(TAG, "word_list_worker");
    furi_check(context);

    FireString* app = context;

    FURI_LOG_I(TAG, "word_list_worker %p starting", furi_thread_get_id(app->thread));

    File* word_file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    uint32_t word_count = 0;

    if(storage_file_open( // TODO: add proper error handling
           word_file,
           APP_ASSETS_PATH(DICT_FILE),
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        uint32_t buf_size = storage_file_size(word_file) + 1;

        if(buf_size > memmgr_get_free_heap()) { // Check if memory is available to read file
            FURI_LOG_E(TAG, "File too large");
        } else { // read file and build string
            uint8_t* file_buf = malloc(buf_size - 1);
            uint32_t i = 0;
            uint32_t j = 0;

            storage_file_read(word_file, file_buf, buf_size);

            while(file_buf[i] != 0x0) {
                if(file_buf[i] == '\n') {
                    word_count++;
                }
                i++;
            }

            app->hid->word_list = (FuriString**)malloc(sizeof(FuriString*) * word_count);
            if(app->hid->word_list == NULL) { // TODO: proper error handling
                FURI_LOG_E(TAG, "Failed to allocate word list");
            }

            FuriString* word_buffer = furi_string_alloc();
            i = 0;
            while(file_buf[i] != 0x0) {
                if(file_buf[i] == '\n') {
                    app->hid->word_list[j] = furi_string_alloc_move(word_buffer);
                    word_buffer = furi_string_alloc();
                    j++;
                } else {
                    if(file_buf[i] > 32 && file_buf[i] < 123) { // check ascii range
                        furi_string_push_back(word_buffer, file_buf[i]);
                    }
                }
                i++;
            }
            app->hid->word_list[j] = furi_string_alloc_move(word_buffer);
            app->hid->word_list[j + 1] = NULL;
            free(file_buf);
        }
    } else {
        FURI_LOG_E(TAG, "File open error");
    }
    storage_file_close(word_file);

    furi_record_close(RECORD_STORAGE);
    free(word_file);

    FURI_LOG_I(TAG, "word_list_worker %p ended", furi_thread_get_id(app->thread));

    return 0;
}

void fire_string_scene_on_enter_loading_word_list(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_loading_word_list");
    furi_check(context);

    FireString* app = context;

    app->thread = furi_thread_alloc_ex("WordListWorker", 2048, word_list_worker, app);
    furi_thread_start(app->thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Loading);
}

bool fire_string_scene_on_event_loading_word_list(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "fire_string_scene_on_event_loading_word_list");
    furi_check(context);

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
            scene_manager_next_scene(app->scene_manager, FireStringScene_Generate);
        } else {
            FURI_LOG_I(
                TAG,
                "loading_word_list thread id: %p state: %s",
                furi_thread_get_id(app->thread),
                furi_thread_get_state(app->thread) == FuriThreadStateRunning ? "Running" :
                                                                               "Starting");
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
    furi_check(context);
}
