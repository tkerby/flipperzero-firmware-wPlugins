// scenes/protopirate_scene_emulate.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "../protocols/protocol_items.h"

#define TAG "ProtoPirateEmulate"

typedef struct {
    uint32_t original_counter;
    uint32_t current_counter;
    uint32_t serial;
    uint8_t original_button;
    FuriString* protocol_name;
    FlipperFormat* flipper_format;
    SubGhzTransmitter* transmitter;
    bool is_transmitting;
} EmulateContext;

static EmulateContext* emulate_context = NULL;

static const char* protopirate_emulate_get_short_preset_name(const char* preset_name) {
    if(!preset_name) return "AM650";

    if(strstr(preset_name, "Ook270") || strcmp(preset_name, "AM270") == 0) {
        return "AM270";
    } else if(strstr(preset_name, "Ook650") || strcmp(preset_name, "AM650") == 0) {
        return "AM650";
    } else if(strstr(preset_name, "2FSKDev238") || strcmp(preset_name, "FM238") == 0) {
        return "FM238";
#if defined(FW_ORIGIN_Unleashed) || defined(FW_ORIGIN_RM)
    } else if(strstr(preset_name, "2FSKDev12K") || strcmp(preset_name, "FM12K") == 0) {
        return "FM12K";
#endif
    } else if(strstr(preset_name, "2FSKDev476") || strcmp(preset_name, "FM476") == 0) {
        return "FM476";
    } else if(strstr(preset_name, "Custom") || strcmp(preset_name, "CUSTOM") == 0) {
        return "CUSTOM";
    }

    // If it's already a short name or custom preset, return as-is
    return preset_name;
}

static uint8_t
    protopirate_get_button_for_protocol(const char* protocol, InputKey key, uint8_t original) {
    // Kia/Hyundai (all versions)
    if(strstr(protocol, "Kia")) {
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock
        case InputKeyOk:
            return 0x2; // Unlock
        case InputKeyDown:
            return 0x3; // Trunk
        case InputKeyLeft:
            return 0x4; // Panic
        case InputKeyRight:
            return 0x8; // Horn/Lights?
        default:
            return original;
        }
    }
    // VW
    else if(strstr(protocol, "VW")) {
        switch(key) {
        case InputKeyUp:
            return 0x2; // Lock
        case InputKeyOk:
            return 0x1; // Unlock
        case InputKeyDown:
            return 0x4; // Trunk
        case InputKeyLeft:
            return 0x8; // Panic
        case InputKeyRight:
            return 0x3; // Un+Lk combo
        default:
            return original;
        }
    }
    // Suzuki
    else if(strstr(protocol, "Suzuki")) {
        switch(key) {
        case InputKeyUp:
            return 0x3; // Lock
        case InputKeyOk:
            return 0x4; // Unlock
        case InputKeyDown:
            return 0x2; // Trunk
        case InputKeyLeft:
            return 0x1; // Panic
        case InputKeyRight:
            return original;
        default:
            return original;
        }
    }
    // Ford - (needs testing)
    else if(strstr(protocol, "Ford")) {
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock?
        case InputKeyOk:
            return 0x2; // Unlock?
        case InputKeyDown:
            return 0x4; // Trunk?
        case InputKeyLeft:
            return 0x8; // Panic?
        case InputKeyRight:
            return 0x3; // ?
        default:
            return original;
        }
    }
    // Subaru - (needs testing)
    else if(strstr(protocol, "Subaru")) {
        switch(key) {
        case InputKeyUp:
            return 0x1; // Lock?
        case InputKeyOk:
            return 0x2; // Unlock?
        case InputKeyDown:
            return 0x3; // Trunk?
        case InputKeyLeft:
            return 0x4; // Panic?
        case InputKeyRight:
            return 0x8; // ?
        default:
            return original;
        }
    }

    return original;
}

static bool protopirate_emulate_update_data(EmulateContext* ctx, uint8_t button) {
    if(!ctx || !ctx->flipper_format) return false;

    // Update button and counter in the flipper format
    flipper_format_rewind(ctx->flipper_format);

    // Update button
    uint32_t btn_value = button;
    flipper_format_insert_or_update_uint32(ctx->flipper_format, "Btn", &btn_value, 1);
    FURI_LOG_I(TAG, "Updated flipper format - Btn: 0x%02X", button);

    flipper_format_insert_or_update_uint32(ctx->flipper_format, "Cnt", &ctx->current_counter, 1);
    FURI_LOG_I(TAG, "Updated flipper format - Cnt: 0x%03lX", (unsigned long)ctx->current_counter);

    return true;
}

