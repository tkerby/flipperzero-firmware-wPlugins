#include "../infrared_app_i.h"

enum SubmenuIndex {
    SubmenuIndexUniversalRemotes,
    SubmenuIndexLearnNewRemote,
    SubmenuIndexSavedRemotes,
    SubmenuIndexGpioSettings,
    SubmenuIndexEasyLearn,
    SubmenuIndexLearnNewRemoteRaw,
    SubmenuIndexDebug
};

static void infrared_scene_start_submenu_callback(void* context, uint32_t index) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, index);
}

void infrared_scene_start_on_enter(void* context) {
    InfraredApp* infrared = context;
    Submenu* submenu = infrared->submenu;
    SceneManager* scene_manager = infrared->scene_manager;

    submenu_add_item(
        submenu,
        "Universal Remotes",
        SubmenuIndexUniversalRemotes,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Learn New Remote",
        SubmenuIndexLearnNewRemote,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "Saved Remotes",
        SubmenuIndexSavedRemotes,
        infrared_scene_start_submenu_callback,
        infrared);
    submenu_add_item(
        submenu,
        "GPIO Settings",
        SubmenuIndexGpioSettings,
        infrared_scene_start_submenu_callback,
        infrared);

    char easy_learn_text[24];
    snprintf(
        easy_learn_text,
        sizeof(easy_learn_text),
        "Easy Learn [%s]",
        infrared->app_state.is_easy_mode ? "X" : " ");
    submenu_add_item(
        submenu,
        easy_learn_text,
        SubmenuIndexEasyLearn,
        infrared_scene_start_submenu_callback,
        infrared);

    submenu_add_lockable_item(
        submenu,
        "Learn New Remote RAW",
        SubmenuIndexLearnNewRemoteRaw,
        infrared_scene_start_submenu_callback,
        infrared,
        !infrared->app_state.is_debug_enabled,
        "Enable\nDebug!");
    submenu_add_lockable_item(
        submenu,
        "Debug RX",
        SubmenuIndexDebug,
        infrared_scene_start_submenu_callback,
        infrared,
        !infrared->app_state.is_debug_enabled,
        "Enable\nDebug!");

    const uint32_t submenu_index =
        scene_manager_get_scene_state(scene_manager, InfraredSceneStart);
    submenu_set_selected_item(submenu, submenu_index);
    // scene_manager_set_scene_state(scene_manager, InfraredSceneStart, SubmenuIndexUniversalRemotes);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewSubmenu);
}

bool infrared_scene_start_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    SceneManager* scene_manager = infrared->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        scene_manager_set_scene_state(scene_manager, InfraredSceneStart, submenu_index);
        if(submenu_index == SubmenuIndexUniversalRemotes) {
            // Set file_path only once here so repeated usages of
            // "Load from Library File" have file browser focused on
            // last selected file, feels more intuitive
            furi_string_set(infrared->file_path, INFRARED_APP_FOLDER);
            scene_manager_next_scene(scene_manager, InfraredSceneUniversal);
        } else if(
            submenu_index == SubmenuIndexLearnNewRemote ||
            submenu_index == SubmenuIndexLearnNewRemoteRaw) {
            // enable automatic signal decoding if "Learn New Remote"
            // disable automatic signal decoding if "Learn New Remote (RAW)"
            infrared_worker_rx_enable_signal_decoding(
                infrared->worker, submenu_index == SubmenuIndexLearnNewRemote);
            infrared->app_state.is_learning_new_remote = true;
            infrared->app_state.current_button_index = 0;  // Reset index when starting new remote
            scene_manager_next_scene(scene_manager, InfraredSceneLearn);
        } else if(submenu_index == SubmenuIndexSavedRemotes) {
            furi_string_set(infrared->file_path, INFRARED_APP_FOLDER);
            scene_manager_next_scene(scene_manager, InfraredSceneRemoteList);
        } else if(submenu_index == SubmenuIndexGpioSettings) {
            scene_manager_next_scene(scene_manager, InfraredSceneGpioSettings);
        } else if(submenu_index == SubmenuIndexEasyLearn) {
            infrared->app_state.is_easy_mode = !infrared->app_state.is_easy_mode;
            infrared_save_settings(infrared);
            // Update the menu item text without scene transition
            char easy_learn_text[24];
            snprintf(
                easy_learn_text,
                sizeof(easy_learn_text),
                "Easy Learn [%s]",
                infrared->app_state.is_easy_mode ? "X" : " ");
            submenu_change_item_label(infrared->submenu, SubmenuIndexEasyLearn, easy_learn_text);
        } else if(submenu_index == SubmenuIndexDebug) {
            scene_manager_next_scene(scene_manager, InfraredSceneDebug);
        }

        consumed = true;
    }

    return consumed;
}

void infrared_scene_start_on_exit(void* context) {
    InfraredApp* infrared = context;
    submenu_reset(infrared->submenu);
}
