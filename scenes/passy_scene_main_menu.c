#include "../passy_i.h"

#define TAG "SceneMainMenu"

enum SubmenuIndex {
    SubmenuIndexRead,
    SubmenuIndexDeleteMRZInfo,
};

void passy_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    Passy* passy = context;
    view_dispatcher_send_custom_event(passy->view_dispatcher, index);
}

void passy_scene_main_menu_on_enter(void* context) {
    Passy* passy = context;
    Submenu* submenu = passy->submenu;
    submenu_reset(submenu);

    submenu_add_item(
        submenu, "Read", SubmenuIndexRead, passy_scene_main_menu_submenu_callback, passy);
    if(strlen(passy->passport_number) > 0 && strlen(passy->date_of_birth) > 0 &&
       strlen(passy->date_of_expiry) > 0) {
        submenu_add_item(
            submenu,
            "Delete MRZ Info",
            SubmenuIndexDeleteMRZInfo,
            passy_scene_main_menu_submenu_callback,
            passy);
    }

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewMenu);
}

bool passy_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexRead) {
            scene_manager_set_scene_state(
                passy->scene_manager, PassySceneMainMenu, SubmenuIndexRead);

            if(strlen(passy->passport_number) > 0 && strlen(passy->date_of_birth) > 0 &&
               strlen(passy->date_of_expiry) > 0) {
                scene_manager_next_scene(passy->scene_manager, PassySceneRead);
            } else {
                scene_manager_next_scene(passy->scene_manager, PassyScenePassportNumberInput);
            }
            consumed = true;
        } else if(event.event == SubmenuIndexDeleteMRZInfo) {
            scene_manager_set_scene_state(
                passy->scene_manager, PassySceneMainMenu, SubmenuIndexDeleteMRZInfo);
            scene_manager_next_scene(passy->scene_manager, PassySceneDelete);
            consumed = true;
        }
    }

    return consumed;
}

void passy_scene_main_menu_on_exit(void* context) {
    Passy* passy = context;

    submenu_reset(passy->submenu);
}