static void protopirate_emulate_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    if(!emulate_context) return;

    static uint8_t animation_frame = 0;
    animation_frame = (animation_frame + 1) % 8;

    canvas_clear(canvas);

    // Header bar
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontSecondary);
    const char* proto_name = furi_string_get_cstr(emulate_context->protocol_name);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, proto_name);
    canvas_invert_color(canvas);

    // Info section
    canvas_set_font(canvas, FontSecondary);

    // Serial - left aligned
    char info_str[32];
    snprintf(info_str, sizeof(info_str), "SN:%08lX", emulate_context->serial);
    canvas_draw_str(canvas, 2, 20, info_str);

    // Counter - left aligned
    snprintf(info_str, sizeof(info_str), "CNT:%04lX", emulate_context->current_counter);
    canvas_draw_str(canvas, 2, 30, info_str);

    // Increment on right if changed
    if(emulate_context->current_counter > emulate_context->original_counter) {
        snprintf(
            info_str,
            sizeof(info_str),
            "+%ld",
            emulate_context->current_counter - emulate_context->original_counter);
        canvas_draw_str(canvas, 60, 30, info_str);
    }

    // Divider
    canvas_draw_line(canvas, 0, 35, 127, 35);

    // Button mapping - adjusted positioning
    canvas_set_font(canvas, FontSecondary);

    // Row 1
    canvas_draw_str(canvas, 2, 44, "^Lock");
    canvas_draw_str(canvas, 95, 44, "<Panic");

    // Row 2
    canvas_draw_str(canvas, 2, 53, "vTrunk");
    canvas_draw_str(canvas, 95, 53, ">0x8");

    // OK at bottom center
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[OK]Unlock");

    // Transmitting overlay
    if(emulate_context->is_transmitting) {
        // TX box
        canvas_draw_rbox(canvas, 24, 18, 80, 18, 3);
        canvas_invert_color(canvas);

        // Waves
        int wave = animation_frame % 3;
        canvas_draw_str(canvas, 28 + wave * 2, 25, ")))");

        // Text
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "TX");

        canvas_invert_color(canvas);
    }
}

static bool protopirate_emulate_input_callback(InputEvent* event, void* context) {
    ProtoPirateApp* app = context;
    EmulateContext* ctx = emulate_context;

    if(!ctx) return false;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyBack) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventEmulateExit);
            return true;
        }

        // Get button mapping for this key
        uint8_t button = protopirate_get_button_for_protocol(
            furi_string_get_cstr(ctx->protocol_name), event->key, ctx->original_button);

        // Update data with new button and counter
        ctx->current_counter++;
        protopirate_emulate_update_data(ctx, button);

        // Start transmission - user can hold as long as they want
        ctx->is_transmitting = true;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventEmulateTransmit);

        return true;
    } else if(event->type == InputTypeRelease) {
        // Stop transmission immediately on release - simple behavior like main SubGhz app
        if(ctx && ctx->is_transmitting) {
            ctx->is_transmitting = false;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventEmulateStop);
            return true;
        }
        return false;
    }

    return false;
}

