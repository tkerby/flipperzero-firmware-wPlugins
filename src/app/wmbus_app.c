#include "wmbus_app_i.h"

#include <furi.h>
#include <string.h>

#define TAG "WmBusDecoder"
static void wmbus_app_free(WmBusApp* app);

static void wmbus_app_log_step(const char* step) {
    if(step) {
        FURI_LOG_D(TAG, "app init: %s", step);
    }
}

static void wmbus_app_handle_frame(
    void* context,
    const WmBusPhyFrame* phy_frame) {
    WmBusApp* app = context;
    if(!app || !app->capture_processor) {
        return;
    }

    wmbus_capture_processor_submit_frame(app->capture_processor, phy_frame);
}

static void wmbus_app_set_freq_valid(void* context, bool freq_valid) {
    WmBusApp* app = context;
    if(!app || !app->rx_view) {
        return;
    }

    wmbus_rx_view_set_freq_valid(app->rx_view, freq_valid);
}

static void wmbus_app_set_live_rssi(void* context, int rssi) {
    WmBusApp* app = context;
    if(!app || !app->rx_view) {
        return;
    }

    wmbus_rx_view_set_live_rssi(app->rx_view, rssi);
}

static bool wmbus_app_custom_event_callback(void* context, uint32_t event) {
    WmBusApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wmbus_app_back_event_callback(void* context) {
    WmBusApp* app = context;
    if(!scene_manager_handle_back_event(app->scene_manager)) {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

bool wmbus_app_apply_runtime_config(WmBusApp* app, bool persist) {
    if(!app) return false;

    wmbus_rx_view_apply_settings(app->rx_view, &app->settings);
    bool saved = !persist || wmbus_settings_save(app->storage, &app->settings);
    bool processor_applied =
        !app->capture_processor || wmbus_capture_processor_apply_config(
                                       app->capture_processor,
                                       &app->settings,
                                       &app->keyring.key_store);
    bool radio_applied =
        !app->rx_service ||
        wmbus_radio_rx_service_apply_mode(app->rx_service, app->settings.mode);
    return saved && processor_applied && radio_applied;
}

bool wmbus_app_add_key(WmBusApp* app, const uint8_t key[WMBUS_KEY_BYTES]) {
    if(!app || !key) return false;
    bool added = wmbus_keyring_append(app->storage, &app->keyring, key);
    if(added) {
        wmbus_app_apply_runtime_config(app, false);
    }
    return added;
}

bool wmbus_app_ensure_config_view(WmBusApp* app) {
    if(!app) return false;
    if(app->config_list) return true;

    wmbus_app_log_step("alloc config view");
    app->config_list = variable_item_list_alloc();
    if(!app->config_list) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewConfig, variable_item_list_get_view(app->config_list));
    return true;
}

bool wmbus_app_ensure_key_input_view(WmBusApp* app) {
    if(!app) return false;
    if(app->key_input) return true;

    wmbus_app_log_step("alloc key input view");
    app->key_input = byte_input_alloc();
    if(!app->key_input) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewKeyInput, byte_input_get_view(app->key_input));
    return true;
}

bool wmbus_app_ensure_detail_view(WmBusApp* app) {
    if(!app) return false;
    if(app->detail_widget) return true;

    wmbus_app_log_step("alloc detail view");
    app->detail_widget = widget_alloc();
    if(!app->detail_widget) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewPacketDetail, widget_get_view(app->detail_widget));
    return true;
}

bool wmbus_app_ensure_about_view(WmBusApp* app) {
    if(!app) return false;
    if(app->about_widget) return true;

    wmbus_app_log_step("alloc about view");
    app->about_widget = widget_alloc();
    if(!app->about_widget) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewAbout, widget_get_view(app->about_widget));
    return true;
}

static WmBusApp* wmbus_app_alloc(void) {
    WmBusApp* app = malloc(sizeof(WmBusApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));

    wmbus_app_log_step("open records");
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    wmbus_app_log_step("load settings");
    wmbus_settings_reset(&app->settings);
    wmbus_settings_load(app->storage, &app->settings);
    wmbus_app_log_step("load keyring");
    wmbus_keyring_load(app->storage, &app->keyring);

    wmbus_app_log_step("alloc core");
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wmbus_scene_handlers, app);
    wmbus_app_log_step("alloc rx view");
    app->rx_view = wmbus_rx_view_alloc();

    wmbus_rx_view_set_dispatcher(app->rx_view, app->view_dispatcher);
    wmbus_rx_view_apply_settings(app->rx_view, &app->settings);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wmbus_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wmbus_app_back_event_callback);

    wmbus_app_log_step("add rx view");
    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewRx, wmbus_rx_view_get_view(app->rx_view));

    wmbus_app_log_step("attach gui");
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    wmbus_app_log_step("enter rx scene");
    scene_manager_next_scene(app->scene_manager, WmBusSceneRx);
    wmbus_app_log_step("alloc capture processor");
    app->capture_processor =
        wmbus_capture_processor_alloc(&app->settings, &app->keyring.key_store);
    if(!app->capture_processor) {
        wmbus_app_free(app);
        return NULL;
    }
    wmbus_csv_sink_init(&app->csv_sink, app->storage);
    wmbus_history_sink_init(&app->history_sink, app->rx_view);
    if(!wmbus_capture_processor_add_sink(app->capture_processor, &app->csv_sink.sink) ||
       !wmbus_capture_processor_add_sink(app->capture_processor, &app->history_sink.sink) ||
       !wmbus_capture_processor_start(app->capture_processor)) {
        wmbus_app_free(app);
        return NULL;
    }
    wmbus_app_log_step("start rx service");
    WmBusRadioRxCallbacks rx_callbacks = {
        .context = app,
        .handle_frame = wmbus_app_handle_frame,
        .set_freq_valid = wmbus_app_set_freq_valid,
        .set_live_rssi = wmbus_app_set_live_rssi,
    };
    app->rx_service = wmbus_radio_rx_service_alloc(&rx_callbacks, app->settings.mode);
    if(!app->rx_service) {
        wmbus_app_free(app);
        return NULL;
    }
    wmbus_app_log_step("init done");
    return app;
}

static void wmbus_app_free(WmBusApp* app) {
    if(!app) return;

    wmbus_radio_rx_service_free(app->rx_service);
    wmbus_capture_processor_free(app->capture_processor);

    if(app->about_widget) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewAbout);
        widget_free(app->about_widget);
    }
    if(app->detail_widget) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewPacketDetail);
        widget_free(app->detail_widget);
    }
    if(app->config_list) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewConfig);
        variable_item_list_free(app->config_list);
    }
    if(app->key_input) {
        view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewKeyInput);
        byte_input_free(app->key_input);
    }
    view_dispatcher_remove_view(app->view_dispatcher, WmBusAppViewRx);
    wmbus_rx_view_free(app->rx_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t wmbus_app(void) {
    WmBusApp* app = wmbus_app_alloc();
    if(!app) {
        return -1;
    }
    view_dispatcher_run(app->view_dispatcher);
    wmbus_app_free(app);
    return 0;
}
