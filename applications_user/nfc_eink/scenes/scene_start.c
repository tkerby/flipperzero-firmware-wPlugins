#include "../nfc_eink_app.h"

enum SubmenuIndex {
    SubmenuIndexEmulate,
    SubmenuIndexWrite,
    SubmenuIndexRead,
};

static void nfc_eink_scene_start_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void nfc_eink_scene_start_on_enter(void* context) {
    NfcEinkApp* instance = context;
    Submenu* submenu = instance->submenu;

    submenu_add_item(
        submenu, "Emulate", SubmenuIndexEmulate, nfc_eink_scene_start_submenu_callback, instance);
    submenu_add_item(
        submenu,
        "Write To Eink",
        SubmenuIndexWrite,
        nfc_eink_scene_start_submenu_callback,
        instance);
    submenu_add_item(
        submenu, "Read Eink", SubmenuIndexWrite, nfc_eink_scene_start_submenu_callback, instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewMenu);
}

bool nfc_eink_scene_start_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        if(submenu_index == SubmenuIndexEmulate) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneChooseType);
            consumed = true;
        } else if(submenu_index == SubmenuIndexWrite) {
            //scene_manager_next_scene(scene_manager, );
            consumed = true;
        } else if(submenu_index == SubmenuIndexRead) {
            //scene_manager_next_scene(scene_manager, );
            consumed = true;
        }
    }

    return consumed;
}

void nfc_eink_scene_start_on_exit(void* context) {
    NfcEinkApp* instance = context;
    submenu_reset(instance->submenu);
}
