#pragma once
// Station-name builder.
//
// The broadcast SSID is ALWAYS prefixed with "Flipper Radio". A user-supplied
// suffix can personalize it but can never replace it, so the station can never
// be made to impersonate another network — an open AP plus an auto-popping
// captive portal would otherwise be an evil-twin kit.
//
// Pure C, no ESP deps, so the sanitizer is host-testable.

#include <stddef.h>

#define SSID_PREFIX    "Flipper Radio"
#define SSID_SEPARATOR " - "
#define SSID_MAX       32 // 802.11 limit, in bytes

// Compose the broadcast SSID from an optional user suffix.
//
// `suffix` may be NULL or empty -> the SSID is exactly SSID_PREFIX.
// Characters outside printable ASCII are dropped, runs of whitespace collapse
// to one space, and the result is trimmed and truncated to fit SSID_MAX.
// A suffix that sanitizes down to nothing is treated as absent.
//
// Always writes a valid NUL-terminated SSID. Returns its length in bytes.
size_t ssid_build(const char* suffix, char* out, size_t out_cap);
