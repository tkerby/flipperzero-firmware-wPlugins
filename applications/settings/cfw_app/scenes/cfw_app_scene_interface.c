#include "../cfw_app.h"

enum VarItemListIndex {
    VarItemListIndexCommon,
    VarItemListIndexDesktop,
    VarItemListIndexLockmenu,
    VarItemListIndexMainmenu,
    VarItemListIndexGamemenu,
    VarItemListIndexPassport,
};

void cfw_app_scene_interface_var_item_list_callback(void* context, uint32_t index) {
    CfwApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_on_enter(void* context) {
    CfwApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_add(var_item_list, "Browser", 0, NULL, app);
    variable_item_list_add(var_item_list, "Desktop", 0, NULL, app);
    variable_item_list_add(var_item_list, "Lock Menu", 0, NULL, app);
    variable_item_list_add(var_item_list, "Main Menu", 0, NULL, app);
    variable_item_list_add(var_item_list, "Game Menu", 0, NULL, app);
    variable_item_list_add(var_item_list, "Passport", 0, NULL, app);

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, CfwAppSceneInterface));

    view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewVarItemList);
}

bool cfw_app_scene_interface_on_event(void* context, SceneManagerEvent event) {
    CfwApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterface, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexCommon:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfaceCommon, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfaceCommon);
            break;
        case VarItemListIndexDesktop:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfaceDesktop, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfaceDesktop);
            break;
        case VarItemListIndexLockmenu:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfaceLockmenu, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfaceLockmenu);
            break;
        case VarItemListIndexMainmenu:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfaceMainmenu, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfaceMainmenu);
            break;
        case VarItemListIndexGamemenu:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfaceGamemenu, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfaceGamemenu);
            break;
        case VarItemListIndexPassport:
            scene_manager_set_scene_state(app->scene_manager, CfwAppSceneInterfacePassport, 0);
            scene_manager_next_scene(app->scene_manager, CfwAppSceneInterfacePassport);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_interface_on_exit(void* context) {
    CfwApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
