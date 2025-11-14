#include "usb_hid_switch.h"
#include <furi_hal.h>

// HID Report Descriptor for Nintendo Switch Pro Controller
static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01, // Usage Page (Generic Desktop)
    0x09, 0x05, // Usage (Game Pad)
    0xA1, 0x01, // Collection (Application)

    // Buttons (14 buttons)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x35, 0x00, //   Physical Minimum (0)
    0x45, 0x01, //   Physical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0E, //   Report Count (14)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (Button 1)
    0x29, 0x0E, //   Usage Maximum (Button 14)
    0x81, 0x02, //   Input (Data,Var,Abs)

    // Padding (2 bits to align to byte boundary)
    0x95, 0x02, //   Report Count (2)
    0x81, 0x01, //   Input (Const,Array,Abs)

    // HAT Switch (D-Pad)
    0x05, 0x01, //   Usage Page (Generic Desktop)
    0x09, 0x39, //   Usage (Hat Switch)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x07, //   Logical Maximum (7)
    0x35, 0x00, //   Physical Minimum (0)
    0x46, 0x3B, 0x01, // Physical Maximum (315)
    0x65, 0x14, //   Unit (Eng Rot: Degree)
    0x75, 0x04, //   Report Size (4)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x42, //   Input (Data,Var,Abs,Null)

    // Padding (4 bits)
    0x65, 0x00, //   Unit (None)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x01, //   Input (Const,Array,Abs)

    // Analog sticks (4 axes: LX, LY, RX, RY)
    0x05, 0x01, //   Usage Page (Generic Desktop)
    0x09, 0x30, //   Usage (X)
    0x09, 0x31, //   Usage (Y)
    0x09, 0x32, //   Usage (Z)
    0x09, 0x35, //   Usage (Rz)
    0x15, 0x00, //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // Logical Maximum (65535)
    0x75, 0x10, //   Report Size (16)
    0x95, 0x04, //   Report Count (4)
    0x81, 0x02, //   Input (Data,Var,Abs)

    0xC0, // End Collection
};

// USB device descriptor
static const struct usb_device_descriptor hid_switch_device_descriptor = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CSCP_NoDeviceClass,
    .bDeviceSubClass = USB_CSCP_NoDeviceSubclass,
    .bDeviceProtocol = USB_CSCP_NoDeviceProtocol,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = USB_VID_NINTENDO,
    .idProduct = USB_PID_SWITCH_PRO,
    .bcdDevice = VERSION_BCD(2, 0, 0),
    .iManufacturer = UsbDevManuf,
    .iProduct = UsbDevProduct,
    .iSerialNumber = UsbDevSerial,
    .bNumConfigurations = 1,
};

// Configuration descriptor
static const struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor hid_interface;
    struct usb_hid_descriptor hid;
    struct usb_endpoint_descriptor hid_ep_in;
} __attribute__((packed)) hid_switch_config_descriptor = {
    .config = {
        .bLength = sizeof(struct usb_config_descriptor),
        .bDescriptorType = USB_DTYPE_CONFIGURATION,
        .wTotalLength = sizeof(hid_switch_config_descriptor),
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = NO_DESCRIPTOR,
        .bmAttributes = USB_CFG_ATTR_RESERVED,
        .bMaxPower = USB_CFG_POWER_MA(500),
    },
    .hid_interface = {
        .bLength = sizeof(struct usb_interface_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = USB_CLASS_HID,
        .bInterfaceSubClass = USB_HID_SUBCLASS_NONBOOT,
        .bInterfaceProtocol = USB_HID_PROTO_NONBOOT,
        .iInterface = NO_DESCRIPTOR,
    },
    .hid = {
        .bLength = sizeof(struct usb_hid_descriptor),
        .bDescriptorType = USB_DTYPE_HID,
        .bcdHID = VERSION_BCD(1, 1, 1),
        .bCountryCode = USB_HID_COUNTRY_NONE,
        .bNumDescriptors = 1,
        .bDescriptorType0 = USB_DTYPE_HID_REPORT,
        .wDescriptorLength0 = sizeof(hid_report_descriptor),
    },
    .hid_ep_in = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = HID_EP_IN,
        .bmAttributes = USB_EPTYPE_INTERRUPT,
        .wMaxPacketSize = HID_EP_SZ,
        .bInterval = HID_INTERVAL,
    },
};

