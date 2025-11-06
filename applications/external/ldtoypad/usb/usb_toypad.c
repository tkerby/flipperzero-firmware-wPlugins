#include <furi_hal_version.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <furi.h>
#include "usb.h"
#include "usb_hid.h"

#include "../views/EmulateToyPad_scene.h"
#include "../tea.h"
#include "../burtle.h"
#include "../minifigures.h"
#include "save_toypad.h"
#include "bytes.h"
#include "descriptors.h"

// Define all the possible commands
#define CMD_WAKE   0xB0
#define CMD_READ   0xD2
#define CMD_MODEL  0xD4
#define CMD_SEED   0xB1
#define CMD_CHAL   0xB3
#define CMD_COL    0xC0
#define CMD_GETCOL 0xC1
#define CMD_FADE   0xC2
#define CMD_FLASH  0xC3
#define CMD_FADRD  0xC4
#define CMD_FADAL  0xC6
#define CMD_FLSAL  0xC7
#define CMD_COLAL  0xC8
#define CMD_TGLST  0xD0
#define CMD_WRITE  0xD3
#define CMD_PWD    0xE1
#define CMD_ACTIVE 0xE5
#define CMD_LEDSQ  0xFF

#define HID_INTERVAL 1

#define USB_EP0_SIZE 64
PLACE_IN_SECTION("MB_MEM2") static uint32_t ubuf[0x20];

ToyPadEmu* emulator;

int connected_status = ConnectedStatusDisconnected;
int get_connected_status() {
    return connected_status;
}
void set_connected_status(int status) {
    connected_status = status;
}

char debug_text[64] = " ";

void set_debug_text(char* text) {
    snprintf(debug_text, sizeof(debug_text), "%s", text);
}

char* get_debug_text() {
    return debug_text;
}

// Function to calculate checksum
void calculate_checksum(uint8_t* buf, int length, int place) {
    uint8_t checksum = 0;

    if(place == -1) {
        place = length;
    }

    // Calculate checksum (up to 'length')
    for(int i = 0; i < length; i++) {
        checksum = (checksum + buf[i]) % 256;
    }

    // Assign checksum to the last position
    buf[place] = checksum;
}

Token* find_token_by_index(ToyPadEmu* emulator, int index) {
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL && emulator->tokens[i]->index == index) {
            return emulator->tokens[i];
        }
    }
    return NULL;
}

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void hid_deinit(usbd_device* dev);
static void hid_on_wakeup(usbd_device* dev);
static void hid_on_suspend(usbd_device* dev);

FuriHalUsbInterface usb_hid_ldtoypad = {
    .init = hid_init,
    .deinit = hid_deinit,
    .wakeup = hid_on_wakeup,
    .suspend = hid_on_suspend,

    .dev_descr = (struct usb_device_descriptor*)&hid_device_desc,

    .str_manuf_descr = NULL,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&hid_cfg_desc,
};

// static bool hid_send_report(uint8_t report_id);
static usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static usbd_device* usb_dev;
static bool hid_connected = false;
static HidStateCallback callback;
static void* cb_ctx;
static bool boot_protocol = false;

void furi_hal_hid_set_state_callback(HidStateCallback cb, void* ctx) {
    if(callback != NULL) {
        if(hid_connected == true) callback(false, cb_ctx);
    }

    callback = cb;
    cb_ctx = ctx;

    if(callback != NULL) {
        if(hid_connected == true) callback(true, cb_ctx);
    }
}

static void* hid_set_string_descr(char* str) {
    furi_assert(str);

    size_t len = strlen(str);
    struct usb_string_descriptor* dev_str_desc = malloc(len * 2 + 2);
    dev_str_desc->bLength = len * 2 + 2;
    dev_str_desc->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++)
        dev_str_desc->wString[i] = str[i];

    return dev_str_desc;
}

usbd_device* get_usb_device() {
    return usb_dev;
}

