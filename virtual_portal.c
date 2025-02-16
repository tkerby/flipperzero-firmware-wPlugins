#include "virtual_portal.h"

#define TAG "VirtualPortal"

#define BLOCK_SIZE 16

static const NotificationSequence pof_sequence_cyan = {
    &message_blink_start_10,
    &message_blink_set_color_cyan,
    NULL,
};

VirtualPortal* virtual_portal_alloc(NotificationApp* notifications) {
    VirtualPortal* virtual_portal = malloc(sizeof(VirtualPortal));
    virtual_portal->notifications = notifications;

    for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
        virtual_portal->tokens[i] = pof_token_alloc();
    }
    virtual_portal->sequence_number = 0;
    virtual_portal->active = false;

    return virtual_portal;
}

void virtual_portal_cleanup(VirtualPortal* virtual_portal) {
    notification_message(virtual_portal->notifications, &sequence_reset_rgb);
    notification_message(virtual_portal->notifications, &sequence_display_backlight_on);
}

void virtual_portal_free(VirtualPortal* virtual_portal) {
    for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
        pof_token_free(virtual_portal->tokens[i]);
        virtual_portal->tokens[i] = NULL;
    }

    free(virtual_portal);
}

NotificationMessage message_red = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 0xFF,
};
NotificationMessage message_green = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 0xFF,
};
NotificationMessage message_blue = {
    .type = NotificationMessageTypeLedBlue,
    .data.led.value = 0xFF,
};
NotificationMessage message_display_backlight = {
    .type = NotificationMessageTypeLedDisplayBacklight,
    .data.led.value = 0xFF,
};
const NotificationSequence sequence_set_backlight = {
    &message_display_backlight,
    &message_do_not_reset,
    NULL,
};
const NotificationSequence sequence_set_leds = {
    &message_red,
    &message_green,
    &message_blue,
    &message_do_not_reset,
    NULL,
};


void virtaul_portal_set_leds(VirtualPortal* virtual_portal, uint8_t r, uint8_t g, uint8_t b) {
    message_red.data.led.value = r;
    message_green.data.led.value = g;
    message_blue.data.led.value = b;
    notification_message(virtual_portal->notifications, &sequence_set_leds);
}
void virtaul_portal_set_backlight(VirtualPortal* virtual_portal, uint8_t brightness) {
    message_display_backlight.data.led.value = brightness;
    notification_message(virtual_portal->notifications, &sequence_set_backlight);
}

