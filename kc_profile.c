// The Bluetooth profile a Kindle accepts as a Kindle Click: what it advertises,
// its GATT services, and the link events that tell the app when the Kindle is
// ready for keys. PROTOCOL.md in Open-Kindle-Click-nRF52840 says where each
// requirement comes from.

#include "kc_profile.h"

#include <furi.h>
#include <furi_hal_version.h>
#include <furi_ble/event_dispatcher.h>
#include <furi_ble/gatt.h>
#include <ble/core/ble_defs.h>

#define TAG "KindleClick"

#define UUID_HID              0x1812
#define UUID_BATTERY          0x180F
#define UUID_DEVICE_INFO      0x180A
#define UUID_HID_INFO         0x2A4A
#define UUID_REPORT_MAP       0x2A4B
#define UUID_HID_CONTROL      0x2A4C
#define UUID_REPORT           0x2A4D
#define UUID_REPORT_REFERENCE 0x2908
#define UUID_BATTERY_LEVEL    0x2A19
#define UUID_FIRMWARE_REV     0x2A26
#define UUID_PNP_ID           0x2A50

// HCI and ST vendor events the profile watches. The SDK has no headers for them.
#define HCI_EVT_DISCONNECTION_COMPLETE  0x05
#define HCI_EVT_ENCRYPTION_CHANGE       0x08
#define HCI_EVT_LE_META                 0x3E
#define HCI_EVT_VENDOR                  0xFF
#define LE_CONNECTION_COMPLETE          0x01
#define LE_CONNECTION_UPDATE_COMPLETE   0x03
#define LE_ENHANCED_CONNECTION_COMPLETE 0x0A
#define ACI_GAP_PAIRING_COMPLETE        0x0401
#define ACI_GATT_ATTRIBUTE_MODIFIED     0x0C01

// One 10-byte input report (ID 1): the boot-keyboard layout, then one consumer
// usage. One application collection: Linux hid-generic then makes one evdev
// node, and it has EV_REP (from the keyboard usages), so held brightness
// repeats.
// clang-format off
static const uint8_t REPORT_MAP[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop)
    0x09, 0x06,                   // Usage (Keyboard)
    0xA1, 0x01,                   // Collection (Application)
    0x85, 0x01,                   //   Report ID (1)
    0x05, 0x07,                   //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0, 0x29, 0xE7,       //   Usage Min/Max (modifiers)
    0x15, 0x00, 0x25, 0x01,       //   Logical Min/Max (0..1)
    0x75, 0x01, 0x95, 0x08,       //   Report Size 1, Count 8
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x75, 0x08, 0x95, 0x01,       //   Report Size 8, Count 1
    0x81, 0x01,                   //   Input (Const): reserved byte
    0x19, 0x00, 0x29, 0x65,       //   Usage Min/Max (0..0x65)
    0x15, 0x00, 0x25, 0x65,       //   Logical Min/Max (0..0x65)
    0x75, 0x08, 0x95, 0x06,       //   Report Size 8, Count 6
    0x81, 0x00,                   //   Input (Data, Array, Abs)
    0x05, 0x0C,                   //   Usage Page (Consumer)
    0x19, 0x00, 0x2A, 0xFF, 0x03, //   Usage Min/Max (0..0x3FF)
    0x15, 0x00, 0x26, 0xFF, 0x03, //   Logical Min/Max (0..0x3FF)
    0x75, 0x10, 0x95, 0x01,       //   Report Size 16, Count 1
    0x81, 0x00,                   //   Input (Data, Array, Abs)
    0xC0,                         // End Collection
};
// clang-format on

// bcdHID 1.11, no country, RemoteWake | NormallyConnectable.
static const uint8_t HID_INFO[] = {0x11, 0x01, 0x00, 0x03};
static const uint8_t HID_CONTROL[] = {0x00};
static const uint8_t REPORT_INIT[10] = {0};
static const uint8_t REPORT_REFERENCE[] = {0x01, 0x01}; // report ID 1, input
static const uint8_t BATTERY_INIT[] = {100};
// Source 0x01 (Bluetooth SIG), VID 0x0171, PID 0x0443, version 0x0100. HOGP
// hands VID/PID to uhid; input_manager matches on them.
static const uint8_t PNP_ID[] = {0x01, 0x71, 0x01, 0x43, 0x04, 0x00, 0x01};
// Shown as "Firmware version" in the Kindle's Settings > Accessories.
static const char FIRMWARE_REV[] = "Flipper Zero " KC_VERSION;

