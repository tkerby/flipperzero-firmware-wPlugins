#include "include/domain/wc_signature.h"

#include <string.h>

bool wc_signature_has_ssid(const WcSignature *sig, const char *ssid) {
    for (uint8_t i = 0; i < sig->ssid_count; i++) {
        if (strncmp(sig->ssids[i], ssid, WC_SSID_MAX_LEN) == 0) {
            return true;
        }
    }
    return false;
}

bool wc_signature_add_ssid(WcSignature *sig, const char *ssid) {
    if (ssid[0] == '\0') {
        return false;
    }
    if (sig->ssid_count >= WC_SIG_MAX_SSIDS) {
        return false;
    }
    if (wc_signature_has_ssid(sig, ssid)) {
        return false;
    }
    strncpy(sig->ssids[sig->ssid_count], ssid, WC_SSID_MAX_LEN);
    sig->ssids[sig->ssid_count][WC_SSID_MAX_LEN] = '\0';
    sig->ssid_count++;
    return true;
}

void wc_signature_from_obs(WcSignature *sig, const WcObservation *obs, uint32_t now) {
    memset(sig, 0, sizeof(*sig));
    memcpy(sig->mac, obs->mac, 6);
    sig->mac_random = obs->mac_random;
    sig->type = wc_device_type_guess(obs);
    sig->rssi_max = obs->rssi;
    sig->obs_count = 1;
    sig->first_seen = now;
    sig->last_seen = now;
    sig->ie_hash = obs->ie_hash;
    sig->ie_vendor = obs->ie_vendor;
    wc_signature_add_ssid(sig, obs->probed_ssid);
}

void wc_signature_merge(WcSignature *sig, const WcObservation *obs, uint32_t now) {
    sig->obs_count++;
    sig->last_seen = now;
    if (obs->rssi != 0 && (sig->rssi_max == 0 || obs->rssi > sig->rssi_max)) {
        sig->rssi_max = obs->rssi;
    }
    // A later beacon or vendor-bearing frame can sharpen an initially unknown type.
    if (sig->type == WcDeviceUnknown) {
        sig->type = wc_device_type_guess(obs);
    }
    if (sig->ie_hash == 0) {
        sig->ie_hash = obs->ie_hash;
    }
    if (sig->ie_vendor == WcVendorUnknown) {
        sig->ie_vendor = obs->ie_vendor;
    }
    wc_signature_add_ssid(sig, obs->probed_ssid);
}

void wc_signature_absorb(WcSignature *dst, const WcSignature *src) {
    dst->obs_count += src->obs_count;
    // Zero means "not measured", not "epoch zero" or "0 dBm": a capture imported from a pcap
    // carries neither a clock nor a signal level, and plain min/max would let those unknowns
    // beat every real reading they are merged with.
    if (src->first_seen != 0 && (dst->first_seen == 0 || src->first_seen < dst->first_seen)) {
        dst->first_seen = src->first_seen;
    }
    if (src->last_seen > dst->last_seen) {
        dst->last_seen = src->last_seen;
    }
    if (src->rssi_max != 0 && (dst->rssi_max == 0 || src->rssi_max > dst->rssi_max)) {
        dst->rssi_max = src->rssi_max;
    }
    for (uint8_t i = 0; i < src->ssid_count; i++) {
        wc_signature_add_ssid(dst, src->ssids[i]);
    }
    if (dst->type == WcDeviceUnknown) {
        dst->type = src->type;
    }
    if (dst->ie_hash == 0) {
        dst->ie_hash = src->ie_hash;
    }
    if (dst->ie_vendor == WcVendorUnknown) {
        dst->ie_vendor = src->ie_vendor;
    }
}

const char *wc_signature_vendor(const WcSignature *sig) {
    const char *oui = wc_oui_vendor_name(sig->mac);
    if (oui[0] != '\0') {
        return oui;
    }
    return wc_vendor_name(sig->ie_vendor);
}
