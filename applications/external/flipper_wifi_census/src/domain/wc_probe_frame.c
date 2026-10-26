#include "include/domain/wc_probe_frame.h"

#include <string.h>

#define WC_80211_HDR 24 // FC(2) Dur(2) A1(6) A2(6) A3(6) SeqCtrl(2)
#define FNV_OFFSET 2166136261u
#define FNV_PRIME 16777619u

static uint32_t fnv1a(uint32_t h, uint8_t b) {
    h ^= b;
    h *= FNV_PRIME;
    return h;
}

// Tags whose contents carry a stable capability fingerprint (rates, HT/VHT/ext-cap). The
// SSID (0) and DS-param/channel (3) are excluded on purpose — they vary per network/channel.
static bool is_fingerprint_tag(uint8_t tag) {
    return tag == 1 || tag == 45 || tag == 50 || tag == 127 || tag == 191 || tag == 221;
}

bool wc_parse_probe_frame(const uint8_t *frame, size_t len, WcObservation *out) {
    if (len < WC_80211_HDR) {
        return false;
    }
    // Frame Control: management type (00) + probe-request subtype (0100) -> byte0 & 0xFC == 0x40.
    if ((frame[0] & 0xFC) != 0x40) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    memcpy(out->mac, frame + 10, 6); // Address 2 = source (the client)
    out->mac_random = wc_mac_is_random(out->mac);
    out->seq = (uint16_t)((frame[22] | (frame[23] << 8)) >> 4);
    out->is_beacon = false;

    uint32_t h = FNV_OFFSET;
    size_t o = WC_80211_HDR;
    while (o + 2 <= len) {
        uint8_t tag = frame[o];
        uint8_t tlen = frame[o + 1];
        if (o + 2 + tlen > len) {
            break; // truncated IE -> stop, do not read past the buffer
        }
        const uint8_t *val = frame + o + 2;
        // Every tag number feeds the fingerprint (its presence and order matter).
        h = fnv1a(h, tag);
        if (tag == 0) {
            uint8_t n = tlen < WC_SSID_MAX_LEN ? tlen : WC_SSID_MAX_LEN;
            memcpy(out->probed_ssid, val, n);
            out->probed_ssid[n] = '\0';
        } else if (tag == 3 && tlen >= 1) {
            out->channel = val[0];
        } else if (tag == 221 && tlen >= 3 && out->ie_vendor == WcVendorUnknown) {
            // A vendor-specific IE can name the maker even under MAC randomization.
            out->ie_vendor = wc_vendor_from_ie_oui(val);
        }
        if (is_fingerprint_tag(tag)) {
            for (uint8_t i = 0; i < tlen; i++) {
                h = fnv1a(h, val[i]);
            }
        }
        o += 2 + tlen;
    }
    out->ie_hash = h;
    return true;
}
