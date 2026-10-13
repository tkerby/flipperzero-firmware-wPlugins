// USB HID interface for the jiggler. Modelled on the firmware's own HID
// interface (targets/f7/furi_hal/furi_hal_usb_hid.c) with one addition: a
// vendor-defined collection carrying an 8-byte output report from the host
// and an 8-byte input report back. That is the channel the desktop helper
// uses to say how long ago the person last touched the real mouse or keyboard.

#include "usb_hid_jig.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <usb.h>
#include <usb_hid.h>

#define HID_EP_IN  0x81
#define HID_EP_OUT 0x01
#define HID_EP_SZ  0x10
#define VND_EP_IN  0x82
#define VND_EP_OUT 0x02
#define VND_EP_SZ  0x10

#define HID_INTERVAL 2

// Same identity as the stock Remote app
#define JIG_VID     0x046D
#define JIG_PID     0xC529
#define JIG_MANUF   "Flipper Devices Inc."
#define JIG_PRODUCT "Jiggler"

#define JIG_VENDOR_PAGE 0xFF4A

enum {
    UsbStrLang = 0,
    UsbStrManuf = 1,
    UsbStrProduct = 2,
};

enum ReportId {
    ReportIdKeyboard = 1,
    ReportIdMouse = 2,
};

struct HidIntfDescriptor {
    struct usb_interface_descriptor hid;
    struct usb_hid_descriptor hid_desc;
    struct usb_endpoint_descriptor hid_ep_in;
    struct usb_endpoint_descriptor hid_ep_out;
};

// Interface 0 is the mouse and keyboard. Interface 1 is the helper channel,
// kept separate so the host can open it without keyboard-level permissions.
struct HidConfigDescriptor {
    struct usb_config_descriptor config;
    struct HidIntfDescriptor intf_0;
    struct HidIntfDescriptor intf_1;
} FURI_PACKED;

static const uint8_t hid_report_desc[] = {
    // clang-format off
    HID_USAGE_PAGE(HID_PAGE_DESKTOP),
    HID_USAGE(HID_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_REPORT_ID(ReportIdKeyboard),
        HID_USAGE_PAGE(HID_DESKTOP_KEYPAD),
        HID_USAGE_MINIMUM(HID_KEYBOARD_L_CTRL),
        HID_USAGE_MAXIMUM(HID_KEYBOARD_R_GUI),
        HID_LOGICAL_MINIMUM(0),
        HID_LOGICAL_MAXIMUM(1),
        HID_REPORT_SIZE(1),
        HID_REPORT_COUNT(8),
        HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE), // modifiers
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(8),
        HID_INPUT(HID_IOF_CONSTANT | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE), // reserved
        HID_USAGE_PAGE(HID_PAGE_LED),
        HID_REPORT_COUNT(8),
        HID_REPORT_SIZE(1),
        HID_USAGE_MINIMUM(1),
        HID_USAGE_MAXIMUM(8),
        HID_OUTPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE), // LEDs
        HID_REPORT_COUNT(HID_KB_MAX_KEYS),
        HID_REPORT_SIZE(8),
        HID_LOGICAL_MINIMUM(0),
        HID_RI_LOGICAL_MAXIMUM(16, 255),
        HID_USAGE_PAGE(HID_DESKTOP_KEYPAD),
        HID_USAGE_MINIMUM(0),
        HID_RI_USAGE_MAXIMUM(16, 255),
        HID_INPUT(HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE), // key codes
    HID_END_COLLECTION,

    HID_USAGE_PAGE(HID_PAGE_DESKTOP),
    HID_USAGE(HID_DESKTOP_MOUSE),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_USAGE(HID_DESKTOP_POINTER),
        HID_COLLECTION(HID_PHYSICAL_COLLECTION),
            HID_REPORT_ID(ReportIdMouse),
            HID_USAGE_PAGE(HID_PAGE_BUTTON),
            HID_USAGE_MINIMUM(1),
            HID_USAGE_MAXIMUM(3),
            HID_LOGICAL_MINIMUM(0),
            HID_LOGICAL_MAXIMUM(1),
            HID_REPORT_COUNT(3),
            HID_REPORT_SIZE(1),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE), // buttons
            HID_REPORT_SIZE(1),
            HID_REPORT_COUNT(5),
            HID_INPUT(HID_IOF_CONSTANT | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE), // padding
            HID_USAGE_PAGE(HID_PAGE_DESKTOP),
            HID_USAGE(HID_DESKTOP_X),
            HID_USAGE(HID_DESKTOP_Y),
            HID_USAGE(HID_DESKTOP_WHEEL),
            HID_LOGICAL_MINIMUM(-127),
            HID_LOGICAL_MAXIMUM(127),
            HID_REPORT_SIZE(8),
            HID_REPORT_COUNT(3),
            HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE), // x, y, wheel
        HID_END_COLLECTION,
    HID_END_COLLECTION,

    // clang-format on
};

