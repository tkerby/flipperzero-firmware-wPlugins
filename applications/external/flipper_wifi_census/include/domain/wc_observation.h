#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WC_SSID_MAX_LEN 32 // 802.11 SSID is at most 32 bytes

typedef enum {
    WcDeviceUnknown = 0,
    WcDevicePhone,
    WcDeviceLaptop,
    WcDeviceIot,
    WcDeviceAp,
    WcDeviceTypeCount,
} WcDeviceType;

typedef enum {
    WcVendorUnknown = 0,
    WcVendorApple,
    WcVendorSamsung,
    WcVendorHuawei,
    WcVendorXiaomi,
    WcVendorGoogle,
    WcVendorIntel,
    WcVendorDell,
    WcVendorMicrosoft,
    WcVendorEspressif,
    WcVendorRaspberryPi,
    WcVendorQualcomm,
    WcVendorBroadcom,
} WcVendor;

// One parsed sighting from a single frame reported by Marauder.
typedef struct {
    uint8_t mac[6];
    bool mac_random;                       // locally-administered address bit set
    int8_t rssi;                           // dBm, as reported (negative)
    uint8_t channel;                       // 1..14
    char probed_ssid[WC_SSID_MAX_LEN + 1]; // directed probe SSID, "" if wildcard/beacon
    bool is_beacon;                        // beacon (an AP) vs probe request (a client)
    uint16_t seq;                          // 802.11 sequence number (0 when unknown)
    uint32_t ie_hash;                      // fingerprint of the probe's IEs (0 if none)
    WcVendor ie_vendor; // vendor from a probe's tag-221 OUI (works for random MACs)
} WcObservation;

// True when the MAC's locally-administered bit is set — the marker of a randomized
// (privacy) address. Such a MAC carries no vendor and is not stable across sessions.
bool wc_mac_is_random(const uint8_t mac[6]);

// Vendor from the OUI (first 3 bytes). Only meaningful for a stable MAC; a randomized MAC
// always returns WcVendorUnknown. Backed by a small table of well-known OUIs, not exhaustive.
WcVendor wc_oui_vendor(const uint8_t mac[6]);

// Human-readable vendor name from the OUI ("Apple", "Samsung", …), or "" when randomized or
// not in the curated table. Never NULL.
const char *wc_oui_vendor_name(const uint8_t mac[6]);

// Name of a vendor enum ("Apple", …), or "" for WcVendorUnknown. Never NULL.
const char *wc_vendor_name(WcVendor v);

// Vendor from a probe's tag-221 (vendor-specific IE) OUI. Only meaningful device-maker OUIs
// map (Apple, Qualcomm, Broadcom); generic WPS/WMM/WFA OUIs return WcVendorUnknown. This is
// the one brand hint that survives MAC randomization.
WcVendor wc_vendor_from_ie_oui(const uint8_t oui[3]);

// Best-effort device class from one observation. A beacon is an AP; a randomized MAC with a
// wildcard probe is the modern-phone pattern; a stable MAC leans on its vendor. Heuristic.
WcDeviceType wc_device_type_guess(const WcObservation *obs);

// Lowercase, stable, ASCII name for a device type ("unknown", "phone", ...). Never NULL.
const char *wc_device_type_name(WcDeviceType type);
