#include "nb_usb_ccid.h"

#include <furi_hal_usb.h>
#include <furi_hal_version.h>
#include <usb.h>
#include <usb_std.h>
#include <usb_ccid.h>
#include <usbd_core.h>
#include <stdlib.h>
#include <string.h>

#define EP_OUT    0x01
#define EP_IN     0x82
#define EP_INT    0x83
#define EP_INT_SZ 8

struct CcidConfigDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_ccid_descriptor ccid;
    struct usb_endpoint_descriptor ep_out;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_int;
} FURI_PACKED;

_Static_assert(
    sizeof(struct usb_ccid_descriptor) == 0x36,
    "CCID class descriptor must be 54 bytes");

static const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC("Flipper Devices Inc.");

static const struct usb_device_descriptor ccid_device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8, /* USB_EP0_SIZE in the firmware's private header */
    .idVendor = NB_USB_VID,
    .idProduct = NB_USB_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct CcidConfigDescriptor ccid_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct CcidConfigDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_CCID,
            .bInterfaceSubClass = USB_CCID_SUBCLASS,
            .bInterfaceProtocol = USB_CCID_PROTO_CCID,
            .iInterface = NO_DESCRIPTOR,
        },
    .ccid =
        {
            .bLength = sizeof(struct usb_ccid_descriptor),
            .bDescriptorType = USB_DTYPE_CCID_FUNCTIONAL,
            .bcdCCID = CCID_CURRENT_SPEC_RELEASE_NUMBER,
            .bMaxSlotIndex = 0,
            .bVoltageSupport = 0x01, /* 5 V */
            .dwProtocols = 0x00000003, /* T=0 and T=1 */
            .dwDefaultClock = 4000,
            .dwMaximumClock = 4000,
            .bNumClockSupported = 0,
            .dwDataRate = 10752,
            .dwMaxDataRate = 344064,
            .bNumDataRatesSupported = 0,
            .dwMaxIFSD = 254,
            .dwSynchProtocols = 0,
            .dwMechanical = 0,
            /* short-APDU exchange level + automatic parameters / voltage / clock / baud
             * + 0x80 "automatic PPS made by the CCID": without it libccid sends a PPS request
             * after power-on, and a contactless card cannot answer it ("Card is unresponsive") */
            .dwFeatures = 0x000200BA,
            .dwMaxCCIDMessageLength = 271, /* 10 byte header + 261 data */
            .bClassGetResponse = 0xFF,
            .bClassEnvelope = 0xFF,
            .wLcdLayout = 0,
            .bPINSupport = 0,
            .bMaxCCIDBusySlots = 1,
        },
    .ep_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = EP_OUT,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = NB_USB_CCID_PKT,
            .bInterval = 0,
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = NB_USB_CCID_PKT,
            .bInterval = 0,
        },
    .ep_int =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = EP_INT,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = EP_INT_SZ,
            .bInterval = 8,
        },
};

static void ccid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void ccid_deinit(usbd_device* dev);
static void ccid_on_wakeup(usbd_device* dev);
static void ccid_on_suspend(usbd_device* dev);
static usbd_respond ccid_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond ccid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

/* NOT called usb_ccid: newer firmware already exports a built-in interface with that name */
static FuriHalUsbInterface nb_ccid_iface = {
    .init = ccid_init,
    .deinit = ccid_deinit,
    .wakeup = ccid_on_wakeup,
    .suspend = ccid_on_suspend,
    .dev_descr = (struct usb_device_descriptor*)&ccid_device_desc,
    .str_manuf_descr = (void*)&dev_manuf_desc,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)&ccid_cfg_desc,
};

static usbd_device* usb_dev;
static FuriHalUsbInterface* cur_if;
static FuriHalUsbInterface* prev_if;
static const NbUsbCcidCallbacks* cbs;
static void* cb_ctx;
static volatile bool connected;