Burtle* burtle; // Define the Burtle object

void create_uid(Token* token, int id) {
    char version_name[7];
    snprintf(version_name, sizeof(version_name), "%s", furi_hal_version_get_name_ptr());

    token->uid[0] = 0x04; // uid always 0x04

    // when token is a vehicle we want a random uid for upgrades etc when creating a new vehicle
    if(!token->id) {
        for(int i = 1; i <= 5; i++) {
            token->uid[i] = rand() % 256;
        }
    } else {
        int count = get_token_count_of_specific_id(id);

        // Generate UID for a minfig, that is always the same for your Flipper Zero
        for(int i = 1; i <= 5; i++) {
            // Combine id, version_name, and index for a hash
            token->uid[i] =
                (uint8_t)((id * 31 + count * 17 + version_name[i % sizeof(version_name)]) % 256);
        }
    }

    token->uid[6] = 0x80; // last uid byte always 0x80
}

Token* createCharacter(int id) {
    Token* token = (Token*)malloc(sizeof(Token)); // Allocate memory for the token

    token->id = id; // Set the ID

    create_uid(token, id); // Create the UID

    // convert the name to a string
    snprintf(token->name, sizeof(token->name), "%s", get_minifigure_name(id));

    return token; // Return the created token
}

Token* createVehicle(int id, uint32_t upgrades[2]) {
    Token* token = (Token*)malloc(sizeof(Token));

    // Initialize the token data to zero
    memset(token, 0, sizeof(Token));

    // Dont set an ID for vehicles, it is in the token buffer already, we can use this to see if it is an minfig or vehicle by checking if the ID set or not

    // Generate a random UID and store it
    create_uid(token, id);

    // Write the upgrades and ID to the token data
    writeUInt32LE(&token->token[0x23 * 4], upgrades[0], 0); // Upgrades[0]
    writeUInt16LE(&token->token[0x24 * 4], id, 0); // ID
    writeUInt32LE(&token->token[0x25 * 4], upgrades[1], 0); // Upgrades[1]
    writeUInt16BE(&token->token[0x26 * 4], 1, 0); // Constant value 1 (Big Endian)

    snprintf(token->name, sizeof(token->name), "%s", get_vehicle_name(id));

    return token;
}

// Remove a token
bool ToyPadEmu_remove(int index) {
    if(index < 0 || index >= MAX_TOKENS || emulator->tokens[index] == NULL) {
        return false; // Invalid index or already removed
    }

    // Send removal command (assuming this is already implemented)
    unsigned char buffer[32] = {0};
    buffer[0] = FRAME_TYPE_REQUEST; // Magic number
    buffer[1] = 0x0b; // Size
    buffer[2] = emulator->tokens[index]->pad; // Pad number
    buffer[3] = 0x00;
    buffer[4] = index; // Index of token to remove
    buffer[5] = 0x01; // Tag removed (not placed)
    memcpy(&buffer[6], emulator->tokens[index]->uid, 7); // UID
    buffer[13] = generate_checksum(buffer, 13);

    usbd_ep_write(get_usb_device(), HID_EP_IN, buffer, sizeof(buffer));

    if(emulator->tokens[index] != NULL) {
        // Free the token and clear the slot
        free(emulator->tokens[index]);
    }

    emulator->tokens[index] = NULL;

    return true;
}