// Helper channel: one 8-byte report each way, no report ids
static const uint8_t vnd_report_desc[] = {
    // clang-format off
    HID_RI_USAGE_PAGE(16, JIG_VENDOR_PAGE),
    HID_USAGE(0x01),
    HID_COLLECTION(HID_APPLICATION_COLLECTION),
        HID_LOGICAL_MINIMUM(0),
        HID_RI_LOGICAL_MAXIMUM(16, 255),
        HID_REPORT_SIZE(8),
        HID_REPORT_COUNT(JIG_PACKET_LEN),
        HID_USAGE(0x02),
        HID_OUTPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
        HID_USAGE(0x03),
        HID_INPUT(HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
    HID_END_COLLECTION,
    // clang-format on
};

static struct usb_device_descriptor hid_device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = 8,
    .idVendor = JIG_VID,
    .idProduct = JIG_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = UsbStrManuf,
    .iProduct = UsbStrProduct,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const struct HidConfigDescriptor hid_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct HidConfigDescriptor),
            .bNumInterfaces = 2,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(500),
        },
    .intf_0 =
        {
            .hid =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DTYPE_INTERFACE,
                    .bInterfaceNumber = 0,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_HID,
                    .bInterfaceSubClass = 0,
                    .bInterfaceProtocol = 0,
                    .iInterface = NO_DESCRIPTOR,
                },
            .hid_desc =
                {
                    .bLength = sizeof(struct usb_hid_descriptor),
                    .bDescriptorType = USB_DTYPE_HID,
                    .bcdHID = VERSION_BCD(1, 0, 0),
                    .bCountryCode = USB_HID_COUNTRY_NONE,
                    .bNumDescriptors = 1,
                    .bDescriptorType0 = USB_DTYPE_HID_REPORT,
                    .wDescriptorLength0 = sizeof(hid_report_desc),
                },
            .hid_ep_in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = HID_EP_IN,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = HID_EP_SZ,
                    .bInterval = HID_INTERVAL,
                },
            .hid_ep_out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = HID_EP_OUT,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = HID_EP_SZ,
                    .bInterval = HID_INTERVAL,
                },
        },
    .intf_1 =
        {
            .hid =
                {
                    .bLength = sizeof(struct usb_interface_descriptor),
                    .bDescriptorType = USB_DTYPE_INTERFACE,
                    .bInterfaceNumber = 1,
                    .bAlternateSetting = 0,
                    .bNumEndpoints = 2,
                    .bInterfaceClass = USB_CLASS_HID,
                    .bInterfaceSubClass = 0,
                    .bInterfaceProtocol = 0,
                    .iInterface = NO_DESCRIPTOR,
                },
            .hid_desc =
                {
                    .bLength = sizeof(struct usb_hid_descriptor),
                    .bDescriptorType = USB_DTYPE_HID,
                    .bcdHID = VERSION_BCD(1, 0, 0),
                    .bCountryCode = USB_HID_COUNTRY_NONE,
                    .bNumDescriptors = 1,
                    .bDescriptorType0 = USB_DTYPE_HID_REPORT,
                    .wDescriptorLength0 = sizeof(vnd_report_desc),
                },
            .hid_ep_in =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = VND_EP_IN,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = VND_EP_SZ,
                    .bInterval = 10,
                },
            .hid_ep_out =
                {
                    .bLength = sizeof(struct usb_endpoint_descriptor),
                    .bDescriptorType = USB_DTYPE_ENDPOINT,
                    .bEndpointAddress = VND_EP_OUT,
                    .bmAttributes = USB_EPTYPE_INTERRUPT,
                    .wMaxPacketSize = VND_EP_SZ,
                    .bInterval = 10,
                },
        },
};

struct HidReportMouse {
    uint8_t report_id;
    uint8_t btn;
    int8_t x;
    int8_t y;
    int8_t wheel;
} FURI_PACKED;

struct HidReportKB {
    uint8_t report_id;
    uint8_t mods;
    uint8_t reserved;
    uint8_t btn[HID_KB_MAX_KEYS];
} FURI_PACKED;

static struct HidReport {
    struct HidReportKB keyboard;
    struct HidReportMouse mouse;
} FURI_PACKED hid_report;

