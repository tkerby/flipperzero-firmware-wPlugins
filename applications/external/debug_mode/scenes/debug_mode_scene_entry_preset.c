#include "../debug_mode.h"
#include <string.h>

static const char* const* debug_mode_get_presets(const char* type, uint32_t* count) {
    if(strcmp(type, "Symptom") == 0) {
        *count = DEBUG_MODE_SYMPTOM_PRESET_COUNT;
        return debug_mode_symptom_presets;
    } else if(strcmp(type, "Test") == 0) {
        *count = DEBUG_MODE_TEST_PRESET_COUNT;
        return debug_mode_test_presets;
    } else if(strcmp(type, "Result") == 0) {
        *count = DEBUG_MODE_RESULT_PRESET_COUNT;
        return debug_mode_result_presets;
    } else if(strcmp(type, "Next Step") == 0) {
        *count = DEBUG_MODE_NEXTSTEP_PRESET_COUNT;
        return debug_mode_nextstep_presets;
    }
    *count = 0;
    return NULL;
}

static void debug_mode_scene_entry_preset_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_entry_preset_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->pending_entry_type);

    uint32_t count = 0;
    const char* const* presets = debug_mode_get_presets(app->pending_entry_type, &count);
    for(uint32_t i = 0; i < count; i++) {
        submenu_add_item(
            app->submenu, presets[i], i, debug_mode_scene_entry_preset_callback, app);
    }
    submenu_add_item(
        app->submenu, "Custom...", count, debug_mode_scene_entry_preset_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_entry_preset_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t count = 0;
        const char* const* presets = debug_mode_get_presets(app->pending_entry_type, &count);

        if(event.event == count) {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryCustomText);
            consumed = true;
        } else if(event.event < count) {
            strncpy(
                app->pending_entry_text,
                presets[event.event],
                sizeof(app->pending_entry_text) - 1);
            app->pending_entry_text[sizeof(app->pending_entry_text) - 1] = '\0';

            if(strcmp(app->pending_entry_type, "Test") == 0) {
                scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryOutcome);
            } else {
                scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryNoteChoice);
            }
            consumed = true;
        }
    }

    return consumed;
}

void debug_mode_scene_entry_preset_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
