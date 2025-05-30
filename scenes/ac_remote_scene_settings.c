#include "../ac_remote_app_i.h"

#define TAG "ACRemoteSettings"

// TODO Settings menu
// Should have a VIL that includes: Side, Timer step, Allow auto mode, Reset settings -> Dialogue
// Check the weather_station good fap for more details on how to use VIL.
void ac_remote_scene_settings_on_enter(void* context) {
    AC_RemoteApp* app = context;
    VariableItemList* vil_settings = app->vil_settings;
    variable_item_list_add(vil_settings, "Test", 1, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, AC_RemoteAppViewSettings);
}

bool ac_remote_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ac_remote_scene_settings_on_exit(void* context) {
    AC_RemoteApp* app = context;
    variable_item_list_set_selected_item(app->vil_settings, 0);
    variable_item_list_reset(app->vil_settings);
}
