
#include "eink_goodisplay_i.h"

static void nfc_eink_screen_command_A4(const APDU_Command_Select* command, APDU_Response* resp) {
    bool equal = false;
    if(command->data_length == 0x07) {
        const uint8_t app_select_data[] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
        equal = memcmp(command->data, app_select_data, 0x07) == 0;
    } else if(command->data_length == 0x02 && command->data[0] == 0xE1) {
        equal = command->data[1] == 0x03 || command->data[1] == 0x04;
    }
    uint16_t status = equal ? 0x9000 : 0x6A82;
    resp->status = __builtin_bswap16(status);
}

///TODO: this function must return NfcCommand and length should be returned through pointer
static uint8_t
    nfc_eink_screen_command_B0(const APDU_Command_Read* command, APDU_Response_Read* resp) {
    uint16_t length = command->expected_length;
    uint16_t* status = NULL;

    if(length == 0x000F) {
        resp->data_length = __builtin_bswap16(0x000F);
        CC_File* cc_file = (CC_File*)resp->data;
        cc_file->mapping_version = 0x20;
        cc_file->r_apdu_max_size = __builtin_bswap16(0x0100);
        cc_file->c_apdu_max_size = __builtin_bswap16(0x00FA);
        cc_file->ctrl_file.type = 0x04;
        cc_file->ctrl_file.length = 0x06;
        cc_file->ctrl_file.file_id = __builtin_bswap16(0xE104);
        cc_file->ctrl_file.file_size = __builtin_bswap16(0x01F4);
        cc_file->ctrl_file.read_access_flags = 0x00;
        cc_file->ctrl_file.write_access_flags = 0x00;

        status = (uint16_t*)(resp->data + length - 2);
    } else if(length == 0x0002) {
        resp->data_length = 0x0000;
        status = (uint16_t*)(resp->data);
        length = 2;
    }

    *status = __builtin_bswap16(0x9000);

    return length;
}

static uint8_t nfc_eink_screen_command_DA(const APDU_Command* command, APDU_Response* resp) {
    uint8_t length = 0;

    const APDU_Header* hdr = &command->header;
    if(hdr->P1 == 0x00 && hdr->P2 == 0x00 && command->data_length == 0x03) {
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    }
    return length;
}

static uint8_t nfc_eink_screen_command_DB(
    NfcEinkScreen* const screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    const APDU_Header* hdr = &command->header;
    uint8_t length = 0;
    if(hdr->P1 == 0x02 && hdr->P2 == 0x00 && command->data_length == 0) {
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    } else if(hdr->P1 == 0x00 && hdr->P2 == 0x00 && command->data_length == 0x67) {
        ///TODO: Add here some saving and parsing logic for screen config
        eink_goodisplay_parse_config(screen, command->data, command->data_length);
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    }

    return length;
}

static uint8_t nfc_eink_screen_command_D2(
    NfcEinkScreenData* screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    //UNUSED(command);
    UNUSED(resp);
    FURI_LOG_D(TAG, "F0 D2: %d", command->data_length);
    uint8_t* data = screen->image_data + screen->received_data;
    memcpy(data, command->data, command->data_length - 2);
    screen->received_data += command->data_length - 2;

    return 1;
}

static uint8_t nfc_eink_screen_command_FF(const APDU_Command* command, APDU_Response* resp) {
    UNUSED(command);
    UNUSED(resp);

    resp->status = __builtin_bswap16(0x9000);
    return 2;
}

