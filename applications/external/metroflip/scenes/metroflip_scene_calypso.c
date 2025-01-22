#include "metroflip_scene_calypso.h"
#include "../metroflip_i.h"
#include <datetime.h>
#include <dolphin/dolphin.h>
#include <notification/notification_messages.h>
#include <locale/locale.h>

#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#define TAG "Metroflip:Scene:Calypso"

int select_new_app(
    int new_app_directory,
    int new_app,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4bPoller* iso14443_4b_poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    select_app[5] = new_app_directory;
    select_app[6] = new_app;

    bit_buffer_reset(tx_buffer);
    bit_buffer_append_bytes(tx_buffer, select_app, sizeof(select_app));
    FURI_LOG_D(
        TAG,
        "SEND %02x %02x %02x %02x %02x %02x %02x %02x",
        select_app[0],
        select_app[1],
        select_app[2],
        select_app[3],
        select_app[4],
        select_app[5],
        select_app[6],
        select_app[7]);
    int error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_I(TAG, "Select File: iso14443_4b_poller_send_block error %d", error);
        *stage = MetroflipPollerEventTypeFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
        return error;
    }
    return 0;
}

int read_new_file(
    int new_file,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4bPoller* iso14443_4b_poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    read_file[2] = new_file;
    bit_buffer_reset(tx_buffer);
    bit_buffer_append_bytes(tx_buffer, read_file, sizeof(read_file));
    FURI_LOG_D(
        TAG,
        "SEND %02x %02x %02x %02x %02x",
        read_file[0],
        read_file[1],
        read_file[2],
        read_file[3],
        read_file[4]);
    Iso14443_4bError error =
        iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4bErrorNone) {
        FURI_LOG_I(TAG, "Read File: iso14443_4b_poller_send_block error %d", error);
        *stage = MetroflipPollerEventTypeFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
        return error;
    }
    return 0;
}

int check_response(
    BitBuffer* rx_buffer,
    Metroflip* app,
    MetroflipPollerEventType* stage,
    size_t* response_length) {
    *response_length = bit_buffer_get_size_bytes(rx_buffer);
    if(bit_buffer_get_byte(rx_buffer, *response_length - 2) != apdu_success[0] ||
       bit_buffer_get_byte(rx_buffer, *response_length - 1) != apdu_success[1]) {
        int error_code_1 = bit_buffer_get_byte(rx_buffer, *response_length - 2);
        int error_code_2 = bit_buffer_get_byte(rx_buffer, *response_length - 1);
        FURI_LOG_E(TAG, "Select profile app/file failed: %02x%02x", error_code_1, error_code_2);
        if(error_code_1 == 0x6a && error_code_2 == 0x82) {
            FURI_LOG_E(TAG, "Wrong parameter(s) P1-P2 - File not found");
        } else if(error_code_1 == 0x69 && error_code_2 == 0x82) {
            FURI_LOG_E(TAG, "Command not allowed - Security status not satisfied");
        }
        *stage = MetroflipPollerEventTypeFail;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MetroflipCustomEventPollerFileNotFound);
        return 1;
    }
    return 0;
}

void update_page_info(void* context, FuriString* parsed_data) {
    Metroflip* app = context;
    CalypsoContext* ctx = app->calypso_context;
    if(ctx->card->card_type != CALYPSO_CARD_NAVIGO && ctx->card->card_type != CALYPSO_CARD_OPUS) {
        furi_string_cat_printf(
            parsed_data,
            "\e#%s %u:\n",
            get_network_string(ctx->card->card_type),
            ctx->card->card_number);
        return;
    }
    if(ctx->page_id == 0 || ctx->page_id == 1 || ctx->page_id == 2 || ctx->page_id == 3) {
        switch(ctx->card->card_type) {
        case CALYPSO_CARD_NAVIGO: {
            furi_string_cat_printf(
                parsed_data,
                "\e#%s %u:\n",
                get_navigo_type(ctx->card->navigo->holder.card_status),
                ctx->card->card_number);
            furi_string_cat_printf(parsed_data, "\e#Contract %d:\n", ctx->page_id + 1);
            show_navigo_contract_info(&ctx->card->navigo->contracts[ctx->page_id], parsed_data);
            break;
        }
        case CALYPSO_CARD_OPUS: {
            furi_string_cat_printf(parsed_data, "\e#Opus %u:\n", ctx->card->card_number);
            furi_string_cat_printf(parsed_data, "\e#Contract %d:\n", ctx->page_id + 1);
            show_opus_contract_info(&ctx->card->opus->contracts[ctx->page_id], parsed_data);
            break;
        }
        default: {
            furi_string_cat_printf(parsed_data, "\e#Unknown %u:\n", ctx->card->card_number);
            break;
        }
        }
    } else if(ctx->page_id == 4) {
        furi_string_cat_printf(parsed_data, "\e#Environment:\n");
        switch(ctx->card->card_type) {
        case CALYPSO_CARD_NAVIGO: {
            show_navigo_environment_info(&ctx->card->navigo->environment, parsed_data);
            break;
        }
        case CALYPSO_CARD_OPUS: {
            show_opus_environment_info(&ctx->card->opus->environment, parsed_data);
            break;
        }
        default: {
            break;
        }
        }
    } else if(ctx->page_id == 5 || ctx->page_id == 6 || ctx->page_id == 7) {
        furi_string_cat_printf(parsed_data, "\e#Event %d:\n", ctx->page_id - 4);
        switch(ctx->card->card_type) {
        case CALYPSO_CARD_NAVIGO: {
            show_navigo_event_info(
                &ctx->card->navigo->events[ctx->page_id - 5],
                ctx->card->navigo->contracts,
                parsed_data);
            break;
        }
        case CALYPSO_CARD_OPUS: {
            show_opus_event_info(
                &ctx->card->opus->events[ctx->page_id - 5],
                ctx->card->opus->contracts,
                parsed_data);
            break;
        }
        default: {
            break;
        }
        }
    }
}

