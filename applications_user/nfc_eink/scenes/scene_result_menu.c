#include "../nfc_eink_app.h"

enum SubmenuIndex {
    SubmenuIndexShow,
    SubmenuIndexSave,
    //SubmenuIndexRead,
};

static void nfc_eink_scene_result_menu_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void nfc_eink_scene_result_menu_on_enter(void* context) {
    NfcEinkApp* instance = context;
    Submenu* submenu = instance->submenu;

    submenu_add_item(
        submenu, "Show", SubmenuIndexShow, nfc_eink_scene_result_menu_submenu_callback, instance);
    submenu_add_item(
        submenu, "Save", SubmenuIndexSave, nfc_eink_scene_result_menu_submenu_callback, instance);
    /*  submenu_add_item(
        submenu,
        "Read Eink",
        SubmenuIndexWrite,
        nfc_eink_scene_result_menu_submenu_callback,
        instance); */

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewMenu);
}

bool nfc_eink_scene_result_menu_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        if(submenu_index == SubmenuIndexShow) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneResultImage);
            consumed = true;
        } else if(submenu_index == SubmenuIndexSave) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneSaveName);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneExitConfirm);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_result_menu_on_exit(void* context) {
    NfcEinkApp* instance = context;
    submenu_reset(instance->submenu);
}
