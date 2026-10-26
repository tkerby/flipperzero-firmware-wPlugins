#include "wmbus_app_i.h"

#include <furi.h>
#include <string.h>
#include <notification/notification_messages.h>
#include "../storage/wmbus_log.h"

#define TAG "WmBusDecoder"
static void wmbus_app_free(WmBusApp* app);

static const NotificationSequence wmbus_packet_stored_sequence = {
    &message_note_c7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

/* Runs on the processing worker; storage must never delay radio FIFO service. */
static void wmbus_app_handle_packet(
    void* context,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record) {
    WmBusApp* app = context;
    bool store = wmbus_packet_record_passes_policy(
        record, settings->memory_quality, settings->min_rssi_dbm);
    bool rssi_ok = settings->min_rssi_dbm >= 0 || record->rssi >= settings->min_rssi_dbm;
    wmbus_rx_view_push_packet(app->rx_view, record, rssi_ok, store);
    if(store) notification_message(app->notification, &wmbus_packet_stored_sequence);
    if(settings->csv_logging != WmBusCsvLoggingNone &&
       wmbus_packet_record_passes_policy(record, settings->csv_quality, settings->min_rssi_dbm) &&
       !wmbus_log_append(app->storage, settings->csv_logging, record)) {
        FURI_LOG_W(TAG, "CSV write failed");
    }
}

static void wmbus_app_handle_frame(void* context, const WmBusPhyFrame* phy_frame) {
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
    bool processor_applied = !app->capture_processor ||
                             wmbus_capture_processor_apply_config(
                                 app->capture_processor, &app->settings, &app->keyring.key_store);
    bool radio_applied = !app->rx_service ||
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

    app->config_list = variable_item_list_alloc();
    if(!app->config_list) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewConfig, variable_item_list_get_view(app->config_list));
    return true;
}

bool wmbus_app_ensure_key_input_view(WmBusApp* app) {
    if(!app) return false;
    if(app->key_input) return true;

    app->key_input = byte_input_alloc();
    if(!app->key_input) return false;

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewKeyInput, byte_input_get_view(app->key_input));
    return true;
}

bool wmbus_app_ensure_widget(WmBusApp* app) {
    if(app->widget) return true;
    app->widget = widget_alloc();
    if(!app->widget) return false;
    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewWidget, widget_get_view(app->widget));
    return true;
}

static WmBusApp* wmbus_app_alloc(void) {
    WmBusApp* app = malloc(sizeof(WmBusApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    wmbus_settings_load(app->storage, &app->settings);
    wmbus_keyring_load(app->storage, &app->keyring);

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        wmbus_app_free(app);
        return NULL;
    }
    app->scene_manager = scene_manager_alloc(&wmbus_scene_handlers, app);
    if(!app->scene_manager) {
        wmbus_app_free(app);
        return NULL;
    }
    app->rx_view = wmbus_rx_view_alloc();
    if(!app->rx_view) {
        wmbus_app_free(app);
        return NULL;
    }

    wmbus_rx_view_set_dispatcher(app->rx_view, app->view_dispatcher);
    wmbus_rx_view_apply_settings(app->rx_view, &app->settings);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, wmbus_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wmbus_app_back_event_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, WmBusAppViewRx, wmbus_rx_view_get_view(app->rx_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, WmBusSceneRx);
    app->capture_processor = wmbus_capture_processor_alloc(
        &app->settings, &app->keyring.key_store, wmbus_app_handle_packet, app);
    if(!app->capture_processor) {
        wmbus_app_free(app);
        return NULL;
    }
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
    return app;
}

static void wmbus_app_remove_view(WmBusApp* app, WmBusAppView view_id) {
    if(app && app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, view_id);
    }
}

static void wmbus_app_free(WmBusApp* app) {
    if(!app) return;

    wmbus_radio_rx_service_free(app->rx_service);
    wmbus_capture_processor_free(app->capture_processor);

    if(app->widget) {
        wmbus_app_remove_view(app, WmBusAppViewWidget);
        widget_free(app->widget);
    }
    if(app->config_list) {
        wmbus_app_remove_view(app, WmBusAppViewConfig);
        variable_item_list_free(app->config_list);
    }
    if(app->key_input) {
        wmbus_app_remove_view(app, WmBusAppViewKeyInput);
        byte_input_free(app->key_input);
    }
    if(app->rx_view) {
        wmbus_app_remove_view(app, WmBusAppViewRx);
        wmbus_rx_view_free(app->rx_view);
    }
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    if(app->scene_manager) scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
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