// String descriptors
static const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC("Nintendo Co., Ltd");
static const struct usb_string_descriptor dev_prod_desc = USB_STRING_DESC("Pro Controller");

// USB interface callbacks
static usbd_respond hid_switch_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond hid_switch_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static usbd_device* usb_dev;

// USB interface structure
static FuriHalUsbInterface usb_hid_switch = {
    .init = NULL,
    .deinit = NULL,
    .wakeup = NULL,
    .suspend = NULL,
    .dev_descr = (struct usb_device_descriptor*)&hid_switch_device_descriptor,
    .str_manuf_descr = (void*)&dev_manuf_desc,
    .str_prod_descr = (void*)&dev_prod_desc,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)&hid_switch_config_descriptor,
};

// Endpoint configuration callback
static usbd_respond hid_switch_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        // Deconfigure
        usbd_ep_deconfig(dev, HID_EP_IN);
        usbd_reg_endpoint(dev, HID_EP_IN, NULL);
        return usbd_ack;
    case 1:
        // Configure
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SZ);
        usbd_reg_endpoint(dev, HID_EP_IN, NULL);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

// Control request handler
static usbd_respond hid_switch_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) == (USB_REQ_INTERFACE | USB_REQ_STANDARD)) {
        switch(req->bRequest) {
        case USB_STD_GET_DESCRIPTOR:
            if(req->wValue == ((USB_DTYPE_HID_REPORT << 8) | 0)) {
                dev->status.data_ptr = (uint8_t*)hid_report_descriptor;
                dev->status.data_count = sizeof(hid_report_descriptor);
                return usbd_ack;
            } else if(req->wValue == ((USB_DTYPE_HID << 8) | 0)) {
                dev->status.data_ptr = (uint8_t*)&hid_switch_config_descriptor.hid;
                dev->status.data_count = sizeof(hid_switch_config_descriptor.hid);
                return usbd_ack;
            }
            return usbd_fail;
        default:
            return usbd_fail;
        }
    }

    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) == (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        switch(req->bRequest) {
        case USB_HID_SETIDLE:
            return usbd_ack;
        case USB_HID_GETREPORT:
            return usbd_fail;
        default:
            return usbd_fail;
        }
    }

    return usbd_fail;
}

bool usb_hid_switch_init() {
    // Set up the USB interface
    usb_hid_switch.init = NULL;
    usb_hid_switch.deinit = NULL;
    usb_hid_switch.wakeup = NULL;
    usb_hid_switch.suspend = NULL;

    // Get USB device
    usb_dev = furi_hal_usb_get_dev();

    // Configure endpoints and control
    usbd_reg_config(usb_dev, hid_switch_ep_config);
    usbd_reg_control(usb_dev, hid_switch_control);

    // Set USB configuration
    furi_hal_usb_unlock();
    bool result = furi_hal_usb_set_config(&usb_hid_switch, NULL);

    return result;
}

void usb_hid_switch_deinit() {
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(NULL, NULL);
}

bool usb_hid_switch_send_report(SwitchControllerState* state) {
    if(!state) return false;

    // Build HID report (12 bytes total)
    uint8_t report[12];

    // Bytes 0-1: Buttons (14 bits + 2 padding)
    report[0] = state->buttons & 0xFF;
    report[1] = (state->buttons >> 8) & 0xFF;

    // Byte 2: HAT (4 bits) + padding (4 bits)
    report[2] = (state->hat & 0x0F);

    // Bytes 3-10: Analog sticks (4 axes, 16-bit each)
    report[3] = state->lx & 0xFF;
    report[4] = (state->lx >> 8) & 0xFF;
    report[5] = state->ly & 0xFF;
    report[6] = (state->ly >> 8) & 0xFF;
    report[7] = state->rx & 0xFF;
    report[8] = (state->rx >> 8) & 0xFF;
    report[9] = state->ry & 0xFF;
    report[10] = (state->ry >> 8) & 0xFF;

    // Send report
    int result = usbd_ep_write(usb_dev, HID_EP_IN, report, sizeof(report));

    return (result == sizeof(report));
}

void usb_hid_switch_reset_state(SwitchControllerState* state) {
    if(!state) return;

    state->buttons = 0;
    state->hat = SWITCH_HAT_NEUTRAL;
    state->lx = STICK_CENTER;
    state->ly = STICK_CENTER;
    state->rx = STICK_CENTER;
    state->ry = STICK_CENTER;
}