void ToyPadEmu_clear() {
    // clear the tokens on the emulator and free the memory
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            free(emulator->tokens[i]);
            emulator->tokens[i] = NULL;
        }
    }
    memset(emulator->tokens, 0, sizeof(emulator->tokens));

    // clear all box info
    for(int i = 0; i < NUM_BOXES; i++) {
        boxInfo[i].isFilled = false;
        boxInfo[i].index = -1; // Reset index
    }
}

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    FuriHalUsbHidConfig* cfg = (FuriHalUsbHidConfig*)ctx;
    usb_dev = dev;

    if(burtle == NULL) burtle = malloc(sizeof(Burtle));

    usb_hid.dev_descr->iManufacturer = 0;
    usb_hid.dev_descr->iProduct = 0;
    usb_hid.str_manuf_descr = NULL;
    usb_hid.str_prod_descr = NULL;
    usb_hid.dev_descr->idVendor = HID_VID_TOYPAD;
    usb_hid.dev_descr->idProduct = HID_PID_TOYPAD;

    if(cfg != NULL) {
        usb_hid.dev_descr->idVendor = cfg->vid;
        usb_hid.dev_descr->idProduct = cfg->pid;

        if(cfg->manuf[0] != '\0') {
            usb_hid.str_manuf_descr = hid_set_string_descr(cfg->manuf);
            usb_hid.dev_descr->iManufacturer = UsbDevManuf;
        }

        if(cfg->product[0] != '\0') {
            usb_hid.str_prod_descr = hid_set_string_descr(cfg->product);
            usb_hid.dev_descr->iProduct = UsbDevProduct;
        }
    }

    usbd_reg_config(dev, hid_ep_config);
    usbd_reg_control(dev, hid_control);

    // Manually initialize the USB because of the custom modiefied USB_EP0_SIZE to 64 from the default 8
    // I did this because the official Toy Pad has a 64 byte EP0 size
    usbd_init(dev, &usbd_hw, USB_EP0_SIZE, ubuf, sizeof(ubuf));

    usbd_connect(dev, true);

    uint8_t default_tea_key[16] = {
        0x55,
        0xFE,
        0xF6,
        0xB0,
        0x62,
        0xBF,
        0x0B,
        0x41,
        0xC9,
        0xB3,
        0x7C,
        0xB4,
        0x97,
        0x3E,
        0x29,
        0x7B};

    memcpy(emulator->tea_key, default_tea_key, sizeof(emulator->tea_key));
}

static void hid_deinit(usbd_device* dev) {
    // Set the USB Endpoint 0 size back to 8
    usbd_init(dev, &usbd_hw, 8, ubuf, sizeof(ubuf));

    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    free(usb_hid_ldtoypad.str_manuf_descr);
    free(usb_hid_ldtoypad.str_prod_descr);

    free(burtle);

    connected_status =
        ConnectedStatusCleanupWanted; // disconnected, needs cleanup outside of the ISR context
}

static void hid_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    if(!hid_connected) {
        hid_connected = true;
        if(callback != NULL) {
            callback(true, cb_ctx);
        }
    }
}

static void hid_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    if(hid_connected) {
        hid_connected = false;
        if(callback != NULL) {
            callback(false, cb_ctx);
        }
        connected_status =
            ConnectedStatusCleanupWanted; // disconnected, needs cleanup outside of the ISR context
    }
}

void hid_in_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);
    UNUSED(dev);

    // nothing to do here
}