void virtual_portal_load_token(VirtualPortal* virtual_portal, PoFToken* pof_token) {
    furi_assert(pof_token);
    FURI_LOG_D(TAG, "virtual_portal_load_token");
    PoFToken* target = NULL;
    uint8_t empty[4] = {0, 0, 0, 0};

    // first try to "reload" to the same slot it used before based on UID
    for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
        if(memcmp(virtual_portal->tokens[i]->UID, pof_token->UID, sizeof(pof_token->UID)) == 0) {
            // Found match
            if(virtual_portal->tokens[i]->loaded) {
                // already loaded, no-op
                return;
            } else {
                FURI_LOG_D(TAG, "Found matching UID at index %d", i);
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    // otherwise load into first slot with no set UID
    if(target == NULL) {
        for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
            if(memcmp(virtual_portal->tokens[i]->UID, empty, sizeof(empty)) == 0) {
                FURI_LOG_D(TAG, "Found empty UID at index %d", i);
                // By definition an empty UID slot would not be loaded, so I'm not checking.  Fight me.
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    // Re-use first unloaded slot
    if(target == NULL) {
        for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
            if(virtual_portal->tokens[i]->loaded == false) {
                FURI_LOG_D(TAG, "Re-using previously used slot %d", i);
                target = virtual_portal->tokens[i];
                break;
            }
        }
    }

    if(target == NULL) {
        FURI_LOG_W(TAG, "Failed to find slot to token into");
        return;
    }
    furi_assert(target);

    // TODO: make pof_token_copy()
    target->change = pof_token->change;
    target->loaded = pof_token->loaded;
    memcpy(target->dev_name, pof_token->dev_name, sizeof(pof_token->dev_name));
    memcpy(target->UID, pof_token->UID, sizeof(pof_token->UID));

    const NfcDeviceData* data = nfc_device_get_data(pof_token->nfc_device, NfcProtocolMfClassic);
    nfc_device_set_data(target->nfc_device, NfcProtocolMfClassic, data);
}

uint8_t virtual_portal_next_sequence(VirtualPortal* virtual_portal) {
    if(virtual_portal->sequence_number == 0xff) {
        virtual_portal->sequence_number = 0;
    }
    return virtual_portal->sequence_number++;
}

int virtual_portal_activate(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    FURI_LOG_D(TAG, "process %c", message[0]);
    virtual_portal->active = (message[1] == 1);

    response[0] = message[0];
    response[1] = message[1];
    response[2] = 0xFF;
    response[3] = 0x77;
    return 4;
}

int virtual_portal_reset(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    FURI_LOG_D(TAG, "process %c", message[0]);

    virtual_portal->active = false;
    //virtual_portal->sequence_number = 0;
    for(int i = 0; i < POF_TOKEN_LIMIT; i++) {
        if(virtual_portal->tokens[i]->loaded) {
            virtual_portal->tokens[i]->change = true;
        }
    }

    uint8_t index = 0;
    response[index++] = 'R';
    response[index++] = 0x02;
    response[index++] = 0x19;
    //response[index++] = 0x0a;
    //response[index++] = 0x03;
    //response[index++] = 0x02;
    // https://github.com/tresni/PoweredPortals/wiki/USB-Protocols
    // Wii Wireless: 01 29 00 00
    // Wii Wired: 02 0a 03 02 (Giants: works)
    // Arduboy: 02 19 (Trap team: works)
    return index;
}

int virtual_portal_status(VirtualPortal* virtual_portal, uint8_t* response) {
    response[0] = 'S';

    bool update = false;
    for(size_t i = 0; i < POF_TOKEN_LIMIT; i++) {
        // Can't use bit_lib since it uses the opposite endian
        if(virtual_portal->tokens[i]->loaded) {
            response[1 + i / 4] |= 1 << ((i % 4) * 2 + 0);
        }
        if(virtual_portal->tokens[i]->change) {
            update = true;
            response[1 + i / 4] |= 1 << ((i % 4) * 2 + 1);
        }

        virtual_portal->tokens[i]->change = false;
    }
    response[5] = virtual_portal_next_sequence(virtual_portal);
    response[6] = 1;

    // Let me know when a status that actually has a change is sent
    if(update) {
        char display[33] = {0};
        memset(display, 0, sizeof(display));
        for(size_t i = 0; i < BLOCK_SIZE; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", response[i]);
        }
        FURI_LOG_I(TAG, "> S %s", display);
    }

    return 7;
}

int virtual_portal_send_status(VirtualPortal* virtual_portal, uint8_t* response) {
    if(virtual_portal->active) {
        // Disable while I work on RGB
        // notification_message(virtual_portal->notifications, &pof_sequence_cyan);
        UNUSED(pof_sequence_cyan);
        return virtual_portal_status(virtual_portal, response);
    }
    return 0;
}

// 4d01ff0000d0077d6c2a77a400000000
int virtual_portal_m(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    virtual_portal->speaker = (message[1] == 1);

    /*
    char display[33] = {0};
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "M %s", display);
    */

    size_t index = 0;
    response[index++] = 'M';
    response[index++] = message[1];
    response[index++] = 0x00;
    response[index++] = 0x19;
    return index;
}

int virtual_portal_l(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    UNUSED(virtual_portal);

    /*
    char display[33] = {0};
    memset(display, 0, sizeof(display));
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "L %s", display);
    */

    uint8_t side = message[1]; // 0: left, 2: right
    uint8_t brightness = 0;
    switch(side) {
    case 0:
    case 2:
        virtaul_portal_set_leds(virtual_portal, message[2], message[3], message[4]);
        break;
    case 1:
        brightness = message[2];
        virtaul_portal_set_backlight(virtual_portal, brightness);
        break;
    case 3:
        brightness = 0xff;
        virtaul_portal_set_backlight(virtual_portal, brightness);
        break;
    }

    // https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/
    size_t index = 0;
    response[index++] = 'J';
    return index;
}

int virtual_portal_j(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    UNUSED(virtual_portal);

    /*
    char display[33] = {0};
    memset(display, 0, sizeof(display));
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[i]);
    }
    FURI_LOG_I(TAG, "J %s", display);
    */

    uint8_t side = message[1]; // 0: left, 2: right
    uint8_t r = message[2]; // 0: left, 2: right
    uint8_t g = message[3]; // 0: left, 2: right
    uint8_t b = message[4]; // 0: left, 2: right
    uint16_t delay = message[6] << 8 | message[5];
    switch(side) {
    case 0:
    case 2:
        virtaul_portal_set_leds(virtual_portal, r, g, b);
        break;
    }

    // Delay response
    // furi_delay_ms(delay); // causes issues
    UNUSED(delay);

    // https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/
    size_t index = 0;
    response[index++] = 'J';
    return index;
}

int virtual_portal_query(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    int index = message[1];
    int blockNum = message[2];
    int arrayIndex = index & 0x0f;
    FURI_LOG_I(TAG, "Query %d %d", arrayIndex, blockNum);

    PoFToken* pof_token = virtual_portal->tokens[arrayIndex];
    if(!pof_token->loaded) {
        response[0] = 'Q';
        response[1] = 0x00 | arrayIndex;
        response[2] = blockNum;
        return 3;
    }
    NfcDevice* nfc_device = pof_token->nfc_device;
    const MfClassicData* data = nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
    const MfClassicBlock block = data->block[blockNum];

    response[0] = 'Q';
    response[1] = 0x10 | arrayIndex;
    response[2] = blockNum;
    memcpy(response + 3, block.data, BLOCK_SIZE);
    return 3 + BLOCK_SIZE;
}

int virtual_portal_write(VirtualPortal* virtual_portal, uint8_t* message, uint8_t* response) {
    int index = message[1];
    int blockNum = message[2];
    int arrayIndex = index & 0x0f;

    char display[33] = {0};
    for(size_t i = 0; i < BLOCK_SIZE; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", message[3 + i]);
    }
    FURI_LOG_I(TAG, "Write %d %d %s", arrayIndex, blockNum, display);

    PoFToken* pof_token = virtual_portal->tokens[arrayIndex];
    if(!pof_token->loaded) {
        response[0] = 'W';
        response[1] = 0x00 | arrayIndex;
        response[2] = blockNum;
        return 3;
    }

    NfcDevice* nfc_device = pof_token->nfc_device;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(nfc_device, NfcProtocolMfClassic, data);
    MfClassicBlock* block = &data->block[blockNum];

    memcpy(block->data, message + 3, BLOCK_SIZE);
    nfc_device_set_data(nfc_device, NfcProtocolMfClassic, data);

    mf_classic_free(data);

    response[0] = 'W';
    response[1] = 0x10 | arrayIndex;
    response[2] = blockNum;
    return 3;
}

// 32 byte message, 32 byte response;
int virtual_portal_process_message(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t* response) {
    memset(response, 0, 32);
    switch(message[0]) {
    case 'A':
        return virtual_portal_activate(virtual_portal, message, response);
    case 'C': //Ring color R G B
        virtaul_portal_set_leds(virtual_portal, message[1], message[2], message[3]);
        return 0;
    case 'J':
        // https://github.com/flyandi/flipper_zero_rgb_led
        return virtual_portal_j(virtual_portal, message, response);
    case 'L':
        return virtual_portal_l(virtual_portal, message, response);
    case 'M':
        return virtual_portal_m(virtual_portal, message, response);
    case 'Q': //Query
        return virtual_portal_query(virtual_portal, message, response);
    case 'R':
        return virtual_portal_reset(virtual_portal, message, response);
    case 'S': //Status
        return virtual_portal_status(virtual_portal, response);
    case 'V':
        return 0;
    case 'W': //Write
        return virtual_portal_write(virtual_portal, message, response);
    case 'Z':
        return 0;
    default:
        FURI_LOG_W(TAG, "Unhandled command %c", message[0]);
        return 0; //No response
    }

    return 0;
}
