#pragma once

#include "include/domain/wc_observation.h"

#define WC_SIG_MAX_SSIDS 2 // directed SSIDs kept per device (bounds Flipper RAM)

// A device as accumulated over a scan session: identity plus the signals gathered about it.
typedef struct {
    uint8_t mac[6];
    bool mac_random;
    WcDeviceType type;
    char ssids[WC_SIG_MAX_SSIDS][WC_SSID_MAX_LEN + 1];
    uint8_t ssid_count;
    int8_t rssi_max; // strongest (closest-to-zero) RSSI seen
    uint32_t obs_count;
    uint32_t first_seen; // epoch seconds, opaque to the domain (the app supplies "now")
    uint32_t last_seen;
    uint32_t ie_hash;   // IE fingerprint (0 if unknown / from a summary-line scan)
    WcVendor ie_vendor; // vendor hinted by a probe's tag-221 (survives MAC randomization)
} WcSignature;

// Initialize a signature from the first observation of a device at time `now`.
void wc_signature_from_obs(WcSignature *sig, const WcObservation *obs, uint32_t now);

// Fold another observation of the same device into `sig`: refresh last_seen, bump the
// count, keep the strongest RSSI, and record a directed SSID if new and there is room.
void wc_signature_merge(WcSignature *sig, const WcObservation *obs, uint32_t now);

// Record a directed SSID on the signature if non-empty, not already present, and there is
// room. Returns true if it was added.
bool wc_signature_add_ssid(WcSignature *sig, const char *ssid);

// True if the signature has recorded this directed SSID.
bool wc_signature_has_ssid(const WcSignature *sig, const char *ssid);

// Fold `src`'s accumulated stats into `dst` (same device across captures): sum obs_count,
// widen first/last_seen, keep the strongest RSSI, union SSIDs, sharpen an unknown type.
void wc_signature_absorb(WcSignature *dst, const WcSignature *src);

// Best vendor label for a device: the OUI vendor for a stable MAC, else the tag-221 hint
// (which works for randomized MACs), else "". Never NULL.
const char *wc_signature_vendor(const WcSignature *sig);
