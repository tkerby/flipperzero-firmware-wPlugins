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

static void wmbus_scene_config_add_item(
    WmBusApp* app,
    const char* label,
    uint8_t count,
    uint8_t index,
    const char* text,
    VariableItemChangeCallback callback) {
    VariableItem* item = variable_item_list_add(app->config_list, label, count, callback, app);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, text);
}

void wmbus_scene_config_on_enter(void* context) {
    WmBusApp* app = context;
    if(!wmbus_app_ensure_config_view(app)) {
        return;
    }

    variable_item_list_set_enter_callback(
        app->config_list, wmbus_scene_config_enter_callback, app);
    wmbus_scene_config_add_item(
        app,
        "Mode",
        COUNT_OF(wmbus_mode_text),
        app->settings.mode,
        wmbus_mode_text[app->settings.mode],
        wmbus_scene_config_mode_changed);
    wmbus_scene_config_add_item(
        app,
        "CSV logging",
        WmBusCsvLoggingCount,
        app->settings.csv_logging,
        wmbus_csv_logging_text[app->settings.csv_logging],
        wmbus_scene_config_csv_logging_changed);
    wmbus_scene_config_add_item(
        app,
        "MEM gate >=",
        WmBusPacketQualityCount,
        app->settings.memory_quality,
        wmbus_packet_quality_short_str(app->settings.memory_quality),
        wmbus_scene_config_memory_quality_changed);
    wmbus_scene_config_add_item(
        app,
        "CSV gate >=",
        WmBusPacketQualityCount,
        app->settings.csv_quality,
        wmbus_packet_quality_short_str(app->settings.csv_quality),
        wmbus_scene_config_csv_quality_changed);
    uint8_t rssi_index = wmbus_min_rssi_index(app->settings.min_rssi_dbm);
    wmbus_scene_config_add_item(
        app,
        "RSSI gate >=",
        COUNT_OF(wmbus_min_rssi_options),
        rssi_index,
        wmbus_min_rssi_options[rssi_index].text,
        wmbus_scene_config_min_rssi_changed);
    wmbus_scene_config_add_item(
        app,
        "Debug overlay",
        COUNT_OF(wmbus_toggle_text),
        app->settings.debug_overlay,
        wmbus_toggle_text[app->settings.debug_overlay],
        wmbus_scene_config_debug_overlay_changed);
    wmbus_scene_config_add_item(app, "Keyring", 1U, 0U, app->keyring.status, NULL);
    wmbus_scene_config_add_item(app, "About", 1U, 0U, "", NULL);

    variable_item_list_set_selected_item(
        app->config_list, scene_manager_get_scene_state(app->scene_manager, WmBusSceneConfig));
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
    scene_manager_set_scene_state(
        app->scene_manager,
        WmBusSceneConfig,
        variable_item_list_get_selected_item_index(app->config_list));
    variable_item_list_reset(app->config_list);
}