NfcCommand eink_goodisplay_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    //NfcEinkApp* instance = context;
    NfcEinkScreen* instance = context;
    Iso14443_4aListenerEvent* Iso14443_4a_event = event.event_data;
    NfcEinkScreenSpecificGoodisplayContext* ctx = instance->device->screen_context;

    if(Iso14443_4a_event->type == Iso14443_4aListenerEventTypeFieldOff) {
        if(ctx->listener_state != NfcEinkScreenGoodisplayListenerStateIdle)
            eink_goodisplay_on_target_lost(instance);
    } else if(Iso14443_4a_event->type == Iso14443_4aListenerEventTypeReceivedData) {
        //FURI_LOG_D(TAG, "ReceivedData");
        BitBuffer* buffer = Iso14443_4a_event->data->buffer;

        const NfcEinkScreenCommand* cmd = (NfcEinkScreenCommand*)bit_buffer_get_data(buffer);

        NfcEinkScreenResponse* response = malloc(250);
        uint8_t response_length = 0;

        if(cmd->command_code == 0xC2) {
            response_length = 1;
            response->response_code = 0xC2;
            command = NfcCommandSleep;
        } else if(cmd->command_code == 0xB3) {
            response_length = 1;
            response->response_code = 0xA2;
        } else if(cmd->command_code == 0xB2) {
            response_length = 1;
            response->response_code = 0xA3;

            instance->was_update = false;
            command = NfcCommandContinue;

            instance->response_cnt++;
            FURI_LOG_D(TAG, "Done B2: %d", instance->response_cnt);
            if(instance->response_cnt >= 20) {
                ctx->listener_state = NfcEinkScreenGoodisplayListenerStateUpdatedSuccefully;
                eink_goodisplay_on_done(instance);
                command = NfcCommandStop;
            }
        } else if(cmd->command_code == 0x13) {
            const APDU_Header* apdu = (APDU_Header*)cmd->command_data;
            if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xD2) {
                instance->was_update = true;
                response_length = nfc_eink_screen_command_D2(
                    instance->data, (APDU_Command*)apdu, &response->apdu_resp.apdu_response);
            }
            response->response_code = 0xA3;
        } else if(cmd->command_code == 0xF2) {
            if(instance->update_cnt < 4) {
                memcpy(response, cmd, 2);
                response_length = 2;
                instance->update_cnt++;
            } else {
                response_length = 3;
                response->response_code = 0x03;
                response->apdu_resp.apdu_response.status = __builtin_bswap16(0x9000);
            }
        } else if(cmd->command_code == 0x02 || cmd->command_code == 0x03) {
            const APDU_Header* apdu = (APDU_Header*)cmd->command_data;
            response->response_code = cmd->command_code;

            if(apdu->CLA_byte == 0 && apdu->CMD_code == 0xA4) {
                FURI_LOG_D(TAG, "00 A4");
                nfc_eink_screen_command_A4(
                    (APDU_Command_Select*)apdu, &response->apdu_resp.apdu_response);
                response_length = 3;
                if(cmd->command_code == 0x02) {
                    ctx->listener_state = NfcEinkScreenGoodisplayListenerStateWaitingForConfig;
                    eink_goodisplay_on_target_detected(instance);
                }
            } else if(apdu->CLA_byte == 0 && apdu->CMD_code == 0xB0) {
                FURI_LOG_D(TAG, "00 B0");
                response_length =
                    nfc_eink_screen_command_B0(
                        (APDU_Command_Read*)apdu, &response->apdu_resp.apdu_response_read) +
                    1 + 2;
            } else if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xDB) {
                FURI_LOG_D(TAG, "F0 DB");
                response_length =
                    nfc_eink_screen_command_DB(
                        instance, (APDU_Command*)apdu, &response->apdu_resp.apdu_response) +
                    1;
            } else if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xDA) {
                FURI_LOG_D(TAG, "F0 DA");
                response_length = nfc_eink_screen_command_DA(
                                      (APDU_Command*)apdu, &response->apdu_resp.apdu_response) +
                                  1;
            } else if(apdu->CLA_byte == 0xFF && apdu->CMD_code == 0xFF && !instance->was_update) {
                FURI_LOG_D(TAG, "FF FF");
                response_length = nfc_eink_screen_command_FF(
                                      (APDU_Command*)apdu, &response->apdu_resp.apdu_response) +
                                  1;
            } else if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xD4) {
                FURI_LOG_D(TAG, "F0 D4");
                response->response_code = 0xF2;
                response->apdu_resp.apdu_response.status = 0x0101;
                response_length = 2;
                instance->update_cnt = 0;
            } else {
                uint8_t* data = instance->data->image_data + instance->data->received_data;
                ctx->listener_state = NfcEinkScreenGoodisplayListenerStateReadingBlocks;
                memcpy(data, cmd->command_data, 2);
                instance->data->received_data += 2;
                FURI_LOG_D(TAG, "Data: %d", instance->data->received_data);
                response_length = 3;
                response->response_code = 0x02;
                response->apdu_resp.apdu_response.status = __builtin_bswap16(0x9000);
            }
        } else {
            ///TODO: throw this error in some other places, because real reader can
            ///send some other commands which are not mentioned here, therefore this will
            ///result into permanent errors

            //ctx->listener_state = NfcEinkScreenGoodisplayListenerStateIdle;
            //eink_goodisplay_on_error(instance);
        }

        bit_buffer_reset(instance->tx_buf);
        bit_buffer_append_bytes(instance->tx_buf, (uint8_t*)response, response_length);

        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        free(response);

        //iso14443_3a_listener_send_standard_frame(event.instance, instance->tx_buf); Not allowed
        nfc_listener_tx(event.instance, instance->tx_buf);
    }
    return command;
}
