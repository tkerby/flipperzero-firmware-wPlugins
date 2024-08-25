#include "../nfc_eink_app.h"

#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/nfc_protocol.h>
//#include <lib/nfc/helpers/iso14443_crc.h>
//#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
//#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

/* 
static void nfc_eink_scene_start_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
} */

/* NfcCommand nfc_eink_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkApp* instance = context;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;

    //Iso14443_3aListenerEventTypeReceivedData
    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedData) {
        FURI_LOG_D(TAG, "ReceivedData");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeFieldOff) {
        FURI_LOG_D(TAG, "FieldOff");
        view_dispatcher_send_custom_event(instance->view_dispatcher, CustomEventEmulationDone);
        command = NfcCommandStop;
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeHalted) {
        FURI_LOG_D(TAG, "Halted");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;

        const uint8_t* data = bit_buffer_get_data(buffer);
        FURI_LOG_D(TAG, "0x%02X, 0x%02X", data[0], data[1]);

        //TODO: move this to process function
        NfcEinkScreen* const screen = instance->screen;
        if(data[1] == 0x08) {
            memcpy(screen->image_data + screen->received_data, &data[3], data[2]);
            screen->received_data += data[2];

            uint16_t last = screen->received_data - 1;
            screen->image_data[last] &= 0xC0;
        }

        bit_buffer_reset(instance->tx_buf);
        bit_buffer_append_byte(instance->tx_buf, (data[1] == 0x0A) ? 0xFF : 0x00);
        bit_buffer_append_byte(instance->tx_buf, 0x00);
        nfc_listener_tx(event.instance, instance->tx_buf);
    }

    return command;
}
 */

#pragma pack(push, 1)
typedef struct {
    uint8_t CLA_byte;
    uint8_t CMD_code;
    uint8_t P1;
    uint8_t P2;
} APDU_Header;

typedef struct {
    APDU_Header header;
    uint8_t data_length;
    uint8_t data[];
} APDU_Command;

typedef APDU_Command APDU_Command_Select;

typedef struct {
    APDU_Header header;
    uint8_t expected_length;
} APDU_Command_Read;

typedef struct {
    uint8_t command_code;
    uint8_t command_data[];
    //APDU_Command* apdu_command;
} NfcEinkScreenCommand;

typedef struct {
    uint16_t status;
} APDU_Response;

typedef struct {
    uint16_t data_length; ///TODO: remove this to CC_File struct
    uint8_t data[];
} APDU_Response_Read;

typedef struct {
    uint16_t length;
    uint8_t ndef_message[];
} NDEF_File;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t file_id;
    uint16_t file_size;
    uint8_t read_access_flags;
    uint8_t write_access_flags;
} NDEF_File_Ctrl_TLV;

typedef struct {
    ///TODO: uint16_t data_length; //maybe this should be here instead of APDU_Response_Read
    uint8_t mapping_version;
    uint16_t r_apdu_max_size;
    uint16_t c_apdu_max_size;
    NDEF_File_Ctrl_TLV ctrl_file;
} CC_File;

typedef struct {
    uint8_t response_code;
    union {
        APDU_Response apdu_response;
        APDU_Response_Read apdu_response_read;
    } apdu_resp;
} NfcEinkScreenResponse;
#pragma pack(pop)

/* static uint8_t nfc_eink_screen_command_D4(const APDU_Command* command, APDU_Response* resp) {
    UNUSED(command);
    UNUSED(resp);

    //resp->status = __builtin_bswap16(0x9000);
    return 2;
} */