static bool report_reference_data(const void* context, const uint8_t** data, uint16_t* len) {
    UNUSED(context);
    *len = sizeof(REPORT_REFERENCE);
    if(data) *data = REPORT_REFERENCE;
    return false;
}

static BleGattCharacteristicDescriptorParams report_reference = {
    .uuid_type = UUID_TYPE_16,
    .uuid.Char_UUID_16 = UUID_REPORT_REFERENCE,
    .max_length = sizeof(REPORT_REFERENCE),
    .data_callback.fn = report_reference_data,
    .security_permissions = ATTR_PERMISSION_NONE,
    .access_permissions = ATTR_ACCESS_READ_ONLY,
    .gatt_evt_mask = GATT_DONT_NOTIFY_EVENTS,
    .is_variable = CHAR_VALUE_LEN_CONSTANT,
};

#define CHAR(char_name, char_uuid, value, len, props, descriptor) \
    {                                                             \
        .name = (char_name),                                      \
        .descriptor_params = (descriptor),                        \
        .data_prop_type = FlipperGattCharacteristicDataFixed,     \
        .data.fixed.ptr = (const uint8_t*)(value),                \
        .data.fixed.length = (len),                               \
        .uuid.Char_UUID_16 = (char_uuid),                         \
        .uuid_type = UUID_TYPE_16,                                \
        .char_properties = (props),                               \
        .security_permissions = ATTR_PERMISSION_NONE,             \
        .gatt_evt_mask = GATT_DONT_NOTIFY_EVENTS,                 \
        .is_variable = CHAR_VALUE_LEN_CONSTANT,                   \
    }

typedef enum {
    KcCharHidInfo,
    KcCharReportMap,
    KcCharReport,
    KcCharHidControl,
    KcCharBattery,
    KcCharPnpId,
    KcCharFirmware,
    KcCharCount,
} KcChar;

static const BleGattCharacteristicParams CHARS[KcCharCount] = {
    [KcCharHidInfo] =
        CHAR("HID Information", UUID_HID_INFO, HID_INFO, sizeof(HID_INFO), CHAR_PROP_READ, NULL),
    [KcCharReportMap] =
        CHAR("Report Map", UUID_REPORT_MAP, REPORT_MAP, sizeof(REPORT_MAP), CHAR_PROP_READ, NULL),
    [KcCharReport] = CHAR(
        "Report",
        UUID_REPORT,
        REPORT_INIT,
        sizeof(REPORT_INIT),
        CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        &report_reference),
    [KcCharHidControl] = CHAR(
        "HID Control Point",
        UUID_HID_CONTROL,
        HID_CONTROL,
        sizeof(HID_CONTROL),
        CHAR_PROP_WRITE_WITHOUT_RESP,
        NULL),
    [KcCharBattery] = CHAR(
        "Battery Level",
        UUID_BATTERY_LEVEL,
        BATTERY_INIT,
        sizeof(BATTERY_INIT),
        CHAR_PROP_READ | CHAR_PROP_NOTIFY,
        NULL),
    [KcCharPnpId] = CHAR("PnP ID", UUID_PNP_ID, PNP_ID, sizeof(PNP_ID), CHAR_PROP_READ, NULL),
    [KcCharFirmware] = CHAR(
        "Firmware Revision",
        UUID_FIRMWARE_REV,
        FIRMWARE_REV,
        sizeof(FIRMWARE_REV) - 1,
        CHAR_PROP_READ,
        NULL),
};

static const struct {
    uint16_t uuid;
    KcChar first, end;
    uint8_t attributes; // the service, 2 per characteristic, CCCDs, descriptors
} SERVICES[] = {
    // HID first, so the services after it never move the HID handles the
    // Kindle keeps for a session.
    {UUID_HID, KcCharHidInfo, KcCharBattery, 1 + 4 * 2 + 1 + 1},
    {UUID_BATTERY, KcCharBattery, KcCharPnpId, 1 + 2 + 1},
    {UUID_DEVICE_INFO, KcCharPnpId, KcCharCount, 1 + 2 * 2},
};