void hid_out_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);

    usb_dev = dev;

    unsigned char req_buf[HID_EP_SZ] = {0};

    // Read data from the OUT endpoint
    int32_t len = usbd_ep_read(dev, HID_EP_OUT, req_buf, HID_EP_SZ);

    if(len <= 0) return;

    Frame frame;
    parse_frame(&frame, req_buf, len);

    if(frame.len == 0) return;

    Request request;

    memset(&request, 0, sizeof(Request));

    // parse request
    parse_request(&request, &frame);

    Response response;
    memset(&response, 0, sizeof(Response));

    response.cid = request.cid;
    response.payload_len = 0;

    uint32_t conf;
    Token* token;

    switch(request.cmd) {
    case CMD_WAKE:
        sprintf(debug_text, "CMD_WAKE");

        // From: https://github.com/AlinaNova21/node-ld/blob/f54b177d2418432688673aa07c54466d2e6041af/src/lib/ToyPadEmu.js#L139
        uint8_t wake_payload[13] = {
            0x28, 0x63, 0x29, 0x20, 0x4C, 0x45, 0x47, 0x4F, 0x20, 0x32, 0x30, 0x31, 0x34};

        memcpy(response.payload, wake_payload, sizeof(wake_payload));

        response.payload_len = sizeof(wake_payload);

        connected_status = ConnectedStatusReconnecting; // Connected / Reconnected

        break;
    case CMD_READ:
        sprintf(debug_text, "CMD_READ");

        int ind = request.payload[0];
        int page = request.payload[1];

        response.payload_len = 17;

        memset(response.payload, 0, sizeof(response.payload));

        response.payload[0] = 0;

        // Find the token that matches the ind
        token = find_token_by_index(emulator, ind);

        if(token) {
            int start = page * 4;
            memcpy(response.payload + 1, token->token + start, 16);
        }

        break;
    case CMD_MODEL:
        if(!strstr(debug_text, "CMD_MODEL")) {
            snprintf(debug_text + strlen(debug_text), sizeof(debug_text), " CMD_MODEL");
        }

        tea_decrypt(request.payload, emulator->tea_key, request.payload);
        int index = request.payload[0];
        conf = readUInt32BE(request.payload, 4);
        unsigned char buf[8] = {0};
        writeUInt32BE(buf, conf, 4);
        token = NULL;

        token = find_token_by_index(emulator, index);

        memset(response.payload, 0, sizeof(response.payload));

        if(token) {
            if(token->id) {
                response.payload[0] = 0x00;
                writeUInt32LE(buf, token->id, 0);
                tea_encrypt(buf, emulator->tea_key, response.payload + 1);
                response.payload_len = 9;
            } else {
                response.payload[0] = 0xF9;
                response.payload_len = 1;
            }
        } else {
            response.payload[0] = 0xF2;
            response.payload_len = 1;
        }
        break;
    case CMD_SEED:
        sprintf(debug_text, "CMD_SEED");

        // decrypt the request.payload with the TEA
        tea_decrypt(request.payload, emulator->tea_key, request.payload);

        uint32_t seed = readUInt32LE(request.payload, 0);

        conf = readUInt32BE(request.payload, 4);

        burtle_init(burtle, seed);

        memset(response.payload, 0, 8); // Fill the payload with 0 with a length of 8
        writeUInt32BE(response.payload, conf, 0); // Write the conf to the payload

        // encrypt the request.payload with the TEA
        tea_encrypt(response.payload, emulator->tea_key, response.payload);

        response.payload_len = 8;

        break;
    case CMD_WRITE:
        sprintf(debug_text, "CMD_WRITE");

        // Extract index, page, and data
        ind = request.payload[0];
        page = request.payload[1];
        uint8_t* data = request.payload + 2;

        // Find the token
        Token* token = find_token_by_index(emulator, ind);
        if(token) {
            // Copy 4 bytes of data to token->token at offset 4 * page
            if(page >= 0 && page < 64) {
                memcpy(token->token + 4 * page, data, 4);
            }

            if(page == 24 || page == 36) {
                uint16_t vehicle_id = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
                snprintf(token->name, sizeof(token->name), "%s", get_vehicle_name(vehicle_id));
            }
        }

        // Prepare the response
        response.payload[0] = 0x00;
        response.payload_len = 1;

        break;
    case CMD_CHAL:
        sprintf(debug_text, "CMD_CHAL");

        // decrypt the request.payload with the TEA
        tea_decrypt(request.payload, emulator->tea_key, request.payload);

        // get conf
        conf = readUInt32BE(request.payload, 0);

        // make a new buffer for the response of 8
        memset(response.payload, 0, 8);

        // get a rand from the burtle
        uint32_t rand = burtle_rand(burtle);

        // write the rand to the response payload as Int32LE
        writeUInt32LE(response.payload, rand, 0);

        // write the conf to the response payload as Int32BE
        writeUInt32BE(response.payload + 4, conf, 0);

        // encrypt the response.payload with the TEA
        tea_encrypt(response.payload, emulator->tea_key, response.payload);

        response.payload_len = 8;

        break;
    case CMD_COL:
        sprintf(debug_text, "CMD_COL");
        break;
    case CMD_GETCOL:
        sprintf(debug_text, "CMD_GETCOL");
        break;
    case CMD_FADE:
        sprintf(debug_text, "CMD_FADE");
        break;
    case CMD_FLASH:
        sprintf(debug_text, "CMD_FLASH");
        break;
    case CMD_FADRD:
        sprintf(debug_text, "CMD_FADRD");
        break;
    case CMD_FADAL:
        if(!strstr(debug_text, "CMD_FADAL")) {
            snprintf(debug_text + strlen(debug_text), sizeof(debug_text), " CMD_FADAL");
        }
        break;
    case CMD_FLSAL:
        sprintf(debug_text, "CMD_FLSAL");
        break;
    case CMD_COLAL:
        sprintf(debug_text, "CMD_COLAL");
        break;
    case CMD_TGLST:
        sprintf(debug_text, "CMD_TGLST");
        break;
    case CMD_PWD:
        sprintf(debug_text, "CMD_PWD");
        break;
    case CMD_ACTIVE:
        sprintf(debug_text, "CMD_ACTIVE");
        break;
    case CMD_LEDSQ:
        sprintf(debug_text, "CMD_LEDSQ");
        break;
    default:
        sprintf(debug_text, "Not a valid command");
        return;
    }

    // check if the response is empty
    if(sizeof(response.payload) == 0) {
        // sprintf(debug_text, "Empty payload_len");
        return;
    }
    if(response.payload_len > HID_EP_SZ) {
        sprintf(debug_text, "Payload too big");
        return;
    }

    // Make the response
    unsigned char res_buf[HID_EP_SZ];

    build_response(&response, res_buf);
    int res_len = build_frame(&response.frame, res_buf);

    if(res_len <= 0) {
        sprintf(debug_text, "res_len is 0");
        return;
    }

    // Send the response
    usbd_ep_write(dev, HID_EP_IN, res_buf, sizeof(res_buf));
}

