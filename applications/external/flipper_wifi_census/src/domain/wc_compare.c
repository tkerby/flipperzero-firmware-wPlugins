#include "include/domain/wc_compare.h"

#include <string.h>

// True if two signatures are the same device by a high-confidence signal. `shared_ssid` is
// filled with the matching SSID when the match is by SSID.
static bool same_device(const WcSignature *x, const WcSignature *y, WcMatchReason *reason,
                        char *shared_ssid) {
    // Stable MAC equality. (Two independent randomized MACs colliding is not a real signal,
    // so require both to be non-random for a MAC match.)
    if (!x->mac_random && !y->mac_random && memcmp(x->mac, y->mac, 6) == 0) {
        *reason = WcMatchByMac;
        shared_ssid[0] = '\0';
        return true;
    }
    // Shared directed SSID crosses a ROTATING (randomized) device between captures — the
    // friend-phone case. It must not link two distinct stable-MAC devices that merely share a
    // common network (e.g. a cafe SSID), so require at least one side to be randomized.
    if ((x->mac_random || y->mac_random) && x->type != WcDeviceAp && y->type != WcDeviceAp) {
        for (uint8_t i = 0; i < x->ssid_count; i++) {
            if (wc_signature_has_ssid(y, x->ssids[i])) {
                *reason = WcMatchBySsid;
                strncpy(shared_ssid, x->ssids[i], WC_SSID_MAX_LEN);
                shared_ssid[WC_SSID_MAX_LEN] = '\0';
                return true;
            }
        }
    }
    return false;
}

const WcSignature *wc_compare_find(const WcCensus *c, const WcSignature *sig, WcMatchReason *reason,
                                   char *shared_ssid) {
    WcMatchReason r = WcMatchByMac;
    char shared[WC_SSID_MAX_LEN + 1];
    for (uint16_t i = 0; i < c->count; i++) {
        if (same_device(sig, &c->devices[i], &r, shared)) {
            if (reason) {
                *reason = r;
            }
            if (shared_ssid) {
                memcpy(shared_ssid, shared, sizeof(shared));
            }
            return &c->devices[i];
        }
    }
    return NULL;
}

static uint16_t count_random(const WcCensus *c) {
    uint16_t n = 0;
    for (uint16_t i = 0; i < c->count; i++) {
        if (c->devices[i].mac_random) {
            n++;
        }
    }
    return n;
}

void wc_compare(WcCompareResult *r, const WcCensus *a, const WcCensus *b) {
    memset(r, 0, sizeof(*r));
    r->na = a->count;
    r->nb = b->count;
    r->random_a = count_random(a);
    r->random_b = count_random(b);

    for (uint16_t i = 0; i < a->count; i++) {
        const WcSignature *da = &a->devices[i];
        WcMatchReason reason = WcMatchByMac;
        char shared[WC_SSID_MAX_LEN + 1];
        bool matched = false;
        for (uint16_t j = 0; j < b->count; j++) {
            if (same_device(da, &b->devices[j], &reason, shared)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            continue;
        }
        r->intersection++;
        if (r->match_count < WC_COMPARE_MAX_MATCHES) {
            WcMatch *m = &r->matches[r->match_count++];
            memcpy(m->mac, da->mac, 6);
            m->reason = reason;
            if (reason == WcMatchBySsid) {
                strncpy(m->detail, shared, WC_SSID_MAX_LEN);
                m->detail[WC_SSID_MAX_LEN] = '\0';
            } else {
                m->detail[0] = '\0';
            }
        }
    }
}