typedef struct {
    FuriHalBleProfileBase base;
    GapSvcEventHandler* event_handler;
    uint16_t svc_handles[COUNT_OF(SERVICES)];
    uint16_t char_svc[KcCharCount];
    BleGattCharacteristicInstance chars[KcCharCount];
    uint16_t conn_handle;
    KcLink link;
} KcProfile;

static uint16_t le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

// Runs in the BLE event thread for every event; never consumes one.
static BleEventAckStatus kc_event(void* event, void* context) {
    KcProfile* profile = context;
    KcLink* link = &profile->link;
    // A transport packet: type, then event code, parameter length, parameters.
    const uint8_t* evt = (const uint8_t*)event + 1;
    const uint8_t* p = evt + 2;
    uint32_t now = furi_get_tick();
    uint32_t t = now - link->connected_at;

    switch(evt[0]) {
    case HCI_EVT_LE_META:
        if((p[0] == LE_CONNECTION_COMPLETE || p[0] == LE_ENHANCED_CONNECTION_COMPLETE) &&
           p[1] == 0) {
            const uint8_t* params = p + (p[0] == LE_CONNECTION_COMPLETE ? 12 : 24);
            profile->conn_handle = le16(p + 2);
            link->connected_at = now;
            link->secured = false;
            link->cccd_at = 0;
            link->cccd_writes = 0;
            link->link_change_at = 0;
            link->fast_interval = le16(params) == 6;
            link->generation++;
            link->connected = true;
            FURI_LOG_I(
                TAG,
                "[ble] connected: interval %u, latency %u, timeout %u0 ms",
                le16(params),
                le16(params + 2),
                le16(params + 4));
        } else if(p[0] == LE_CONNECTION_UPDATE_COMPLETE && p[1] == 0) {
            uint16_t interval = le16(p + 4);
            uint16_t latency = le16(p + 6);
            // 7.5 ms: the Kindle's GATT discovery. 20 ms with no latency: the
            // link the Kindle settles on once it has set the remote up.
            bool fast = interval == 6;
            if((link->fast_interval && !fast) || (interval == 16 && latency == 0))
                link->link_change_at = now | 1;
            link->fast_interval = fast;
            FURI_LOG_I(
                TAG,
                "[link] +%lu ms: interval %u, latency %u, timeout %u0 ms",
                t,
                interval,
                latency,
                le16(p + 8));
        }
        break;
    case HCI_EVT_ENCRYPTION_CHANGE:
        if(p[0] == 0 && p[3]) {
            link->secured_at = now;
            link->secured = true;
            FURI_LOG_I(TAG, "[ble] +%lu ms: link encrypted", t);
        } else {
            FURI_LOG_I(TAG, "[ble] +%lu ms: encryption change, status 0x%02X", t, p[0]);
        }
        break;
    case HCI_EVT_DISCONNECTION_COMPLETE:
        if(p[0] == 0 && le16(p + 1) == profile->conn_handle) {
            link->connected = false;
            link->secured = false;
            FURI_LOG_I(TAG, "[ble] disconnected (reason 0x%02X)", p[3]);
        }
        break;
    case HCI_EVT_VENDOR: {
        const uint8_t* d = p + 2;
        if(le16(p) == ACI_GATT_ATTRIBUTE_MODIFIED) {
            // A characteristic's CCCD follows its value: declaration handle + 2.
            uint16_t attr = le16(d + 2);
            bool report = attr == profile->chars[KcCharReport].handle + 2;
            bool battery = attr == profile->chars[KcCharBattery].handle + 2;
            if((report || battery) && le16(d + 6) >= 1) {
                bool on = d[8] & 0x01;
                if(on) {
                    link->cccd_at = now | 1;
                    link->cccd_writes++;
                }
                FURI_LOG_I(
                    TAG,
                    "[link] +%lu ms: %s notifications %s",
                    t,
                    report ? "report" : "battery",
                    on ? "on" : "off");
            }
        } else if(le16(p) == ACI_GAP_PAIRING_COMPLETE) {
            FURI_LOG_I(
                TAG,
                "[ble] +%lu ms: pairing %s (status 0x%02X, reason 0x%02X)",
                t,
                d[2] ? "FAILED" : "complete",
                d[2],
                d[3]);
        }
        break;
    }
    default:
        break;
    }
    return BleEventNotAck;
}

