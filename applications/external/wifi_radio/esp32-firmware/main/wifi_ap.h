#pragma once
// Brings up the WiFi softAP. SSID comes from NVS (set over UART), defaulting
// to "Flipper Radio".

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ssid_name.h"

#define WIFI_AP_SSID_MAX SSID_MAX

void wifi_ap_start(void);

// Persist a personalizing SUFFIX and apply it live. The broadcast SSID is
// always "Flipper Radio[ - <suffix>]" (see ssid_name.h) — a caller cannot
// replace the prefix, so the station can never impersonate another network.
// Reconnects any listeners, since this tears down the AP association.
void wifi_ap_set_name_suffix(const char* suffix);

void wifi_ap_get_ssid(char* out, size_t out_cap);

// Phones currently associated to the AP (not the same as stream listeners).
uint8_t wifi_ap_station_count(void);

// The hardware address behind an IP the HTTP server saw, or false if that IP
// is not a current station. The captive portal remembers who has signed in by
// MAC: DHCP re-issues addresses, so keying on the IP means a phone that
// rejoins looks like a stranger, and iOS goes straight back to flagging the
// network as having no internet.
bool wifi_ap_mac_for_ip(uint32_t ip, uint8_t mac_out[6]);