void update_widget_elements(void* context) {
    Metroflip* app = context;
    CalypsoContext* ctx = app->calypso_context;
    Widget* widget = app->widget;
    if(ctx->card->card_type != CALYPSO_CARD_NAVIGO && ctx->card->card_type != CALYPSO_CARD_OPUS) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_next_button_widget_callback, context);
        return;
    }
    if(ctx->page_id < 7) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Next", metroflip_next_button_widget_callback, context);
    } else {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_next_button_widget_callback, context);
    }
    if(ctx->page_id > 0) {
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Back", metroflip_back_button_widget_callback, context);
    }
}

void metroflip_back_button_widget_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    CalypsoContext* ctx = app->calypso_context;
    UNUSED(result);

    Widget* widget = app->widget;

    if(type == InputTypePress) {
        widget_reset(widget);

        FURI_LOG_I(TAG, "Page ID: %d -> %d", ctx->page_id, ctx->page_id - 1);

        if(ctx->page_id > 0) {
            if(ctx->page_id == 4 && ctx->card->contracts_count < 4) {
                ctx->page_id -= 1;
            }
            if(ctx->page_id == 3 && ctx->card->contracts_count < 3) {
                ctx->page_id -= 1;
            }
            if(ctx->page_id == 2 && ctx->card->contracts_count < 2) {
                ctx->page_id -= 1;
            }
            ctx->page_id -= 1;
        }

        FuriString* parsed_data = furi_string_alloc();

        // Ensure no nested mutexes
        furi_mutex_acquire(ctx->mutex, FuriWaitForever);
        update_page_info(app, parsed_data);
        furi_mutex_release(ctx->mutex);

        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
        // widget_add_icon_element(widget, 0, 0, &I_RFIDDolphinReceive_97x61);

        // Ensure no nested mutexes
        furi_mutex_acquire(ctx->mutex, FuriWaitForever);
        update_widget_elements(app);
        furi_mutex_release(ctx->mutex);

        furi_string_free(parsed_data);
    }
}

void metroflip_next_button_widget_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    CalypsoContext* ctx = app->calypso_context;
    UNUSED(result);

    Widget* widget = app->widget;

    if(type == InputTypePress) {
        widget_reset(widget);

        FURI_LOG_I(TAG, "Page ID: %d -> %d", ctx->page_id, ctx->page_id + 1);

        if(ctx->card->card_type != CALYPSO_CARD_NAVIGO &&
           ctx->card->card_type != CALYPSO_CARD_OPUS) {
            ctx->page_id = 0;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            return;
        }
        if(ctx->page_id < 7) {
            if(ctx->page_id == 0 && ctx->card->contracts_count < 2) {
                ctx->page_id += 1;
            }
            if(ctx->page_id == 1 && ctx->card->contracts_count < 3) {
                ctx->page_id += 1;
            }
            if(ctx->page_id == 2 && ctx->card->contracts_count < 4) {
                ctx->page_id += 1;
            }
            ctx->page_id += 1;
        } else {
            ctx->page_id = 0;
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            return;
        }

        FuriString* parsed_data = furi_string_alloc();

        // Ensure no nested mutexes
        furi_mutex_acquire(ctx->mutex, FuriWaitForever);
        update_page_info(app, parsed_data);
        furi_mutex_release(ctx->mutex);

        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        // Ensure no nested mutexes
        furi_mutex_acquire(ctx->mutex, FuriWaitForever);
        update_widget_elements(app);
        furi_mutex_release(ctx->mutex);

        furi_string_free(parsed_data);
    }
}