void protopirate_scene_emulate_on_enter(void* context) {
    ProtoPirateApp* app = context;

    // Create emulate context
    emulate_context = malloc(sizeof(EmulateContext));
    memset(emulate_context, 0, sizeof(EmulateContext));

    emulate_context->protocol_name = furi_string_alloc();

    // Load the file
    if(app->loaded_file_path) {
        FlipperFormat* ff =
            protopirate_storage_load_file(furi_string_get_cstr(app->loaded_file_path));

        if(ff) {
            emulate_context->flipper_format = ff;

            // Read protocol name
            flipper_format_rewind(ff);
            if(!flipper_format_read_string(ff, "Protocol", emulate_context->protocol_name)) {
                FURI_LOG_E(TAG, "Failed to read protocol name");
            }

            // Read serial
            flipper_format_rewind(ff);
            if(!flipper_format_read_uint32(ff, "Serial", &emulate_context->serial, 1)) {
                FURI_LOG_W(TAG, "Failed to read serial");
            }

            // Read original button
            flipper_format_rewind(ff);
            uint32_t btn_temp;
            if(flipper_format_read_uint32(ff, "Btn", &btn_temp, 1)) {
                emulate_context->original_button = (uint8_t)btn_temp;
            }

            // Read counter
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Cnt", &emulate_context->original_counter, 1)) {
                emulate_context->current_counter = emulate_context->original_counter;
            }

            // Set up transmitter based on protocol
            const char* proto_name = furi_string_get_cstr(emulate_context->protocol_name);
            FURI_LOG_I(TAG, "Setting up transmitter for protocol: %s", proto_name);

            // Find the protocol in the registry - check for version-specific names too
            const SubGhzProtocol* protocol = NULL;
            const char* matched_name = NULL; // Store the actual registry name
            for(size_t i = 0; i < protopirate_protocol_registry.size; i++) {
                const char* reg_name = protopirate_protocol_registry.items[i]->name;
                // Match exact name OR if it's a V3/V4 variant
                if(strcmp(reg_name, proto_name) == 0 ||
                   (strcmp(proto_name, "Kia V3") == 0 && strcmp(reg_name, "Kia V3/V4") == 0) ||
                   (strcmp(proto_name, "Kia V4") == 0 && strcmp(reg_name, "Kia V3/V4") == 0)) {
                    protocol = protopirate_protocol_registry.items[i];
                    matched_name = reg_name; // Save the actual registry name
                    FURI_LOG_I(
                        TAG,
                        "Found protocol %s in registry at index %zu (matched %s)",
                        proto_name,
                        i,
                        reg_name);
                    break;
                }
            }

            if(protocol) {
                if(protocol->encoder && protocol->encoder->alloc) {
                    FURI_LOG_I(TAG, "Protocol has encoder support");
                    FURI_LOG_I(TAG, "Encoder alloc function at: %p", protocol->encoder->alloc);
                    FURI_LOG_I(TAG, "Encoder free function at: %p", protocol->encoder->free);
                    FURI_LOG_I(
                        TAG,
                        "Encoder deserialize function at: %p",
                        protocol->encoder->deserialize);
                    FURI_LOG_I(TAG, "Encoder yield function at: %p", protocol->encoder->yield);

                    // Make sure the protocol registry is set in the environment
                    subghz_environment_set_protocol_registry(
                        app->txrx->environment, &protopirate_protocol_registry);

                    // Use matched_name (the actual registry name) instead of proto_name
                    emulate_context->transmitter =
                        subghz_transmitter_alloc_init(app->txrx->environment, matched_name);

                    if(emulate_context->transmitter) {
                        FURI_LOG_I(
                            TAG,
                            "Transmitter allocated successfully at %p",
                            emulate_context->transmitter);

                        // Deserialize for transmission
                        flipper_format_rewind(ff);
                        SubGhzProtocolStatus status =
                            subghz_transmitter_deserialize(emulate_context->transmitter, ff);

                        if(status == SubGhzProtocolStatusOk) {
                            FURI_LOG_I(TAG, "Transmitter deserialized successfully");

                            // Test: Try to get a few samples from the transmitter
                            FURI_LOG_I(TAG, "Testing transmitter yield function...");
                            for(int i = 0; i < 10; i++) {
                                LevelDuration ld =
                                    subghz_transmitter_yield(emulate_context->transmitter);
                                if(!level_duration_is_reset(ld)) {
                                    FURI_LOG_I(
                                        TAG,
                                        "Test yield %d: level=%d, duration=%lu",
                                        i,
                                        level_duration_get_level(ld),
                                        level_duration_get_duration(ld));
                                } else {
                                    FURI_LOG_W(
                                        TAG,
                                        "Test yield %d: RESET - transmission ended or error",
                                        i);
                                    break;
                                }
                            }
                            FURI_LOG_I(TAG, "Transmitter test complete");

                            // Re-deserialize to reset the state after test
                            flipper_format_rewind(ff);
                            subghz_transmitter_deserialize(emulate_context->transmitter, ff);
                        } else if(status == SubGhzProtocolStatusErrorEncoderGetUpload) {
                            FURI_LOG_E(TAG, "Encoder get upload error");
                            subghz_transmitter_free(emulate_context->transmitter);
                            emulate_context->transmitter = NULL;
                        } else if(status == SubGhzProtocolStatusErrorParserOthers) {
                            FURI_LOG_E(TAG, "Parser error");
                            subghz_transmitter_free(emulate_context->transmitter);
                            emulate_context->transmitter = NULL;
                        } else {
                            FURI_LOG_E(
                                TAG, "Failed to deserialize transmitter, status: %d", status);
                            subghz_transmitter_free(emulate_context->transmitter);
                            emulate_context->transmitter = NULL;
                        }
                    } else {
                        FURI_LOG_E(TAG, "Failed to allocate transmitter for %s", proto_name);
                    }
                } else {
                    FURI_LOG_E(TAG, "Protocol %s has no encoder", proto_name);
                }
            } else {
                FURI_LOG_E(TAG, "Protocol %s not found in registry", proto_name);
            }
        } else {
            FURI_LOG_E(TAG, "Failed to load file");
        }
    } else {
        FURI_LOG_E(TAG, "No file path set");
    }

    // Set up view
    view_set_draw_callback(app->view_about, protopirate_emulate_draw_callback);
    view_set_input_callback(app->view_about, protopirate_emulate_input_callback);
    view_set_context(app->view_about, app);
    view_set_previous_callback(app->view_about, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewAbout);
}