static uint8_t status_report[JIG_PACKET_LEN];

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void hid_deinit(usbd_device* dev);
static void hid_on_wakeup(usbd_device* dev);
static void hid_on_suspend(usbd_device* dev);
static usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

static FuriHalUsbInterface usb_hid_jig = {
    .init = hid_init,
    .deinit = hid_deinit,
    .wakeup = hid_on_wakeup,
    .suspend = hid_on_suspend,
    .dev_descr = &hid_device_desc,
    .str_manuf_descr = NULL,
    .str_prod_descr = NULL,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)&hid_cfg_desc,
};

static usbd_device* usb_dev;
static FuriSemaphore* tx_semaphore;
static FuriSemaphore* vnd_semaphore;
static FuriHalUsbInterface* usb_prev_config;
static volatile bool hid_connected;

// Written by the USB thread, read by the app thread
static volatile bool host_fresh;
static volatile uint8_t host_packet[JIG_PACKET_LEN];
static volatile uint32_t host_tick;

static void* make_string_descr(const char* str) {
    size_t len = strlen(str);
    struct usb_string_descriptor* d = malloc(len * 2 + 2);
    d->bLength = len * 2 + 2;
    d->bDescriptorType = USB_DTYPE_STRING;
    for(size_t i = 0; i < len; i++)
        d->wString[i] = str[i];
    return d;
}

static void host_packet_received(const uint8_t* data, size_t len) {
    if(len < JIG_PACKET_LEN) return;
    if(data[0] != JIG_HOST_MAGIC || data[1] != JIG_HOST_VERSION) return;
    for(size_t i = 0; i < JIG_PACKET_LEN; i++)
        host_packet[i] = data[i];
    host_tick = furi_get_tick();
    host_fresh = true;
}

static void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    usb_dev = dev;
    hid_report.keyboard.report_id = ReportIdKeyboard;
    hid_report.mouse.report_id = ReportIdMouse;

    usb_hid_jig.str_manuf_descr = make_string_descr(JIG_MANUF);
    usb_hid_jig.str_prod_descr = make_string_descr(JIG_PRODUCT);

    usbd_reg_config(dev, hid_ep_config);
    usbd_reg_control(dev, hid_control);
    usbd_connect(dev, true);
}

static void hid_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    free(usb_hid_jig.str_manuf_descr);
    free(usb_hid_jig.str_prod_descr);
    usb_hid_jig.str_manuf_descr = NULL;
    usb_hid_jig.str_prod_descr = NULL;
}

static void hid_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    hid_connected = true;
}

static void hid_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    if(hid_connected) {
        hid_connected = false;
        furi_semaphore_release(tx_semaphore);
        furi_semaphore_release(vnd_semaphore);
    }
}

static bool ep_send(FuriSemaphore* sem, uint8_t ep, const void* report, size_t len) {
    if(!sem || !hid_connected) return false;
    FuriStatus status = furi_semaphore_acquire(sem, 50);
    if(status != FuriStatusOk) return false;
    if(!hid_connected) return false;
    usbd_ep_write(usb_dev, ep, (void*)report, len);
    return true;
}

static bool hid_send(const void* report, size_t len) {
    return ep_send(tx_semaphore, HID_EP_IN, report, len);
}

static void hid_txrx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(tx_semaphore);
    } else {
        // Keyboard LED reports: read and ignore
        uint8_t buf[HID_EP_SZ];
        usbd_ep_read(dev, ep, buf, sizeof(buf));
    }
}

static void vnd_txrx_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(vnd_semaphore);
    } else {
        uint8_t buf[VND_EP_SZ];
        int32_t len = usbd_ep_read(dev, ep, buf, sizeof(buf));
        if(len > 0) host_packet_received(buf, len);
    }
}

static usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        usbd_ep_deconfig(dev, HID_EP_OUT);
        usbd_ep_deconfig(dev, HID_EP_IN);
        usbd_ep_deconfig(dev, VND_EP_OUT);
        usbd_ep_deconfig(dev, VND_EP_IN);
        usbd_reg_endpoint(dev, HID_EP_OUT, 0);
        usbd_reg_endpoint(dev, HID_EP_IN, 0);
        usbd_reg_endpoint(dev, VND_EP_OUT, 0);
        usbd_reg_endpoint(dev, VND_EP_IN, 0);
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_ep_config(dev, HID_EP_OUT, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_ep_config(dev, VND_EP_IN, USB_EPTYPE_INTERRUPT, VND_EP_SZ);
        usbd_ep_config(dev, VND_EP_OUT, USB_EPTYPE_INTERRUPT, VND_EP_SZ);
        usbd_reg_endpoint(dev, HID_EP_IN, hid_txrx_ep_callback);
        usbd_reg_endpoint(dev, HID_EP_OUT, hid_txrx_ep_callback);
        usbd_reg_endpoint(dev, VND_EP_IN, vnd_txrx_ep_callback);
        usbd_reg_endpoint(dev, VND_EP_OUT, vnd_txrx_ep_callback);
        usbd_ep_write(dev, HID_EP_IN, 0, 0);
        usbd_ep_write(dev, VND_EP_IN, 0, 0);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    uint8_t kind = (USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType;
    uint16_t intf = req->wIndex;
    if(intf > 1) return usbd_fail;
    if(kind == (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
        case USB_HID_SETPROTOCOL:
            return usbd_ack;
        case USB_HID_GETREPORT:
            if(intf == 0) {
                dev->status.data_ptr = &hid_report;
                dev->status.data_count = sizeof(hid_report);
            } else {
                dev->status.data_ptr = status_report;
                dev->status.data_count = sizeof(status_report);
            }
            return usbd_ack;
        case USB_HID_SETREPORT:
            // Some hosts write output reports through the control pipe
            if(intf == 1) host_packet_received(req->data, req->wLength);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    if(kind == (USB_REQ_INTERFACE | USB_REQ_STANDARD) && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        const struct HidIntfDescriptor* d = intf == 0 ? &hid_cfg_desc.intf_0 :
                                                        &hid_cfg_desc.intf_1;
        switch(req->wValue >> 8) {
        case USB_DTYPE_HID:
            dev->status.data_ptr = (uint8_t*)&d->hid_desc;
            dev->status.data_count = sizeof(d->hid_desc);
            return usbd_ack;
        case USB_DTYPE_HID_REPORT:
            dev->status.data_ptr = (uint8_t*)(intf == 0 ? hid_report_desc : vnd_report_desc);
            dev->status.data_count = intf == 0 ? sizeof(hid_report_desc) : sizeof(vnd_report_desc);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

// ---------------------------------------------------------------- public

void jig_usb_start(void) {
    if(!tx_semaphore) tx_semaphore = furi_semaphore_alloc(1, 1);
    if(!vnd_semaphore) vnd_semaphore = furi_semaphore_alloc(1, 1);
    hid_connected = false;
    host_fresh = false;
    usb_prev_config = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid_jig, NULL));
}

void jig_usb_stop(void) {
    furi_hal_usb_set_config(usb_prev_config, NULL);
    if(tx_semaphore) {
        furi_semaphore_free(tx_semaphore);
        tx_semaphore = NULL;
    }
    if(vnd_semaphore) {
        furi_semaphore_free(vnd_semaphore);
        vnd_semaphore = NULL;
    }
}

bool jig_usb_is_connected(void) {
    return hid_connected;
}

bool jig_usb_mouse_move(int8_t dx, int8_t dy) {
    hid_report.mouse.x = dx;
    hid_report.mouse.y = dy;
    bool ok = hid_send(&hid_report.mouse, sizeof(hid_report.mouse));
    hid_report.mouse.x = 0;
    hid_report.mouse.y = 0;
    return ok;
}

bool jig_usb_key_tap(uint16_t key) {
    hid_report.keyboard.mods = key >> 8;
    hid_report.keyboard.btn[0] = key & 0xFF;
    bool ok = hid_send(&hid_report.keyboard, sizeof(hid_report.keyboard));
    hid_report.keyboard.mods = 0;
    hid_report.keyboard.btn[0] = 0;
    ok = hid_send(&hid_report.keyboard, sizeof(hid_report.keyboard)) && ok;
    return ok;
}

bool jig_usb_take_host_info(JigHostInfo* info) {
    if(!host_fresh) return false;
    host_fresh = false;
    info->valid = true;
    info->flags = host_packet[2];
    info->idle_seconds = host_packet[3] | ((uint16_t)host_packet[4] << 8);
    info->tick = host_tick;
    return true;
}

bool jig_usb_send_status(
    uint8_t state,
    uint16_t next_seconds,
    uint16_t since_move_ms,
    uint32_t moves) {
    uint8_t* d = status_report;
    d[0] = JIG_HOST_MAGIC;
    d[1] = state;
    d[2] = next_seconds & 0xFF;
    d[3] = next_seconds >> 8;
    d[4] = since_move_ms & 0xFF;
    d[5] = since_move_ms >> 8;
    d[6] = moves & 0xFF;
    d[7] = (moves >> 8) & 0xFF;
    return ep_send(vnd_semaphore, VND_EP_IN, status_report, sizeof(status_report));
}
