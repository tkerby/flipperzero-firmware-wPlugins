#include "../usb_can_bridge.hpp"
#include "../usb_can_app_i.hpp"
#include <furi_hal.h>

typedef enum { UsbUartLineIndexVcp } LineIndex;

static const char* vcp_ch[] = {"0 (CLI)", "1"};
static UsbCanConfig* SceneConfigToSet;
bool usb_can_scene_config_on_event(void* context, SceneManagerEvent event) {
    UsbCanApp* app = (UsbCanApp*)context;
    furi_assert(app);
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == UsbCanCustomEventConfig) {
            usb_can_set_config(app->usb_can_bridge, SceneConfigToSet);
            return true;
        }
    }
    return false;
}

static void line_vcp_cb(VariableItem* item) {
    UsbCanApp* app = (UsbCanApp*)variable_item_get_context(item);
    furi_assert(app);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, vcp_ch[index]);

    SceneConfigToSet->vcp_ch = index;
    view_dispatcher_send_custom_event(app->views.view_dispatcher, UsbCanCustomEventConfig);
}

void usb_can_scene_config_on_enter(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    furi_assert(app);
    VariableItemList* var_item_list = app->views.menu;

    SceneConfigToSet = (UsbCanConfig*)malloc(sizeof(UsbCanConfig));
    usb_can_get_config(app->usb_can_bridge, SceneConfigToSet);

    VariableItem* item;

    item = variable_item_list_add(var_item_list, "USB Channel", 2, line_vcp_cb, app);
    variable_item_set_current_value_index(item, SceneConfigToSet->vcp_ch);
    variable_item_set_current_value_text(item, vcp_ch[SceneConfigToSet->vcp_ch]);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, UsbCanSceneConfig));

    view_dispatcher_switch_to_view(app->views.view_dispatcher, UsbCanAppViewMenu);
}

void usb_can_scene_config_on_exit(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    scene_manager_set_scene_state(
        app->scene_manager,
        UsbCanSceneConfig,
        variable_item_list_get_selected_item_index(app->views.menu));
    variable_item_list_reset(app->views.menu);
    free(SceneConfigToSet);
}