void delay(int milliseconds) {
    furi_thread_flags_wait(0, FuriFlagWaitAny, milliseconds);
}

static NfcCommand metroflip_scene_navigo_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4b);
    NfcCommand next_command = NfcCommandContinue;
    MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    Metroflip* app = context;
    FuriString* parsed_data = furi_string_alloc();
    Widget* widget = app->widget;
    furi_string_reset(app->text_box_store);

    const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;

    Iso14443_4bPoller* iso14443_4b_poller = event.instance;

    BitBuffer* tx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);

    if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
        if(stage == MetroflipPollerEventTypeStart) {
            // Start Flipper vibration
            NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notification, &sequence_set_vibro_on);
            delay(50);
            notification_message(notification, &sequence_reset_vibro);
            nfc_device_set_data(
                app->nfc_device, NfcProtocolIso14443_4b, nfc_poller_get_data(app->poller));

            Iso14443_4bError error;
            size_t response_length = 0;

            do {
                // Initialize the card data
                CalypsoCardData* card = malloc(sizeof(CalypsoCardData));

                // Select app ICC
                error = select_new_app(
                    0x00, 0x02, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                // Now send the read command for ICC
                error = read_new_file(0x01, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                char icc_bit_representation[response_length * 8 + 1];
                icc_bit_representation[0] = '\0';
                for(size_t i = 0; i < response_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(icc_bit_representation, bits, sizeof(icc_bit_representation));
                }
                icc_bit_representation[response_length * 8] = '\0';

                int start = 128, end = 159;
                card->card_number = bit_slice_to_dec(icc_bit_representation, start, end);

                // Select app for ticketing
                error = select_new_app(
                    0x20, 0x00, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    FURI_LOG_E(TAG, "Failed to select app for ticketing");
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_E(TAG, "Failed to check response after selecting app for ticketing");
                    break;
                }

                // Select app for environment
                error = select_new_app(
                    0x20, 0x1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                // read file 1
                error = read_new_file(1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                if(error != 0) {
                    break;
                }

                // Check the response after reading the file
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }

                char environment_bit_representation[response_length * 8 + 1];
                environment_bit_representation[0] = '\0';
                for(size_t i = 0; i < response_length; i++) {
                    char bits[9];
                    uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                    byte_to_binary(byte, bits);
                    strlcat(
                        environment_bit_representation,
                        bits,
                        sizeof(environment_bit_representation));
                }
                // FURI_LOG_I(
                //     TAG, "Environment bit_representation: %s", environment_bit_representation);
                start = 13;
                end = 16;
                int country_num =
                    bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                    bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                    bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
                start = 25;
                end = 28;
                int network_num =
                    bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                    bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                    bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
                card->card_type = guess_card_type(country_num, network_num);
                switch(card->card_type) {
                case CALYPSO_CARD_NAVIGO: {
                    card->navigo = malloc(sizeof(NavigoCardData));

                    card->navigo->environment.country_num = country_num;
                    card->navigo->environment.network_num = network_num;

                    CalypsoApp* IntercodeEnvHolderStructure = get_intercode_env_holder_structure();

                    // EnvApplicationVersionNumber
                    const char* env_key = "EnvApplicationVersionNumber";
                    int positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, IntercodeEnvHolderStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
                    card->navigo->environment.app_version =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // EnvApplicationValidityEndDate
                    env_key = "EnvApplicationValidityEndDate";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, IntercodeEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
                    float decimal_value =
                        bit_slice_to_dec(environment_bit_representation, start, end);
                    uint64_t end_validity_timestamp =
                        (decimal_value * 24 * 3600) + (float)epoch + 3600;
                    datetime_timestamp_to_datetime(
                        end_validity_timestamp, &card->navigo->environment.end_dt);

                    // HolderDataCardStatus
                    env_key = "HolderDataCardStatus";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, IntercodeEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
                    card->navigo->holder.card_status =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // HolderDataCommercialID
                    env_key = "HolderDataCommercialID";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, IntercodeEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset +
                          get_calypso_node_size(env_key, IntercodeEnvHolderStructure) - 1;
                    card->navigo->holder.commercial_id =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // Free the calypso structure
                    free_calypso_structure(IntercodeEnvHolderStructure);

                    // Select app for contracts
                    error = select_new_app(
                        0x20, 0x20, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        FURI_LOG_E(TAG, "Failed to select app for contracts");
                        break;
                    }

                    // Check the response after selecting app
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        FURI_LOG_E(
                            TAG, "Failed to check response after selecting app for contracts");
                        break;
                    }

                    // Prepare calypso structure
                    CalypsoApp* IntercodeContractStructure = get_intercode_contract_structure();
                    if(!IntercodeContractStructure) {
                        FURI_LOG_E(TAG, "Failed to load Intercode Contract structure");
                        break;
                    }

                    // Now send the read command for contracts
                    for(size_t i = 1; i < 5; i++) {
                        error = read_new_file(
                            i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                        if(error != 0) {
                            FURI_LOG_E(TAG, "Failed to read contract %d", i);
                            break;
                        }

                        // Check the response after reading the file
                        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                            FURI_LOG_E(
                                TAG, "Failed to check response after reading contract %d", i);
                            break;
                        }

                        char bit_representation[response_length * 8 + 1];
                        bit_representation[0] = '\0';
                        for(size_t i = 0; i < response_length; i++) {
                            char bits[9];
                            uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                            byte_to_binary(byte, bits);
                            strlcat(bit_representation, bits, sizeof(bit_representation));
                        }
                        bit_representation[response_length * 8] = '\0';

                        if(bit_slice_to_dec(
                               bit_representation,
                               0,
                               IntercodeContractStructure->container->elements[0].bitmap->size -
                                   1) == 0) {
                            break;
                        }

                        card->navigo->contracts[i - 1].present = 1;
                        card->contracts_count++;

                        // 2. ContractTariff
                        const char* contract_key = "ContractTariff";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].tariff =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // 3. ContractSerialNumber
                        contract_key = "ContractSerialNumber";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].serial_number =
                                bit_slice_to_dec(bit_representation, start, end);
                            card->navigo->contracts[i - 1].serial_number_available = true;
                        }

                        // 8. ContractPayMethod
                        contract_key = "ContractPayMethod";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].pay_method =
                                bit_slice_to_dec(bit_representation, start, end);
                            card->navigo->contracts[i - 1].pay_method_available = true;
                        }

                        // 10. ContractPriceAmount
                        contract_key = "ContractPriceAmount";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].price_amount =
                                bit_slice_to_dec(bit_representation, start, end) / 100.0;
                            card->navigo->contracts[i - 1].price_amount_available = true;
                        }

                        // 13.0. ContractValidityStartDate
                        contract_key = "ContractValidityStartDate";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            float decimal_value =
                                bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                            uint64_t start_validity_timestamp =
                                (decimal_value + (float)epoch) + 3600;
                            datetime_timestamp_to_datetime(
                                start_validity_timestamp,
                                &card->navigo->contracts[i - 1].start_date);
                        }

                        // 13.2. ContractValidityEndDate
                        contract_key = "ContractValidityEndDate";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            float decimal_value =
                                bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                            uint64_t end_validity_timestamp =
                                (decimal_value + (float)epoch) + 3600;
                            datetime_timestamp_to_datetime(
                                end_validity_timestamp, &card->navigo->contracts[i - 1].end_date);
                            card->navigo->contracts[i - 1].end_date_available = true;
                        }

                        // 13.6. ContractValidityZones
                        contract_key = "ContractValidityZones";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int start = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            // binary form is 00011111 for zones 5, 4, 3, 2, 1
                            for(int j = 0; j < 5; j++) {
                                card->navigo->contracts[i - 1].zones[j] = bit_slice_to_dec(
                                    bit_representation, start + 3 + j, start + 3 + j);
                            }
                            card->navigo->contracts[i - 1].zones_available = true;
                        }

                        // 13.7. ContractValidityJourneys  -- pas sûr de le mettre lui

                        // 15.0. ContractValiditySaleDate
                        contract_key = "ContractValiditySaleDate";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            float decimal_value =
                                bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                            uint64_t sale_timestamp = (decimal_value + (float)epoch) + 3600;
                            datetime_timestamp_to_datetime(
                                sale_timestamp, &card->navigo->contracts[i - 1].sale_date);
                        }

                        // 15.2. ContractValiditySaleAgent - FIX NEEDED
                        contract_key = "ContractValiditySaleAgent";
                        /* if(is_calypso_node_present(
                           bit_representation, contract_key, NavigoContractStructure)) { */
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, IntercodeContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, IntercodeContractStructure) -
                                  1;
                        card->navigo->contracts[i - 1].sale_agent =
                            bit_slice_to_dec(bit_representation, start, end);
                        // }

                        // 15.3. ContractValiditySaleDevice
                        contract_key = "ContractValiditySaleDevice";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].sale_device =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // 16. ContractStatus  -- 0x1 ou 0xff
                        contract_key = "ContractStatus";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].status =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // 18. ContractAuthenticator
                        contract_key = "ContractAuthenticator";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, IntercodeContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, IntercodeContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(
                                          contract_key, IntercodeContractStructure) -
                                      1;
                            card->navigo->contracts[i - 1].authenticator =
                                bit_slice_to_dec(bit_representation, start, end);
                        }
                    }

                    // Free the calypso structure
                    free_calypso_structure(IntercodeContractStructure);

                    // Select app for counters (remaining tickets on Navigo Easy)
                    error = select_new_app(
                        0x20, 0x69, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        break;
                    }

                    // Check the response after selecting app
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        break;
                    }

                    // read file 1
                    error =
                        read_new_file(1, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        break;
                    }

                    // Check the response after reading the file
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        break;
                    }

                    char counter_bit_representation[response_length * 8 + 1];
                    counter_bit_representation[0] = '\0';
                    for(size_t i = 0; i < response_length; i++) {
                        char bits[9];
                        uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                        byte_to_binary(byte, bits);
                        strlcat(
                            counter_bit_representation, bits, sizeof(counter_bit_representation));
                    }
                    // FURI_LOG_I(TAG, "Counter bit_representation: %s", counter_bit_representation);

                    // Ticket counts (contracts 1-4)
                    for(int i = 0; i < 4; i++) {
                        start = 0;
                        end = 5;
                        card->navigo->contracts[i].counter.count = bit_slice_to_dec(
                            counter_bit_representation, 24 * i + start, 24 * i + end);

                        start = 6;
                        end = 23;
                        card->navigo->contracts[i].counter.relative_first_stamp_15mn =
                            bit_slice_to_dec(
                                counter_bit_representation, 24 * i + start, 24 * i + end);
                    }

                    // Select app for events
                    error = select_new_app(
                        0x20, 0x10, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        break;
                    }

                    // Check the response after selecting app
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        break;
                    }

                    // Load the calypso structure for events
                    CalypsoApp* IntercodeEventStructure = get_intercode_event_structure();
                    if(!IntercodeEventStructure) {
                        FURI_LOG_E(TAG, "Failed to load Intercode Event structure");
                        break;
                    }

                    // furi_string_cat_printf(parsed_data, "\e#Events :\n");
                    // Now send the read command for events
                    for(size_t i = 1; i < 4; i++) {
                        error = read_new_file(
                            i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                        if(error != 0) {
                            break;
                        }

                        // Check the response after reading the file
                        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                            break;
                        }

                        char event_bit_representation[response_length * 8 + 1];
                        event_bit_representation[0] = '\0';
                        for(size_t i = 0; i < response_length; i++) {
                            char bits[9];
                            uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                            byte_to_binary(byte, bits);
                            strlcat(
                                event_bit_representation, bits, sizeof(event_bit_representation));
                        }

                        // furi_string_cat_printf(parsed_data, "Event 0%d :\n", i);
                        /* int count = 0;
                    int start = 25, end = 52;
                    char bit_slice[end - start + 2];
                    strncpy(bit_slice, event_bit_representation + start, end - start + 1);
                    bit_slice[end - start + 1] = '\0';
                    int* positions = get_bit_positions(bit_slice, &count);
                    FURI_LOG_I(TAG, "Positions: ");
                    for(int i = 0; i < count; i++) {
                        FURI_LOG_I(TAG, "%d ", positions[i]);
                    } */

                        // 2. EventCode
                        const char* event_key = "EventCode";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(event_key, IntercodeEventStructure) -
                                      1;
                            int decimal_value =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].transport_type = decimal_value >> 4;
                            card->navigo->events[i - 1].transition = decimal_value & 15;
                        }

                        // 4. EventServiceProvider
                        event_key = "EventServiceProvider";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].service_provider =
                                bit_slice_to_dec(event_bit_representation, start, end);
                        }

                        // 8. EventLocationId
                        event_key = "EventLocationId";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            int decimal_value =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].station_group_id = decimal_value >> 9;
                            card->navigo->events[i - 1].station_id = (decimal_value >> 4) & 31;
                        }

                        // 9. EventLocationGate
                        event_key = "EventLocationGate";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].location_gate =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].location_gate_available = true;
                        }

                        // 10. EventDevice
                        event_key = "EventDevice";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            int decimal_value =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].device = decimal_value;
                            int bus_device = decimal_value >> 8;
                            card->navigo->events[i - 1].door = bus_device / 2 + 1;
                            card->navigo->events[i - 1].side = bus_device % 2;
                            card->navigo->events[i - 1].device_available = true;
                        }

                        // 11. EventRouteNumber
                        event_key = "EventRouteNumber";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].route_number =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].route_number_available = true;
                        }

                        // 13. EventJourneyRun
                        event_key = "EventJourneyRun";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].mission =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].mission_available = true;
                        }

                        // 14. EventVehicleId
                        event_key = "EventVehicleId";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].vehicle_id =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].vehicle_id_available = true;
                        }

                        // 25. EventContractPointer
                        event_key = "EventContractPointer";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, IntercodeEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, IntercodeEventStructure);
                            start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                            card->navigo->events[i - 1].used_contract =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->navigo->events[i - 1].used_contract_available = true;
                        }

                        // EventDateStamp
                        event_key = "EventDateStamp";
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, IntercodeEventStructure);
                        start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                        int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                        uint64_t date_timestamp = (decimal_value * 24 * 3600) + epoch + 3600;
                        datetime_timestamp_to_datetime(
                            date_timestamp, &card->navigo->events[i - 1].date);

                        // EventTimeStamp
                        event_key = "EventTimeStamp";
                        positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, IntercodeEventStructure);
                        start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, IntercodeEventStructure) - 1;
                        decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                        card->navigo->events[i - 1].date.hour = (decimal_value * 60) / 3600;
                        card->navigo->events[i - 1].date.minute =
                            ((decimal_value * 60) % 3600) / 60;
                        card->navigo->events[i - 1].date.second =
                            ((decimal_value * 60) % 3600) % 60;
                    }

                    // Free the calypso structure
                    free_calypso_structure(IntercodeEventStructure);
                    break;
                }
                case CALYPSO_CARD_OPUS: {
                    card->opus = malloc(sizeof(OpusCardData));

                    card->opus->environment.country_num = country_num;
                    card->opus->environment.network_num = network_num;

                    CalypsoApp* OpusEnvHolderStructure = get_opus_env_holder_structure();

                    // EnvApplicationVersionNumber
                    const char* env_key = "EnvApplicationVersionNumber";
                    int positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, OpusEnvHolderStructure);
                    int start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(env_key, OpusEnvHolderStructure) - 1;
                    card->opus->environment.app_version =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // EnvApplicationValidityEndDate
                    env_key = "EnvApplicationValidityEndDate";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, OpusEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) -
                          1;
                    float decimal_value =
                        bit_slice_to_dec(environment_bit_representation, start, end);
                    uint64_t end_validity_timestamp =
                        (decimal_value * 24 * 3600) + (float)epoch + 3600;
                    datetime_timestamp_to_datetime(
                        end_validity_timestamp, &card->opus->environment.end_dt);

                    // HolderDataCardStatus
                    env_key = "HolderDataCardStatus";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, OpusEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) -
                          1;
                    card->opus->holder.card_status =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // HolderDataCommercialID
                    env_key = "HolderDataCommercialID";
                    positionOffset = get_calypso_node_offset(
                        environment_bit_representation, env_key, OpusEnvHolderStructure);
                    start = positionOffset,
                    end = positionOffset + get_calypso_node_size(env_key, OpusEnvHolderStructure) -
                          1;
                    card->opus->holder.commercial_id =
                        bit_slice_to_dec(environment_bit_representation, start, end);

                    // Free the calypso structure
                    free_calypso_structure(OpusEnvHolderStructure);

                    // Select app for contracts
                    error = select_new_app(
                        0x20, 0x20, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        FURI_LOG_E(TAG, "Failed to select app for contracts");
                        break;
                    }

                    // Check the response after selecting app
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        FURI_LOG_E(
                            TAG, "Failed to check response after selecting app for contracts");
                        break;
                    }

                    // Prepare calypso structure
                    CalypsoApp* OpusContractStructure = get_opus_contract_structure();
                    if(!OpusContractStructure) {
                        FURI_LOG_E(TAG, "Failed to load Opus Contract structure");
                        break;
                    }

                    // Now send the read command for contracts
                    for(size_t i = 1; i < 5; i++) {
                        error = read_new_file(
                            i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                        if(error != 0) {
                            FURI_LOG_E(TAG, "Failed to read contract %d", i);
                            break;
                        }

                        // Check the response after reading the file
                        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                            FURI_LOG_E(
                                TAG, "Failed to check response after reading contract %d", i);
                            break;
                        }

                        char bit_representation[response_length * 8 + 1];
                        bit_representation[0] = '\0';
                        for(size_t i = 0; i < response_length; i++) {
                            char bits[9];
                            uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                            byte_to_binary(byte, bits);
                            strlcat(bit_representation, bits, sizeof(bit_representation));
                        }
                        bit_representation[response_length * 8] = '\0';

                        if(bit_slice_to_dec(
                               bit_representation,
                               0,
                               OpusContractStructure->container->elements[1].bitmap->size - 1) ==
                           0) {
                            break;
                        }

                        card->opus->contracts[i - 1].present = 1;
                        card->contracts_count++;

                        // ContractProvider
                        const char* contract_key = "ContractProvider";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, OpusContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, OpusContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(contract_key, OpusContractStructure) -
                                      1;
                            card->opus->contracts[i - 1].provider =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // ContractTariff
                        contract_key = "ContractTariff";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, OpusContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, OpusContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(contract_key, OpusContractStructure) -
                                      1;
                            card->opus->contracts[i - 1].tariff =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // ContractStartDate
                        contract_key = "ContractStartDate";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, OpusContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, OpusContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(contract_key, OpusContractStructure) -
                                      1;
                            float decimal_value =
                                bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                            uint64_t start_validity_timestamp =
                                (decimal_value + (float)epoch) + 3600;
                            datetime_timestamp_to_datetime(
                                start_validity_timestamp,
                                &card->opus->contracts[i - 1].start_date);
                        }

                        // ContractEndDate
                        contract_key = "ContractEndDate";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, OpusContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, OpusContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(contract_key, OpusContractStructure) -
                                      1;
                            float decimal_value =
                                bit_slice_to_dec(bit_representation, start, end) * 24 * 3600;
                            uint64_t end_validity_timestamp =
                                (decimal_value + (float)epoch) + 3600;
                            datetime_timestamp_to_datetime(
                                end_validity_timestamp, &card->opus->contracts[i - 1].end_date);
                        }

                        // ContractStatus
                        contract_key = "ContractStatus";
                        if(is_calypso_node_present(
                               bit_representation, contract_key, OpusContractStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                bit_representation, contract_key, OpusContractStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(contract_key, OpusContractStructure) -
                                      1;
                            card->opus->contracts[i - 1].status =
                                bit_slice_to_dec(bit_representation, start, end);
                        }

                        // ContractSaleDate + ContractSaleTime
                        contract_key = "ContractSaleDate";
                        int positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, OpusContractStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                        uint64_t sale_date_timestamp =
                            (bit_slice_to_dec(bit_representation, start, end) + (float)epoch) +
                            3600;
                        datetime_timestamp_to_datetime(
                            sale_date_timestamp, &card->opus->contracts[i - 1].sale_date);

                        contract_key = "ContractSaleTime";
                        positionOffset = get_calypso_node_offset(
                            bit_representation, contract_key, OpusContractStructure);
                        start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(contract_key, OpusContractStructure) - 1;
                        int decimal_value = bit_slice_to_dec(bit_representation, start, end);
                        card->opus->contracts[i - 1].sale_date.hour = (decimal_value * 60) / 3600;
                        card->opus->contracts[i - 1].sale_date.minute =
                            ((decimal_value * 60) % 3600) / 60;
                        card->opus->contracts[i - 1].sale_date.second =
                            ((decimal_value * 60) % 3600) % 60;
                    }

                    // Free the calypso structure
                    free_calypso_structure(OpusContractStructure);

                    // Select app for events
                    error = select_new_app(
                        0x20, 0x10, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                    if(error != 0) {
                        break;
                    }

                    // Check the response after selecting app
                    if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                        break;
                    }

                    // Load the calypso structure for events
                    CalypsoApp* OpusEventStructure = get_opus_event_structure();
                    if(!OpusEventStructure) {
                        FURI_LOG_E(TAG, "Failed to load Opus Event structure");
                        break;
                    }

                    // Now send the read command for events
                    for(size_t i = 1; i < 4; i++) {
                        error = read_new_file(
                            i, tx_buffer, rx_buffer, iso14443_4b_poller, app, &stage);
                        if(error != 0) {
                            break;
                        }

                        // Check the response after reading the file
                        if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                            break;
                        }

                        char event_bit_representation[response_length * 8 + 1];
                        event_bit_representation[0] = '\0';
                        for(size_t i = 0; i < response_length; i++) {
                            char bits[9];
                            uint8_t byte = bit_buffer_get_byte(rx_buffer, i);
                            byte_to_binary(byte, bits);
                            strlcat(
                                event_bit_representation, bits, sizeof(event_bit_representation));
                        }

                        // EventServiceProvider
                        const char* event_key = "EventServiceProvider";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, OpusEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, OpusEventStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(event_key, OpusEventStructure) - 1;
                            card->opus->events[i - 1].service_provider =
                                bit_slice_to_dec(event_bit_representation, start, end);
                        }

                        // EventRouteNumber
                        event_key = "EventRouteNumber";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, OpusEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, OpusEventStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(event_key, OpusEventStructure) - 1;
                            card->opus->events[i - 1].route_number =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->opus->events[i - 1].route_number_available = true;
                        }

                        // EventContractPointer
                        event_key = "EventContractPointer";
                        if(is_calypso_node_present(
                               event_bit_representation, event_key, OpusEventStructure)) {
                            int positionOffset = get_calypso_node_offset(
                                event_bit_representation, event_key, OpusEventStructure);
                            int start = positionOffset,
                                end = positionOffset +
                                      get_calypso_node_size(event_key, OpusEventStructure) - 1;
                            card->opus->events[i - 1].used_contract =
                                bit_slice_to_dec(event_bit_representation, start, end);
                            card->opus->events[i - 1].used_contract_available = true;
                        }

                        // EventDate + EventTime
                        event_key = "EventDate";
                        int positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, OpusEventStructure);
                        int start = positionOffset,
                            end = positionOffset +
                                  get_calypso_node_size(event_key, OpusEventStructure) - 1;
                        uint64_t date_timestamp =
                            (bit_slice_to_dec(event_bit_representation, start, end) +
                             (float)epoch) +
                            3600;
                        datetime_timestamp_to_datetime(
                            date_timestamp, &card->opus->events[i - 1].date);

                        event_key = "EventTime";
                        positionOffset = get_calypso_node_offset(
                            event_bit_representation, event_key, OpusEventStructure);
                        start = positionOffset,
                        end = positionOffset +
                              get_calypso_node_size(event_key, OpusEventStructure) - 1;
                        int decimal_value = bit_slice_to_dec(event_bit_representation, start, end);
                        card->opus->events[i - 1].date.hour = (decimal_value * 60) / 3600;
                        card->opus->events[i - 1].date.minute = ((decimal_value * 60) % 3600) / 60;
                        card->opus->events[i - 1].date.second = ((decimal_value * 60) % 3600) % 60;
                    }

                    // Free the calypso structure
                    free_calypso_structure(OpusEventStructure);

                    break;
                }
                case CALYPSO_CARD_UNKNOWN: {
                    start = 3;
                    end = 6;
                    country_num =
                        bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                        bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                        bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
                    start = 15;
                    end = 18;
                    network_num =
                        bit_slice_to_dec(environment_bit_representation, start, end) * 100 +
                        bit_slice_to_dec(environment_bit_representation, start + 4, end + 4) * 10 +
                        bit_slice_to_dec(environment_bit_representation, start + 8, end + 8);
                    card->card_type = guess_card_type(country_num, network_num);
                    if(card->card_type == CALYPSO_CARD_RAVKAV) {
                    }
                    break;
                }
                default:
                    break;
                }

                widget_add_text_scroll_element(
                    widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

                CalypsoContext* context = malloc(sizeof(CalypsoContext));
                context->card = card;
                context->page_id = 0;
                context->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
                app->calypso_context = context;

                // Ensure no nested mutexes
                furi_mutex_acquire(context->mutex, FuriWaitForever);
                update_page_info(app, parsed_data);
                furi_mutex_release(context->mutex);

                widget_add_text_scroll_element(
                    widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

                // Ensure no nested mutexes
                furi_mutex_acquire(context->mutex, FuriWaitForever);
                update_widget_elements(app);
                furi_mutex_release(context->mutex);

                furi_string_free(parsed_data);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                metroflip_app_blink_stop(app);
                stage = MetroflipPollerEventTypeSuccess;
                next_command = NfcCommandStop;
            } while(false);

            if(stage != MetroflipPollerEventTypeSuccess) {
                next_command = NfcCommandStop;
            }
        }
    }
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return next_command;
}

void metroflip_scene_navigo_on_enter(void* context) {
    Metroflip* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
    nfc_scanner_alloc(app->nfc);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4b);
    nfc_poller_start(app->poller, metroflip_scene_navigo_poller_callback, app);

    metroflip_app_blink_start(app);
}

bool metroflip_scene_navigo_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipPollerEventTypeCardDetect) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Scanning..", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFileNotFound) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read Error,\n wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Error, try\n again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

void metroflip_scene_navigo_on_exit(void* context) {
    Metroflip* app = context;

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
    metroflip_app_blink_stop(app);
    widget_reset(app->widget);

    // Clear view
    popup_reset(app->popup);

    if(app->calypso_context) {
        CalypsoContext* ctx = app->calypso_context;
        free(ctx->card->navigo);
        free(ctx->card->opus);
        free(ctx->card);
        furi_mutex_free(ctx->mutex);
        free(ctx);
        app->calypso_context = NULL;
    }
}
