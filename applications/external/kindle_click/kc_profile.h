#pragma once

#include <furi_ble/profile_interface.h>

#define KC_VERSION "0.3"

// The link as the profile sees it. Written in the BLE event thread; the app
// reads it.
typedef struct {
    volatile bool connected;
    volatile bool secured; // encrypted, so the Kindle holds our bond
    volatile uint32_t generation; // counts connections
    volatile uint32_t secured_at;
    volatile uint32_t cccd_at; // last notification-enabling CCCD write (report, battery)
    volatile uint32_t link_change_at; // last link change: left 7.5 ms, or 20 ms with no latency
    volatile bool fast_interval; // 7.5 ms: the Kindle is discovering GATT
} KcLink;

extern const FuriHalBleProfileTemplate* kc_profile;

KcLink* kc_profile_link(FuriHalBleProfileBase* profile);

// Both return false when the stack refused the update.
bool kc_profile_send_report(FuriHalBleProfileBase* profile, const uint8_t report[10]);
bool kc_profile_set_battery(FuriHalBleProfileBase* profile, uint8_t pct);