static struct usb_string_descriptor* make_string(const char* s, const char* prefix) {
    size_t pl = prefix ? strlen(prefix) : 0;
    size_t sl = s ? strlen(s) : 0;
    struct usb_string_descriptor* d = malloc((pl + sl) * 2 + 2);
    d->bLength = (uint8_t)((pl + sl) * 2 + 2);
    d->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < pl; i++)
        d->wString[i] = (uint8_t)prefix[i];
    for(size_t i = 0; i < sl; i++)
        d->wString[pl + i] = (uint8_t)s[i];
    return d;
}

static void ccid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(ctx);
    usb_dev = dev;
    cur_if = intf;

    cur_if->str_prod_descr = make_string("Flipper NFC Reader", NULL);
    cur_if->str_serial_descr = make_string(furi_hal_version_get_name_ptr(), "flip_");

    usbd_reg_config(dev, ccid_ep_config);
    usbd_reg_control(dev, ccid_control);
    usbd_connect(dev, true);
}

static void ccid_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    free(cur_if->str_prod_descr);
    free(cur_if->str_serial_descr);
    cur_if->str_prod_descr = NULL;
    cur_if->str_serial_descr = NULL;
    cur_if = NULL;
}

static void ccid_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    connected = true;
    if(cbs && cbs->connected) cbs->connected(cb_ctx, true);
}

static void ccid_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    connected = false;
    if(cbs && cbs->connected) cbs->connected(cb_ctx, false);
}

static void ccid_ep_cb(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    if(!cbs) return;
    if(event == usbd_evt_eprx && ep == EP_OUT) {
        if(cbs->rx) cbs->rx(cb_ctx);
    } else if(event == usbd_evt_eptx && ep == EP_IN) {
        if(cbs->tx_done) cbs->tx_done(cb_ctx);
    }
}

static usbd_respond ccid_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0: /* deconfigure */
        usbd_ep_deconfig(dev, EP_INT);
        usbd_ep_deconfig(dev, EP_IN);
        usbd_ep_deconfig(dev, EP_OUT);
        usbd_reg_endpoint(dev, EP_OUT, 0);
        usbd_reg_endpoint(dev, EP_IN, 0);
        return usbd_ack;
    case 1: /* configure */
        usbd_ep_config(dev, EP_OUT, USB_EPTYPE_BULK, NB_USB_CCID_PKT);
        usbd_ep_config(dev, EP_IN, USB_EPTYPE_BULK, NB_USB_CCID_PKT);
        usbd_ep_config(dev, EP_INT, USB_EPTYPE_INTERRUPT, EP_INT_SZ);
        usbd_reg_endpoint(dev, EP_OUT, ccid_ep_cb);
        usbd_reg_endpoint(dev, EP_IN, ccid_ep_cb);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond ccid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(callback);
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case CCID_ABORT: /* we never have a long-running command to abort */
            return usbd_ack;
        default: /* clock / data-rate tables: not advertised, so not asked for */
            return usbd_fail;
        }
    }
    return usbd_fail;
}

bool nb_usb_ccid_start(const NbUsbCcidCallbacks* cb, void* ctx) {
    cbs = cb;
    cb_ctx = ctx;
    prev_if = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&nb_ccid_iface, NULL)) {
        cbs = NULL;
        return false;
    }
    return true;
}

void nb_usb_ccid_stop(void) {
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(prev_if ? prev_if : &usb_cdc_single, NULL);
    cbs = NULL;
}

int32_t nb_usb_ccid_read(uint8_t* buf, uint16_t max) {
    if(!usb_dev) return 0;
    int32_t n = usbd_ep_read(usb_dev, EP_OUT, buf, max);
    return n < 0 ? 0 : n;
}

int32_t nb_usb_ccid_write(const uint8_t* buf, uint16_t len) {
    if(!usb_dev) return -1;
    return usbd_ep_write(usb_dev, EP_IN, buf, len);
}

int32_t nb_usb_ccid_notify(const uint8_t* buf, uint16_t len) {
    if(!usb_dev) return -1;
    return usbd_ep_write(usb_dev, EP_INT, buf, len);
}
