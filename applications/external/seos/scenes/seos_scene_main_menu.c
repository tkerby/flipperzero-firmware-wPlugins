#include "../seos_i.h"

#define TAG "SceneMainMenu"

enum SubmenuIndex {
    SubmenuIndexSaved,
    SubmenuIndexRead,
    SubmenuIndexBLEReader,
    SubmenuIndexScannerMenu,
    SubmenuIndexBLECredInterrogate,
    SubmenuIndexAbout,
    SubmenuIndexInspect,
};

void seos_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, index);
}

void seos_scene_main_menu_on_enter(void* context) {
    Seos* seos = context;
    Submenu* submenu = seos->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu, "Saved", SubmenuIndexSaved, seos_scene_main_menu_submenu_callback, seos);
    submenu_add_item(
        submenu, "Read NFC", SubmenuIndexRead, seos_scene_main_menu_submenu_callback, seos);
    if(seos->has_ble) {
        submenu_add_item(
            submenu,
            "MainMenu BLE Reader",
            SubmenuIndexBLEReader,
            seos_scene_main_menu_submenu_callback,
            seos);
        submenu_add_item(
            submenu,
            "Scanners >",
            SubmenuIndexScannerMenu,
            seos_scene_main_menu_submenu_callback,
            seos);
        /*
        submenu_add_item(
            submenu,
            "BLE Cred Interrogate",
            SubmenuIndexBLECredInterrogate,
            seos_scene_main_menu_submenu_callback,
            seos);
            */
    }
    /*
    submenu_add_item(
        submenu, "Inspect", SubmenuIndexInspect, seos_scene_main_menu_submenu_callback, seos);
    */
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, seos_scene_main_menu_submenu_callback, seos);

    submenu_set_selected_item(
        seos->submenu, scene_manager_get_scene_state(seos->scene_manager, SeosSceneMainMenu));

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewMenu);
}

bool seos_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexRead);
            scene_manager_next_scene(seos->scene_manager, SeosSceneRead);
            consumed = true;
        } else if(event.event == SubmenuIndexBLEReader) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexBLEReader);
            seos->flow_mode = FLOW_READER;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBlePeripheral);
            consumed = true;
        } else if(event.event == SubmenuIndexScannerMenu) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexScannerMenu);
            scene_manager_next_scene(seos->scene_manager, SeosSceneScannerMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexBLECredInterrogate) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexBLECredInterrogate);
            seos->flow_mode = FLOW_READER;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBleCentral);
            consumed = true;
        } else if(event.event == SubmenuIndexSaved) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexSaved);
            scene_manager_next_scene(seos->scene_manager, SeosSceneFileSelect);
            consumed = true;
        } else if(event.event == SubmenuIndexInspect) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexInspect);
            seos->flow_mode = FLOW_INSPECT;
            scene_manager_next_scene(seos->scene_manager, SeosSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexAbout);
            scene_manager_next_scene(seos->scene_manager, SeosSceneAbout);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        while(scene_manager_previous_scene(seos->scene_manager))
            ;
    }

    return consumed;
}

void seos_scene_main_menu_on_exit(void* context) {
    Seos* seos = context;

    submenu_reset(seos->submenu);
}