static FuriHalBleProfileBase* kc_profile_start(FuriHalBleProfileParams params) {
    UNUSED(params);
    KcProfile* profile = malloc(sizeof(KcProfile));
    memset(profile, 0, sizeof(KcProfile));
    profile->base.config = kc_profile;

    for(size_t s = 0; s < COUNT_OF(SERVICES); s++) {
        Service_UUID_t uuid = {.Service_UUID_16 = SERVICES[s].uuid};
        furi_check(ble_gatt_service_add(
            UUID_TYPE_16,
            &uuid,
            PRIMARY_SERVICE,
            SERVICES[s].attributes,
            &profile->svc_handles[s]));
        for(KcChar c = SERVICES[s].first; c < SERVICES[s].end; c++) {
            profile->char_svc[c] = profile->svc_handles[s];
            ble_gatt_characteristic_init(profile->char_svc[c], &CHARS[c], &profile->chars[c]);
            ble_gatt_characteristic_update(profile->char_svc[c], &profile->chars[c], NULL);
        }
    }
    profile->event_handler = ble_event_dispatcher_register_svc_handler(kc_event, profile);
    return &profile->base;
}

static void kc_profile_stop(FuriHalBleProfileBase* base) {
    furi_check(base && base->config == kc_profile);
    KcProfile* profile = (KcProfile*)base;
    ble_event_dispatcher_unregister_svc_handler(profile->event_handler);
    for(size_t s = 0; s < COUNT_OF(SERVICES); s++) {
        for(KcChar c = SERVICES[s].first; c < SERVICES[s].end; c++) {
            ble_gatt_characteristic_delete(profile->svc_handles[s], &profile->chars[c]);
        }
        ble_gatt_service_delete(profile->svc_handles[s]);
    }
    free(profile);
}

static void kc_profile_get_gap_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    memset(config, 0, sizeof(GapConfig));
    config->adv_service.UUID_Type = UUID_TYPE_16;
    config->adv_service.Service_UUID_16 = UUID_HID;
    config->appearance_char = 0x0180; // generic remote control; the Kindle never reads it
    config->bonding_mode = true;
    // Just Works: the Kindle pairs as NoInputNoOutput and needs no MITM.
    config->pairing_method = GapPairingNone;
    // An address of its own, apart from the Flipper's other Bluetooth profiles.
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
    config->mac_address[2] += 2;
    config->mac_address[0] ^= 0x4B;
    // The Kindle only lists a remote whose ADV_IND itself carries manufacturer
    // data 71 01 04 43 (company 0x0171, PID 0x0443 big-endian). The GAP puts
    // manufacturer data in the scan response, but it sends adv_name as one AD
    // structure whose first byte is the AD type. With type 0xFF, the "name" is
    // the manufacturer data.
    const char adv[] = {(char)AD_TYPE_MANUFACTURER_SPECIFIC_DATA, 0x71, 0x01, 0x04, 0x43, 0};
    memcpy(config->adv_name, adv, sizeof(adv));
    // The Kindle picks the link: 7.5 ms while it discovers GATT, then 20 ms.
    // The GAP only asks for a change when the interval is below this minimum.
    config->conn_param.conn_int_min = 0x06;
    config->conn_param.conn_int_max = 0x10;
}

static const FuriHalBleProfileTemplate kc_profile_template = {
    .start = kc_profile_start,
    .stop = kc_profile_stop,
    .get_gap_config = kc_profile_get_gap_config,
};

const FuriHalBleProfileTemplate* kc_profile = &kc_profile_template;

KcLink* kc_profile_link(FuriHalBleProfileBase* base) {
    furi_check(base && base->config == kc_profile);
    return &((KcProfile*)base)->link;
}

// ble_gatt_characteristic_update() returns true when it failed.
static bool kc_update(FuriHalBleProfileBase* base, KcChar c, const void* value) {
    furi_check(base && base->config == kc_profile);
    KcProfile* profile = (KcProfile*)base;
    return !ble_gatt_characteristic_update(profile->char_svc[c], &profile->chars[c], value);
}

bool kc_profile_send_report(FuriHalBleProfileBase* base, const uint8_t report[10]) {
    return kc_update(base, KcCharReport, report);
}

bool kc_profile_set_battery(FuriHalBleProfileBase* base, uint8_t pct) {
    return kc_update(base, KcCharBattery, &pct);
}
