#include "../passy_i.h"

#define TAG "SceneMainMenu"

enum SubmenuIndex {
    SubmenuIndexEnterMRZInfo,
    SubmenuIndexDeleteMRZInfo,
    SubmenuIndexReadCOM,
    SubmenuIndexReadDG1,
    SubmenuIndexReadDG2,
    SubmenuIndexReadDG7,
};

void passy_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    Passy* passy = context;
    view_dispatcher_send_custom_event(passy->view_dispatcher, index);
}

void passy_scene_main_menu_on_enter(void* context) {
    Passy* passy = context;
    Submenu* submenu = passy->submenu;
    submenu_reset(submenu);

    passy_load_mrz_info(passy);

    submenu_add_item(
        submenu,
        "Enter MRZ Info",
        SubmenuIndexEnterMRZInfo,
        passy_scene_main_menu_submenu_callback,
        passy);
    if(strlen(passy->passport_number) > 0 && strlen(passy->date_of_birth) > 0 &&
       strlen(passy->date_of_expiry) > 0) {
        submenu_add_item(
            submenu,
            "Delete MRZ Info",
            SubmenuIndexDeleteMRZInfo,
            passy_scene_main_menu_submenu_callback,
            passy);

        submenu_add_item(
            submenu,
            "Read COM",
            SubmenuIndexReadCOM,
            passy_scene_main_menu_submenu_callback,
            passy);
        submenu_add_item(
            submenu,
            "Read DG1",
            SubmenuIndexReadDG1,
            passy_scene_main_menu_submenu_callback,
            passy);
        submenu_add_item(
            submenu,
            "Read DG2",
            SubmenuIndexReadDG2,
            passy_scene_main_menu_submenu_callback,
            passy);
        submenu_add_item(
            submenu,
            "Read DG7",
            SubmenuIndexReadDG7,
            passy_scene_main_menu_submenu_callback,
            passy);
    }

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewMenu);
}

bool passy_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(passy->scene_manager, PassySceneMainMenu, event.event);
        if(event.event == SubmenuIndexEnterMRZInfo) {
            scene_manager_next_scene(passy->scene_manager, PassyScenePassportNumberInput);
            consumed = true;
        } else if(event.event == SubmenuIndexDeleteMRZInfo) {
            scene_manager_next_scene(passy->scene_manager, PassySceneDelete);
            consumed = true;
        } else if(event.event == SubmenuIndexReadCOM) {
            passy->read_type = PassyReadCOM;
            scene_manager_next_scene(passy->scene_manager, PassySceneRead);
            consumed = true;
        } else if(event.event == SubmenuIndexReadDG1) {
            passy->read_type = PassyReadDG1;
            scene_manager_next_scene(passy->scene_manager, PassySceneRead);
            consumed = true;
        } else if(event.event == SubmenuIndexReadDG2) {
            passy->read_type = PassyReadDG2;
            scene_manager_next_scene(passy->scene_manager, PassySceneRead);
            consumed = true;
        } else if(event.event == SubmenuIndexReadDG7) {
            passy->read_type = PassyReadDG7;
            scene_manager_next_scene(passy->scene_manager, PassySceneRead);
            consumed = true;
        }
    }

    return consumed;
}

void passy_scene_main_menu_on_exit(void* context) {
    Passy* passy = context;

    submenu_reset(passy->submenu);
}
