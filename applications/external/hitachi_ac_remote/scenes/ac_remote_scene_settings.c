#include "../ac_remote_app_i.h"

#define TAG "ACRemoteSettings"

static const char* const SIDE_LABEL_TEXT[SETTINGS_SIDE_COUNT] = {"A", "B"};
static const char* const TIMER_STEP_TEXT[SETTINGS_TIMER_STEP_COUNT] = {
    "1min",
    "2min",
    "3min",
    "5min",
    "10min",
    "15min",
    "30min",
};

static const char* const BOOLEAN_TEXT[2] = {
    "No",
    "Yes",
};

static void on_change_side(VariableItem* item) {
    AC_RemoteApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->app_state.side = index;
    variable_item_set_current_value_text(item, SIDE_LABEL_TEXT[app->app_state.side]);
}

static void on_change_timer_step(VariableItem* item) {
    AC_RemoteApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->app_state.timer_step = index;
    variable_item_set_current_value_text(item, TIMER_STEP_TEXT[app->app_state.timer_step]);
}

static void on_change_allow_auto(VariableItem* item) {
    AC_RemoteApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->app_state.allow_auto = !!index;
    variable_item_set_current_value_text(item, BOOLEAN_TEXT[app->app_state.allow_auto]);
}

// TODO Settings menu
// Should have a VIL that includes: Side, Timer step, Allow auto mode, Reset settings -> Dialogue
// Check the weather_station good fap for more details on how to use VIL.
void ac_remote_scene_settings_on_enter(void* context) {
    AC_RemoteApp* app = context;
    VariableItemList* vil_settings = app->vil_settings;
    VariableItem* item;

    item = variable_item_list_add(vil_settings, "Side", SETTINGS_SIDE_COUNT, on_change_side, app);
    variable_item_set_current_value_index(item, app->app_state.side);
    variable_item_set_current_value_text(item, SIDE_LABEL_TEXT[app->app_state.side]);

    item = variable_item_list_add(
        vil_settings, "Timer step", SETTINGS_TIMER_STEP_COUNT, on_change_timer_step, app);
    variable_item_set_current_value_index(item, app->app_state.timer_step);
    variable_item_set_current_value_text(item, TIMER_STEP_TEXT[app->app_state.timer_step]);

    item = variable_item_list_add(vil_settings, "Allow auto", 2, on_change_allow_auto, app);
    variable_item_set_current_value_index(item, app->app_state.allow_auto);
    variable_item_set_current_value_text(item, BOOLEAN_TEXT[app->app_state.allow_auto]);

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
