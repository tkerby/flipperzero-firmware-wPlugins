#include "ami_tool_i.h"

/* Local indexes for submenu items */
typedef enum {
    AmiToolMainMenuIndexRead,
    AmiToolMainMenuIndexGenerate,
    AmiToolMainMenuIndexSaved,
    AmiToolMainMenuIndexExit,
} AmiToolMainMenuIndex;

static void ami_tool_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;

    switch(index) {
    case AmiToolMainMenuIndexRead:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuRead);
        break;
    case AmiToolMainMenuIndexGenerate:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuGenerate);
        break;
    case AmiToolMainMenuIndexSaved:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuSaved);
        break;
    case AmiToolMainMenuIndexExit:
        view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventMainMenuExit);
        break;
    default:
        break;
    }
}

void ami_tool_scene_main_menu_on_enter(void* context) {
    AmiToolApp* app = context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Read",
        AmiToolMainMenuIndexRead,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Generate",
        AmiToolMainMenuIndexGenerate,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Saved",
        AmiToolMainMenuIndexSaved,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Exit",
        AmiToolMainMenuIndexExit,
        ami_tool_scene_main_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

bool ami_tool_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventMainMenuRead:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneRead);
            return true;
        case AmiToolEventMainMenuGenerate:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneGenerate);
            return true;
        case AmiToolEventMainMenuSaved:
            scene_manager_next_scene(app->scene_manager, AmiToolSceneSaved);
            return true;
        case AmiToolEventMainMenuExit:
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Back from main menu exits the app */
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

void ami_tool_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
    /* Nothing to clean up */
}