/* Configure endpoints */
static usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        /* deconfiguring device */
        usbd_ep_deconfig(dev, HID_EP_IN);
        usbd_ep_deconfig(dev, HID_EP_OUT);

        usbd_reg_endpoint(dev, HID_EP_IN, 0);
        usbd_reg_endpoint(dev, HID_EP_OUT, 0);
        return usbd_ack;
    case 1:
        /* configuring device */
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_reg_endpoint(dev, HID_EP_IN, hid_in_callback);
        usbd_ep_config(dev, HID_EP_OUT, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_reg_endpoint(dev, HID_EP_OUT, hid_out_callback);
        boot_protocol = false; /* BIOS will SET_PROTOCOL if it wants this */
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

/* Control requests handler */
static usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    /* HID control requests */
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        // case USB_HID_GETREPORT:
        //     if(boot_protocol == true) {
        //         dev->status.data_ptr = &hid_report.keyboard.boot;
        //         dev->status.data_count = sizeof(hid_report.keyboard.boot);
        //     } else {
        //         dev->status.data_ptr = &hid_report;
        //         dev->status.data_count = sizeof(hid_report);
        //     }
        //     return usbd_ack;
        case USB_HID_SETPROTOCOL:
            if(req->wValue == 0)
                boot_protocol = true;
            else if(req->wValue == 1)
                boot_protocol = false;
            else
                return usbd_fail;
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&(hid_cfg_desc.intf_0.hid_desc);
            dev->status.data_count = sizeof(hid_cfg_desc.intf_0.hid_desc);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            boot_protocol = false; /* BIOS does not read this */
            dev->status.data_ptr = (uint8_t*)hid_report_desc;
            dev->status.data_count = sizeof(hid_report_desc);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}