/*
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

static uint8_t nfc_eink_screen_command_DB(const APDU_Command* command, APDU_Response* resp) {
    const APDU_Header* hdr = &command->header;
    uint8_t length = 0;
    if(hdr->P1 == 0x02 && hdr->P2 == 0x00 && command->data_length == 0) {
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    } else if(hdr->P1 == 0x00 && hdr->P2 == 0x00 && command->data_length == 0x67) {
        ///TODO: Add here some saving and parsing logic for screen config
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
    FURI_LOG_D(TAG, "F0 D2");
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



NfcCommand nfc_eink_listener_callback11(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkApp* instance = context;
    Iso14443_4aListenerEvent* Iso14443_4a_event = event.event_data;

    if(Iso14443_4a_event->type == Iso14443_4aListenerEventTypeReceivedData) {
        //FURI_LOG_D(TAG, "ReceivedData");
        BitBuffer* buffer = Iso14443_4a_event->data->buffer;

        //const uint8_t rx_length = bit_buffer_get_size_bytes(buffer);
        const NfcEinkScreenCommand* cmd = (NfcEinkScreenCommand*)bit_buffer_get_data(buffer);
        //NfcEinkScreenCommand cmd = {0};
        //cmd.command_code = data[0];

        NfcEinkScreenResponse* response = malloc(250);
        uint8_t response_length = 0;

        if(cmd->command_code == 0xC2) {
            response_length = 1;
            response->response_code = 0xC2;
            command = NfcCommandSleep;
            if(!instance->was_update)
                command = NfcCommandSleep;
            else {
                FURI_LOG_D(TAG, "Done");
                furi_timer_start(instance->timer, furi_ms_to_ticks(5000));
                command = NfcCommandStop;
                instance->was_update = false;
            }
        } else if(cmd->command_code == 0xB3) {
            response_length = 1;
            response->response_code = 0xA2;
        } else if(cmd->command_code == 0xB2) {
            response_length = 1;
            response->response_code = 0xA3;
            FURI_LOG_D(TAG, "Done B2");
            furi_timer_start(instance->timer, furi_ms_to_ticks(5000));
            instance->was_update = false;
        } else if(cmd->command_code == 0x13) {
            const APDU_Header* apdu = (APDU_Header*)cmd->command_data;
            if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xD2) {
                instance->was_update = true;
                response_length = nfc_eink_screen_command_D2(
                    instance->screen->data,
                    (APDU_Command*)apdu,
                    &response->apdu_resp.apdu_response);
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
                nfc_eink_screen_command_A4(
                    (APDU_Command_Select*)apdu, &response->apdu_resp.apdu_response);
                response_length = 3;
            } else if(apdu->CLA_byte == 0 && apdu->CMD_code == 0xB0) {
                response_length =
                    nfc_eink_screen_command_B0(
                        (APDU_Command_Read*)apdu, &response->apdu_resp.apdu_response_read) +
                    1 + 2;
            } else if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xDB) {
                response_length = nfc_eink_screen_command_DB(
                                      (APDU_Command*)apdu, &response->apdu_resp.apdu_response) +
                                  1;
            } else if(apdu->CLA_byte == 0xF0 && apdu->CMD_code == 0xDA) {
                response_length = nfc_eink_screen_command_DA(
                                      (APDU_Command*)apdu, &response->apdu_resp.apdu_response) +
                                  1;
            } else if(apdu->CLA_byte == 0xFF && apdu->CMD_code == 0xFF) {
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
                uint8_t* data =
                    instance->screen->data->image_data + instance->screen->data->received_data;

                memcpy(data, cmd->command_data, 2);
                instance->screen->data->received_data += 2;

                response_length = 3;
                response->response_code = 0x02;
                response->apdu_resp.apdu_response.status = __builtin_bswap16(0x9000);
            }
        }

        bit_buffer_reset(instance->tx_buf);
        bit_buffer_append_bytes(instance->tx_buf, (uint8_t*)response, response_length);

        iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        free(response);

        //iso14443_3a_listener_send_standard_frame(event.instance, instance->tx_buf); Not allowed
        nfc_listener_tx(event.instance, instance->tx_buf);
    }
    return command;
} */
static void nfc_eink_emulate_callback(NfcEinkScreenEventType type, void* context) {
    furi_assert(context);
    NfcEinkApp* instance = context;
    NfcEinkAppCustomEvents event = NfcEinkAppCustomEventProcessFinish;
    switch(type) {
    case NfcEinkScreenEventTypeTargetDetected:
        event = NfcEinkAppCustomEventTargetDetected;
        FURI_LOG_D(TAG, "Target detected");
        break;
    case NfcEinkScreenEventTypeFinish:
        event = NfcEinkAppCustomEventProcessFinish;
        break;

    default:
        FURI_LOG_E(TAG, "Event: %02X nor implemented", type);
        furi_crash();
        break;
    }
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

void nfc_eink_scene_emulate_on_enter(void* context) {
    NfcEinkApp* instance = context;

    Widget* widget = instance->widget;
    //TextBox* text_box = instance->text_box;

    widget_add_icon_element(widget, 0, 3, &I_NFC_dolphin_emulation_51x64);
    widget_add_string_element(widget, 90, 26, AlignCenter, AlignCenter, FontPrimary, "Emulating");

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewWidget);

    /* const Iso14443_3aData* data =
        nfc_device_get_data(instance->screen->nfc_device, NfcProtocolIso14443_3a); */
    //instance->listener = nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_3a, data);

    /* const Iso14443_4aData* data =
        nfc_device_get_data(instance->screen->nfc_device, NfcProtocolIso14443_4a);
    instance->listener = nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_4a, data); */
    //nfc_listener_start(instance->listener, nfc_eink_listener_callback, instance);
    const NfcEinkScreen* screen = instance->screen;
    nfc_eink_screen_set_callback(instance->screen, nfc_eink_emulate_callback, instance);

    NfcDevice* nfc_device = screen->device->nfc_device;
    NfcProtocol protocol = nfc_device_get_protocol(nfc_device);
    const NfcDeviceData* data = nfc_device_get_data(nfc_device, protocol);
    instance->listener = nfc_listener_alloc(instance->nfc, protocol, data);
    nfc_listener_start(instance->listener, screen->handlers->listener_callback, instance->screen);

    nfc_eink_blink_emulate_start(instance);
}

bool nfc_eink_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    /*   if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        if(submenu_index == SubmenuIndexEmulate) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneChooseType);
            consumed = true;
        } else if(submenu_index == SubmenuIndexWrite) {
            //scene_manager_next_scene(scene_manager, );
            consumed = true;
        } else if(submenu_index == SubmenuIndexRead) {
            //scene_manager_next_scene(scene_manager, );
            consumed = true;
        }
    } */

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcEinkAppCustomEventProcessFinish) {
        scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneResultMenu);
        notification_message(instance->notifications, &sequence_success);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_emulate_on_exit(void* context) {
    NfcEinkApp* instance = context;
    nfc_listener_stop(instance->listener);
    nfc_eink_blink_stop(instance);
    widget_reset(instance->widget);
}