bool protopirate_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ProtoPirateCustomEventEmulateTransmit:
            if(emulate_context && emulate_context->transmitter &&
               emulate_context->flipper_format) {
                // Stop any ongoing transmission FIRST
                if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                    FURI_LOG_W(TAG, "Previous transmission still active, stopping it");
                    subghz_devices_stop_async_tx(app->txrx->radio_device);

                    if(emulate_context->transmitter) {
                        subghz_transmitter_stop(emulate_context->transmitter);
                    }

                    furi_delay_ms(10);
                    subghz_devices_idle(app->txrx->radio_device);
                    app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
                }

                flipper_format_rewind(emulate_context->flipper_format);
                uint32_t verify_btn, verify_cnt;
                if(flipper_format_read_uint32(
                       emulate_context->flipper_format, "Btn", &verify_btn, 1)) {
                    FURI_LOG_I(
                        TAG,
                        "Flipper format Btn value before deserialize: 0x%02lX",
                        (unsigned long)verify_btn);
                }
                flipper_format_rewind(emulate_context->flipper_format);
                if(flipper_format_read_uint32(
                       emulate_context->flipper_format, "Cnt", &verify_cnt, 1)) {
                    FURI_LOG_I(
                        TAG,
                        "Flipper format Cnt value before deserialize: 0x%03lX",
                        (unsigned long)verify_cnt);
                }

                flipper_format_rewind(emulate_context->flipper_format);
                SubGhzProtocolStatus status = subghz_transmitter_deserialize(
                    emulate_context->transmitter, emulate_context->flipper_format);

                if(status != SubGhzProtocolStatusOk) {
                    FURI_LOG_E(TAG, "Failed to re-deserialize transmitter: %d", status);
                    notification_message(app->notifications, &sequence_error);
                    consumed = true;
                    break;
                }

                // Read frequency and preset from the saved file
                uint32_t frequency = 433920000;
                FuriString* preset_str = furi_string_alloc();

                flipper_format_rewind(emulate_context->flipper_format);
                if(!flipper_format_read_uint32(
                       emulate_context->flipper_format, "Frequency", &frequency, 1)) {
                    FURI_LOG_W(TAG, "Failed to read frequency, using default");
                }

                flipper_format_rewind(emulate_context->flipper_format);
                if(!flipper_format_read_string(
                       emulate_context->flipper_format, "Preset", preset_str)) {
                    FURI_LOG_W(TAG, "Failed to read preset, using FM476");
                    furi_string_set(preset_str, "FM476");
                }

                // Convert long preset name to short name
                const char* preset_long = furi_string_get_cstr(preset_str);
                const char* preset_name = protopirate_emulate_get_short_preset_name(preset_long);
                FURI_LOG_I(
                    TAG,
                    "Using frequency %lu Hz, preset %s (from %s)",
                    frequency,
                    preset_name,
                    preset_long);

                // Get preset data
                uint8_t* preset_data =
                    subghz_setting_get_preset_data_by_name(app->setting, preset_name);

                if(!preset_data) {
                    FURI_LOG_W(TAG, "Preset %s not found, trying FM476", preset_name);
                    preset_data = subghz_setting_get_preset_data_by_name(app->setting, "FM476");
                }
                if(!preset_data) {
                    FURI_LOG_W(TAG, "FM476 not found, trying AM650");
                    preset_data = subghz_setting_get_preset_data_by_name(app->setting, "AM650");
                }

                if(preset_data) {
                    FURI_LOG_I(TAG, "Got preset data, configuring radio");

                    // Configure radio for TX
                    subghz_devices_reset(app->txrx->radio_device);
                    subghz_devices_idle(app->txrx->radio_device);
                    subghz_devices_load_preset(
                        app->txrx->radio_device, FuriHalSubGhzPresetCustom, preset_data);
                    subghz_devices_set_frequency(app->txrx->radio_device, frequency);

                    FURI_LOG_I(TAG, "Starting async TX");

                    // Start transmission
                    subghz_devices_set_tx(app->txrx->radio_device);

                    if(subghz_devices_start_async_tx(
                           app->txrx->radio_device,
                           subghz_transmitter_yield,
                           emulate_context->transmitter)) {
                        app->txrx->txrx_state = ProtoPirateTxRxStateTx;
                        notification_message(app->notifications, &sequence_single_vibro);
                        FURI_LOG_I(TAG, "Transmission started successfully");
                    } else {
                        FURI_LOG_E(TAG, "Failed to start async TX");
                        subghz_devices_idle(app->txrx->radio_device);
                        notification_message(app->notifications, &sequence_error);
                    }
                } else {
                    FURI_LOG_E(TAG, "No preset data available - cannot transmit!");
                    notification_message(app->notifications, &sequence_error);
                }

                furi_string_free(preset_str);
            } else {
                FURI_LOG_E(
                    TAG,
                    "No transmitter available: ctx=%p, tx=%p, ff=%p",
                    emulate_context,
                    emulate_context ? emulate_context->transmitter : NULL,
                    emulate_context ? emulate_context->flipper_format : NULL);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
            break;

        case ProtoPirateCustomEventEmulateStop:
            // Stop transmission
            FURI_LOG_I(TAG, "Stop event received, txrx_state=%d", app->txrx->txrx_state);

            if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                FURI_LOG_I(TAG, "Stopping transmission");

                // Stop async TX first
                subghz_devices_stop_async_tx(app->txrx->radio_device);

                // Stop the encoder
                if(emulate_context && emulate_context->transmitter) {
                    subghz_transmitter_stop(emulate_context->transmitter);
                }

                furi_delay_ms(10);

                subghz_devices_idle(app->txrx->radio_device);

                app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;

                FURI_LOG_I(TAG, "Transmission stopped, state set to IDLE");
            } else {
                FURI_LOG_W(
                    TAG,
                    "Stop event received but txrx_state is not Tx (%d)",
                    app->txrx->txrx_state);
            }

            notification_message(app->notifications, &sequence_blink_stop);
            consumed = true;
            break;

        case ProtoPirateCustomEventEmulateExit:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        // Update display
        view_commit_model(app->view_about, true);

        if(emulate_context && emulate_context->is_transmitting) {
            if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
                notification_message(app->notifications, &sequence_blink_magenta_10);
            }
        }

        consumed = true;
    }

    return consumed;
}

void protopirate_scene_emulate_on_exit(void* context) {
    ProtoPirateApp* app = context;

    if(app->txrx->txrx_state == ProtoPirateTxRxStateTx) {
        FURI_LOG_I(TAG, "Stopping transmission on exit");

        subghz_devices_stop_async_tx(app->txrx->radio_device);

        if(emulate_context && emulate_context->transmitter) {
            subghz_transmitter_stop(emulate_context->transmitter);
        }

        furi_delay_ms(10);

        subghz_devices_idle(app->txrx->radio_device);
        app->txrx->txrx_state = ProtoPirateTxRxStateIDLE;
    } else if(app->txrx->txrx_state != ProtoPirateTxRxStateIDLE) {
        protopirate_idle(app);
    }

    if(emulate_context) {
        if(emulate_context->transmitter) {
            subghz_transmitter_free(emulate_context->transmitter);
        }
        if(emulate_context->flipper_format) {
            flipper_format_free(emulate_context->flipper_format);
        }
        furi_string_free(emulate_context->protocol_name);
        free(emulate_context);
        emulate_context = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);

    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);
}
