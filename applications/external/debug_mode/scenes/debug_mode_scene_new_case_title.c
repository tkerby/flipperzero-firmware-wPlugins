#include "../debug_mode.h"
#include <string.h>

static void debug_mode_scene_new_case_title_callback(void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void debug_mode_scene_new_case_title_on_enter(void* context) {
    DebugMode* app = context;

    if(case_store_total_count(app->storage) >= DEBUG_MODE_MAX_CASES) {
        snprintf(app->message_title, sizeof(app->message_title), "Case Limit Reached");
        snprintf(
            app->message_body,
            sizeof(app->message_body),
            "Resolve or remove an existing case before creating a new one.");
        app->message_return_scene = DebugModeSceneMainMenu;
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        return;
    }

    text_input_reset(app->text_input);
    app->current_case_title[0] = '\0';
    text_input_set_header_text(app->text_input, "Case Title");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        debug_mode_scene_new_case_title_callback,
        app,
        app->current_case_title,
        sizeof(app->current_case_title),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdTextInput);
}

bool debug_mode_scene_new_case_title_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t id = case_store_next_id(app->storage);
        if(case_store_create_with_priority(
               app->storage,
               id,
               app->current_case_title,
               app->current_case_category,
               app->current_case_priority[0] ? app->current_case_priority : "Medium")) {
            app->current_case_id = id;
            strncpy(app->current_case_status, "Open", sizeof(app->current_case_status) - 1);
            app->current_case_status[sizeof(app->current_case_status) - 1] = '\0';
            if(app->pending_template_id != 0) {
                case_store_append_entry(
                    app->storage,
                    id,
                    "Note",
                    "Guided case created from template.",
                    "",
                    "");
                case_store_append_entry(
                    app->storage,
                    id,
                    "Next Step",
                    "Capture the first symptom, then log one test and result.",
                    "",
                    "");
            }
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseView);
        } else {
            snprintf(app->message_title, sizeof(app->message_title), "Save Failed");
            snprintf(
                app->message_body,
                sizeof(app->message_body),
                "Could not write the case file. Check the SD card.");
            app->message_return_scene = DebugModeSceneMainMenu;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_new_case_title_on_exit(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
}
