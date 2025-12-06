// kia_decoder_app.c
#include "kia_decoder_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include "protocols/protocol_items.h"

#define TAG "ProtoPirateApp"

static bool kia_decoder_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool kia_decoder_app_back_event_callback(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void kia_decoder_app_tick_event_callback(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

KiaDecoderApp* kia_decoder_app_alloc() {
    KiaDecoderApp* app = malloc(sizeof(KiaDecoderApp));

    FURI_LOG_I(TAG, "Allocating Kia Decoder App");

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&kia_decoder_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, kia_decoder_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, kia_decoder_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, kia_decoder_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Variable Item List
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        KiaDecoderViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KiaDecoderViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KiaDecoderViewWidget, widget_get_view(app->widget));

    // Receiver
    app->kia_receiver = kia_view_receiver_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        KiaDecoderViewReceiver,
        kia_view_receiver_get_view(app->kia_receiver));

    // Receiver Info
    app->kia_receiver_info = kia_view_receiver_info_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        KiaDecoderViewReceiverInfo,
        kia_view_receiver_info_get_view(app->kia_receiver_info));

    // Init setting
    app->setting = subghz_setting_alloc();
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

    // Init Worker & Protocol & History
    app->lock = KiaLockOff;
    app->txrx = malloc(sizeof(KiaDecoderTxRx));
    app->txrx->preset = malloc(sizeof(SubGhzRadioPreset));
    app->txrx->preset->name = furi_string_alloc();
    app->txrx->txrx_state = KiaTxRxStateIDLE;
    app->txrx->rx_key_state = KiaRxKeyStateIDLE;
    
    kia_preset_init(app, "AM650", subghz_setting_get_default_frequency(app->setting), NULL, 0);

    app->txrx->hopper_state = KiaHopperStateOFF;
    app->txrx->hopper_idx_frequency = 0;
    app->txrx->hopper_timeout = 0;
    app->txrx->idx_menu_chosen = 0;
    
    app->txrx->history = kia_history_alloc();
    app->txrx->worker = subghz_worker_alloc();
    
    // Create environment with our custom protocols
    app->txrx->environment = subghz_environment_alloc();
    
    FURI_LOG_I(TAG, "Registering %zu Kia protocols", kia_protocol_registry.size);
    subghz_environment_set_protocol_registry(
        app->txrx->environment, (void*)&kia_protocol_registry);
    
    // Create receiver
    app->txrx->receiver = subghz_receiver_alloc_init(app->txrx->environment);

    // Initialize SubGhz devices
    subghz_devices_init();

    // Use internal radio
    app->txrx->radio_device = radio_device_loader_set(
        app->txrx->radio_device, 
        SubGhzRadioDeviceTypeInternal);
    
    if(!app->txrx->radio_device) {
        FURI_LOG_E(TAG, "Failed to initialize radio device!");
    } else {
        FURI_LOG_I(TAG, "Radio device initialized");
    }

    subghz_devices_reset(app->txrx->radio_device);
    subghz_devices_idle(app->txrx->radio_device);

    // Set filter to accept decodable protocols
    subghz_receiver_set_filter(app->txrx->receiver, SubGhzProtocolFlag_Decodable);
    
    // Set up worker callbacks
    subghz_worker_set_overrun_callback(
        app->txrx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(
        app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(app->txrx->worker, app->txrx->receiver);

    furi_hal_power_suppress_charge_enter();

    scene_manager_next_scene(app->scene_manager, KiaDecoderSceneStart);

    return app;
}

void kia_decoder_app_free(KiaDecoderApp* app) {
    furi_assert(app);

    FURI_LOG_I(TAG, "Freeing Kia Decoder App");

    // Make sure we're not receiving
    if(app->txrx->txrx_state == KiaTxRxStateRx) {
        subghz_worker_stop(app->txrx->worker);
        subghz_devices_stop_async_rx(app->txrx->radio_device);
    }

    subghz_devices_sleep(app->txrx->radio_device);
    radio_device_loader_end(app->txrx->radio_device);

    subghz_devices_deinit();

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, KiaDecoderViewSubmenu);
    submenu_free(app->submenu);

    // Variable Item List
    view_dispatcher_remove_view(app->view_dispatcher, KiaDecoderViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    // Widget
    view_dispatcher_remove_view(app->view_dispatcher, KiaDecoderViewWidget);
    widget_free(app->widget);

    // Receiver
    view_dispatcher_remove_view(app->view_dispatcher, KiaDecoderViewReceiver);
    kia_view_receiver_free(app->kia_receiver);

    // Receiver Info
    view_dispatcher_remove_view(app->view_dispatcher, KiaDecoderViewReceiverInfo);
    kia_view_receiver_info_free(app->kia_receiver_info);

    // Setting
    subghz_setting_free(app->setting);

    // Worker & Protocol & History
    subghz_receiver_free(app->txrx->receiver);
    subghz_environment_free(app->txrx->environment);
    kia_history_free(app->txrx->history);
    subghz_worker_free(app->txrx->worker);
    furi_string_free(app->txrx->preset->name);
    free(app->txrx->preset);
    free(app->txrx);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    furi_hal_power_suppress_charge_exit();

    free(app);
}

int32_t kia_decoder_app(void* p) {
    UNUSED(p);
    KiaDecoderApp* kia_decoder_app = kia_decoder_app_alloc();

    view_dispatcher_run(kia_decoder_app->view_dispatcher);

    kia_decoder_app_free(kia_decoder_app);

    return 0;
}