#include "../cfw_app.h"

enum VarItemListIndex {
    // VarItemListIndexBadkbMode,
    // VarItemListIndexBadbtRemember,
    VarItemListIndexSubghzFrequencies,
    VarItemListIndexSubghzExtend,
};

void cfw_app_scene_protocols_var_item_list_callback(void* context, uint32_t index) {
    CfwApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// static void cfw_app_scene_protocols_badkb_mode_changed(VariableItem* item) {
    // CfwApp* app = variable_item_get_context(item);
    // bool value = variable_item_get_current_value_index(item);
    // variable_item_set_current_value_text(item, value ? "BT" : "USB");
    // CFW_SETTINGS()->bad_bt = value;
    // app->save_settings = true;
// }

// static void cfw_app_scene_protocols_badbt_remember_changed(VariableItem* item) {
    // CfwApp* app = variable_item_get_context(item);
    // bool value = variable_item_get_current_value_index(item);
    // variable_item_set_current_value_text(item, value ? "ON" : "OFF");
    // CFW_SETTINGS()->bad_bt_remember = value;
    // app->save_settings = true;
// }

static void cfw_app_scene_protocols_subghz_extend_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    app->subghz_extend = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");
    app->save_subghz = true;
}

void cfw_app_scene_protocols_on_enter(void* context) {
    CfwApp* app = context;
    CfwSettings* cfw_settings = CFW_SETTINGS();
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    // item = variable_item_list_add(
        // var_item_list, "BadKB Mode", 2, cfw_app_scene_protocols_badkb_mode_changed, app);
    // variable_item_set_current_value_index(item, cfw_settings->bad_bt);
    // variable_item_set_current_value_text(item, cfw_settings->bad_bt ? "BT" : "USB");

    // item = variable_item_list_add(
        // var_item_list, "BadBT Remember", 2, cfw_app_scene_protocols_badbt_remember_changed, app);
    // variable_item_set_current_value_index(item, cfw_settings->bad_bt_remember);
    // variable_item_set_current_value_text(item, cfw_settings->bad_bt_remember ? "ON" : "OFF");

    variable_item_list_add(var_item_list, "SubGHz Frequencies", 0, NULL, app);

    item = variable_item_list_add(
        var_item_list, "SubGHz Extend", 2, cfw_app_scene_protocols_subghz_extend_changed, app);
    variable_item_set_current_value_index(item, app->subghz_extend);
    variable_item_set_current_value_text(item, app->subghz_extend ? "ON" : "OFF");

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_protocols_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, CfwAppSceneProtocols));

    view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewVarItemList);
}

bool cfw_app_scene_protocols_on_event(void* context, SceneManagerEvent event) {
    CfwApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CfwAppSceneProtocols, event.event);
        consumed = true;
        switch(event.event) {
        case VarItemListIndexSubghzFrequencies:
            scene_manager_next_scene(app->scene_manager, CfwAppSceneProtocolsFrequencies);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_protocols_on_exit(void* context) {
    CfwApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
