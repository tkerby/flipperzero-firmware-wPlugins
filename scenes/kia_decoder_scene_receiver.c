// scenes/kia_decoder_scene_receiver.c
#include "../kia_decoder_app_i.h"

#define TAG "ProtoPirateSceneRx"

// Forward declaration
void kia_decoder_scene_receiver_view_callback(KiaCustomEvent event, void* context);

static void kia_decoder_scene_receiver_update_statusbar(void* context) {
    KiaDecoderApp* app = context;
    FuriString* frequency_str = furi_string_alloc();
    FuriString* modulation_str = furi_string_alloc();
    FuriString* history_stat_str = furi_string_alloc();

    kia_get_frequency_modulation(app, frequency_str, modulation_str);

    furi_string_printf(
        history_stat_str,
        "%u/%u",
        kia_history_get_item(app->txrx->history),
        kia_history_get_last_index(app->txrx->history));

    kia_view_receiver_add_data_statusbar(
        app->kia_receiver,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(modulation_str),
        furi_string_get_cstr(history_stat_str),
        false);

    furi_string_free(frequency_str);
    furi_string_free(modulation_str);
    furi_string_free(history_stat_str);
}

static void kia_decoder_scene_receiver_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    furi_assert(context);
    KiaDecoderApp* app = context;

    FURI_LOG_I(TAG, "=== SIGNAL DECODED ===");

    FuriString* str_buff = furi_string_alloc();
    subghz_protocol_decoder_base_get_string(decoder_base, str_buff);
    FURI_LOG_I(TAG, "%s", furi_string_get_cstr(str_buff));

    // Add to history
    if(kia_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset)) {
        FURI_LOG_I(TAG, "Added to history, total items: %u", 
                   kia_history_get_item(app->txrx->history));
        
        FuriString* item_name = furi_string_alloc();
        kia_history_get_text_item_menu(
            app->txrx->history,
            item_name,
            kia_history_get_item(app->txrx->history) - 1);

        kia_view_receiver_add_item_to_menu(
            app->kia_receiver,
            furi_string_get_cstr(item_name),
            0);

        furi_string_free(item_name);

        view_dispatcher_send_custom_event(
            app->view_dispatcher, KiaCustomEventSceneReceiverUpdate);
    } else {
        FURI_LOG_W(TAG, "Failed to add to history (duplicate or full)");
    }

    furi_string_free(str_buff);

    // Pause hopper when we receive something
    if(app->txrx->hopper_state == KiaHopperStateRunning) {
        app->txrx->hopper_state = KiaHopperStatePause;
        app->txrx->hopper_timeout = 10;
    }
}

void kia_decoder_scene_receiver_on_enter(void* context) {
    KiaDecoderApp* app = context;

    FURI_LOG_I(TAG, "=== ENTERING RECEIVER SCENE ===");
    FURI_LOG_I(TAG, "Frequency: %lu Hz", app->txrx->preset->frequency);
    FURI_LOG_I(TAG, "Modulation: %s", furi_string_get_cstr(app->txrx->preset->name));

    // Set up the receiver callback
    subghz_receiver_set_rx_callback(
        app->txrx->receiver,
        kia_decoder_scene_receiver_callback,
        app);

    // Set up view callback
    kia_view_receiver_set_callback(
        app->kia_receiver,
        kia_decoder_scene_receiver_view_callback,
        app);

    // Update status bar
    kia_decoder_scene_receiver_update_statusbar(app);

    // Start hopper if enabled
    if(app->txrx->hopper_state != KiaHopperStateOFF) {
        app->txrx->hopper_state = KiaHopperStateRunning;
    }

    // Get preset data
    const char* preset_name = furi_string_get_cstr(app->txrx->preset->name);
    uint8_t* preset_data = subghz_setting_get_preset_data_by_name(app->setting, preset_name);
    
    if(preset_data == NULL) {
        FURI_LOG_E(TAG, "Failed to get preset data for %s, using AM650", preset_name);
        preset_data = subghz_setting_get_preset_data_by_name(app->setting, "AM650");
    }

    // Begin receiving
    kia_begin(app, preset_data);

    uint32_t frequency = app->txrx->preset->frequency;
    if(app->txrx->hopper_state == KiaHopperStateRunning) {
        frequency = subghz_setting_get_hopper_frequency(app->setting, 0);
        app->txrx->hopper_idx_frequency = 0;
    }

    FURI_LOG_I(TAG, "Starting RX on %lu Hz", frequency);
    kia_rx(app, frequency);
    FURI_LOG_I(TAG, "RX started, state: %d", app->txrx->txrx_state);

    // Switch to receiver view
    view_dispatcher_switch_to_view(app->view_dispatcher, KiaDecoderViewReceiver);
}

bool kia_decoder_scene_receiver_on_event(void* context, SceneManagerEvent event) {
    KiaDecoderApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case KiaCustomEventSceneReceiverUpdate:
            kia_decoder_scene_receiver_update_statusbar(app);
            consumed = true;
            break;
            
        case KiaCustomEventViewReceiverOK:
            {
                uint16_t idx = kia_view_receiver_get_idx_menu(app->kia_receiver);
                FURI_LOG_I(TAG, "Selected item %d", idx);
                if(idx < kia_history_get_item(app->txrx->history)) {
                    app->txrx->idx_menu_chosen = idx;
                    scene_manager_next_scene(
                        app->scene_manager, KiaDecoderSceneReceiverInfo);
                }
            }
            consumed = true;
            break;
            
        case KiaCustomEventViewReceiverConfig:
            scene_manager_next_scene(
                app->scene_manager, KiaDecoderSceneReceiverConfig);
            consumed = true;
            break;
            
        case KiaCustomEventViewReceiverBack:
            if(app->txrx->txrx_state == KiaTxRxStateRx) {
                kia_rx_end(app);
            }
            kia_sleep(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, KiaDecoderSceneStart);
            consumed = true;
            break;
            
        case KiaCustomEventViewReceiverUnlock:
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update hopper
        if(app->txrx->hopper_state != KiaHopperStateOFF) {
            kia_hopper_update(app);
            kia_decoder_scene_receiver_update_statusbar(app);
        }

        // Update RSSI
        if(app->txrx->txrx_state == KiaTxRxStateRx) {
            float rssi = subghz_devices_get_rssi(app->txrx->radio_device);
            kia_view_receiver_set_rssi(app->kia_receiver, rssi);
        }

        consumed = true;
    }

    return consumed;
}

void kia_decoder_scene_receiver_on_exit(void* context) {
    KiaDecoderApp* app = context;

    FURI_LOG_I(TAG, "=== EXITING RECEIVER SCENE ===");

    if(app->txrx->txrx_state == KiaTxRxStateRx) {
        kia_rx_end(app);
    }
}

void kia_decoder_scene_receiver_view_callback(KiaCustomEvent event, void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}