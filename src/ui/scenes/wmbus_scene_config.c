#include "../../app/wmbus_app_i.h"

enum WmBusConfigItem {
    WmBusConfigItemMode = 0,
    WmBusConfigItemCsvLogging,
    WmBusConfigItemMemoryQuality,
    WmBusConfigItemCsvQuality,
    WmBusConfigItemMinRssi,
    WmBusConfigItemDebugOverlay,
    WmBusConfigItemKeyring,
    WmBusConfigItemAbout,
};

static const char* const wmbus_mode_text[] = {
    "T",
    "C",
};

static const char* const wmbus_csv_logging_text[] = {
    "Off",
    "Basic",
    "Full",
};

static const char* const wmbus_toggle_text[] = {
    "Off",
    "On",
};

typedef struct {
    int32_t value;
    const char* text;
} WmBusRssiOption;

static const WmBusRssiOption wmbus_min_rssi_options[] = {
    {.value = 0, .text = "Off"},
    {.value = -100, .text = "-100 dBm"},
    {.value = -95, .text = "-95 dBm"},
    {.value = -90, .text = "-90 dBm"},
    {.value = -85, .text = "-85 dBm"},
    {.value = -80, .text = "-80 dBm"},
    {.value = -75, .text = "-75 dBm"},
    {.value = -70, .text = "-70 dBm"},
    {.value = -65, .text = "-65 dBm"},
    {.value = -60, .text = "-60 dBm"},
    {.value = -55, .text = "-55 dBm"},
    {.value = -50, .text = "-50 dBm"},
};

static uint8_t wmbus_min_rssi_index(int32_t min_rssi_dbm) {
    for(uint8_t i = 0; i < COUNT_OF(wmbus_min_rssi_options); i++) {
        if(wmbus_min_rssi_options[i].value == min_rssi_dbm) return i;
    }
    return 0U;
}

static void wmbus_scene_config_enter_callback(void* context, uint32_t index) {
    WmBusApp* app = context;

    if(index == WmBusConfigItemKeyring) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WmBusCustomEventConfigOpenKeyInput);
    } else if(index == WmBusConfigItemAbout) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WmBusCustomEventConfigOpenAbout);
    }
}

static void wmbus_scene_config_mode_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U) index = 0U;
    variable_item_set_current_value_text(item, wmbus_mode_text[index]);
    app->settings.mode = (index == 1U) ? WmBusRxModeC : WmBusRxModeT;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_csv_logging_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= WmBusCsvLoggingCount) index = WmBusCsvLoggingNone;
    variable_item_set_current_value_text(item, wmbus_csv_logging_text[index]);
    app->settings.csv_logging = (WmBusCsvLogging)index;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_quality_changed(VariableItem* item, WmBusPacketQuality* quality) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= WmBusPacketQualityCount) {
        index = 0U;
    }
    variable_item_set_current_value_text(
        item, wmbus_packet_quality_short_str((WmBusPacketQuality)index));
    *quality = (WmBusPacketQuality)index;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_memory_quality_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    wmbus_scene_config_quality_changed(item, &app->settings.memory_quality);
}

static void wmbus_scene_config_csv_quality_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    wmbus_scene_config_quality_changed(item, &app->settings.csv_quality);
}

static void wmbus_scene_config_min_rssi_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(wmbus_min_rssi_options)) {
        index = 0U;
    }
    variable_item_set_current_value_text(item, wmbus_min_rssi_options[index].text);
    app->settings.min_rssi_dbm = wmbus_min_rssi_options[index].value;
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_debug_overlay_changed(VariableItem* item) {
    WmBusApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U) index = 0U;
    variable_item_set_current_value_text(item, wmbus_toggle_text[index]);
    app->settings.debug_overlay = (index != 0U);
    wmbus_app_apply_runtime_config(app, true);
}

static void wmbus_scene_config_add_mode_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list, "Mode", COUNT_OF(wmbus_mode_text), wmbus_scene_config_mode_changed, app);
    uint8_t index = (app->settings.mode == WmBusRxModeC) ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_mode_text[index]);
}

static void wmbus_scene_config_add_csv_logging_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "CSV logging",
        WmBusCsvLoggingCount,
        wmbus_scene_config_csv_logging_changed,
        app);
    variable_item_set_current_value_index(item, app->settings.csv_logging);
    variable_item_set_current_value_text(item, wmbus_csv_logging_text[app->settings.csv_logging]);
}

static void wmbus_scene_config_add_quality_item(
    WmBusApp* app,
    const char* label,
    WmBusPacketQuality current,
    VariableItemChangeCallback callback) {
    VariableItem* item =
        variable_item_list_add(app->config_list, label, WmBusPacketQualityCount, callback, app);
    uint8_t index = (uint8_t)wmbus_packet_quality_clamp(current);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(
        item, wmbus_packet_quality_short_str((WmBusPacketQuality)index));
}

static void wmbus_scene_config_add_min_rssi_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "RSSI gate >=",
        COUNT_OF(wmbus_min_rssi_options),
        wmbus_scene_config_min_rssi_changed,
        app);
    uint8_t index = wmbus_min_rssi_index(app->settings.min_rssi_dbm);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_min_rssi_options[index].text);
}

static void wmbus_scene_config_add_debug_overlay_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(
        app->config_list,
        "Debug overlay",
        COUNT_OF(wmbus_toggle_text),
        wmbus_scene_config_debug_overlay_changed,
        app);
    uint8_t index = app->settings.debug_overlay ? 1U : 0U;
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, wmbus_toggle_text[index]);
}

static void wmbus_scene_config_add_keyring_item(WmBusApp* app) {
    VariableItem* item = variable_item_list_add(app->config_list, "Keyring", 1U, NULL, app);
    variable_item_set_current_value_text(item, app->keyring.status);
}

static void wmbus_scene_config_add_about_item(WmBusApp* app) {
    variable_item_list_add(app->config_list, "About", 1U, NULL, app);
}

void wmbus_scene_config_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_config_view(app)) {
        return;
    }

    variable_item_list_set_enter_callback(
        app->config_list, wmbus_scene_config_enter_callback, app);
    wmbus_scene_config_add_mode_item(app);
    wmbus_scene_config_add_csv_logging_item(app);
    wmbus_scene_config_add_quality_item(
        app,
        "MEM gate >=",
        app->settings.memory_quality,
        wmbus_scene_config_memory_quality_changed);
    wmbus_scene_config_add_quality_item(
        app, "CSV gate >=", app->settings.csv_quality, wmbus_scene_config_csv_quality_changed);
    wmbus_scene_config_add_min_rssi_item(app);
    wmbus_scene_config_add_debug_overlay_item(app);
    wmbus_scene_config_add_keyring_item(app);
    wmbus_scene_config_add_about_item(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewConfig);
}

bool wmbus_scene_config_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    switch(event.event) {
    case WmBusCustomEventConfigOpenKeyInput:
        scene_manager_next_scene(app->scene_manager, WmBusSceneKeyAdd);
        return true;
    case WmBusCustomEventConfigOpenAbout:
        scene_manager_next_scene(app->scene_manager, WmBusSceneAbout);
        return true;
    default:
        return false;
    }
}

void wmbus_scene_config_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->config_list) return;
    variable_item_list_set_selected_item(app->config_list, 0U);
    variable_item_list_reset(app->config_list);
}
