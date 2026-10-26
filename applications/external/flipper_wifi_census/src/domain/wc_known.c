#include "include/domain/wc_known.h"

#include <string.h>

static const uint8_t k_magic[4] = {'W', 'C', 'K', 'N'};

#define WC_KNOWN_HEADER_SIZE (4 + 2 + 2)
#define WC_KNOWN_SSID_SLOT (WC_SSID_MAX_LEN + 1)
#define WC_KNOWN_LABEL_SLOT (WC_KNOWN_LABEL_MAX + 1)
#define WC_KNOWN_ITEM_SIZE (WC_KNOWN_LABEL_SLOT + 1 + 6 + WC_KNOWN_SSID_SLOT)

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

void wc_known_init(WcKnownDb *db) {
    memset(db, 0, sizeof(*db));
}

bool wc_known_add(WcKnownDb *db, const WcKnown *k) {
    if (db->count >= WC_KNOWN_MAX) {
        return false;
    }
    db->items[db->count++] = *k;
    return true;
}

bool wc_known_rule_from_signature(WcKnown *out, const WcSignature *sig, const char *label) {
    memset(out, 0, sizeof(*out));
    strncpy(out->label, label, WC_KNOWN_LABEL_MAX);
    out->label[WC_KNOWN_LABEL_MAX] = '\0';
    if (!sig->mac_random) {
        out->type = WcRuleByMac;
        memcpy(out->mac, sig->mac, 6);
        return true;
    }
    if (sig->ssid_count > 0) {
        out->type = WcRuleBySsid;
        strncpy(out->ssid, sig->ssids[0], WC_SSID_MAX_LEN);
        out->ssid[WC_SSID_MAX_LEN] = '\0';
        return true;
    }
    // Randomized MAC with no directed SSID: nothing durable to key on.
    return false;
}

bool wc_known_rule_ssid(WcKnown *out, const char *ssid, const char *label) {
    if (!ssid || ssid[0] == '\0') {
        return false;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->label, label, WC_KNOWN_LABEL_MAX);
    out->label[WC_KNOWN_LABEL_MAX] = '\0';
    out->type = WcRuleBySsid;
    strncpy(out->ssid, ssid, WC_SSID_MAX_LEN);
    out->ssid[WC_SSID_MAX_LEN] = '\0';
    return true;
}

bool wc_known_remove(WcKnownDb *db, uint16_t index) {
    if (index >= db->count) {
        return false;
    }
    for (uint16_t i = index; i + 1 < db->count; i++) {
        db->items[i] = db->items[i + 1];
    }
    db->count--;
    return true;
}

const WcKnown *wc_known_match(const WcKnownDb *db, const WcSignature *sig) {
    for (uint16_t i = 0; i < db->count; i++) {
        const WcKnown *k = &db->items[i];
        if (k->type == WcRuleByMac) {
            if (!sig->mac_random && memcmp(k->mac, sig->mac, 6) == 0) {
                return k;
            }
        } else if (k->type == WcRuleBySsid) {
            if (wc_signature_has_ssid(sig, k->ssid)) {
                return k;
            }
        }
    }
    return NULL;
}

size_t wc_known_size(const WcKnownDb *db) {
    return WC_KNOWN_HEADER_SIZE + (size_t)db->count * WC_KNOWN_ITEM_SIZE;
}

size_t wc_known_write(uint8_t *buf, size_t cap, const WcKnownDb *db) {
    size_t need = wc_known_size(db);
    if (cap < need) {
        return 0;
    }
    uint8_t *p = buf;
    memcpy(p, k_magic, 4);
    p += 4;
    put_u16(p, WC_KNOWN_VERSION);
    p += 2;
    put_u16(p, db->count);
    p += 2;
    for (uint16_t i = 0; i < db->count; i++) {
        const WcKnown *k = &db->items[i];
        memset(p, 0, WC_KNOWN_LABEL_SLOT);
        strncpy((char *)p, k->label, WC_KNOWN_LABEL_MAX);
        p += WC_KNOWN_LABEL_SLOT;
        *p++ = (uint8_t)k->type;
        memcpy(p, k->mac, 6);
        p += 6;
        memset(p, 0, WC_KNOWN_SSID_SLOT);
        strncpy((char *)p, k->ssid, WC_SSID_MAX_LEN);
        p += WC_KNOWN_SSID_SLOT;
    }
    return need;
}

bool wc_known_read(WcKnownDb *db, const uint8_t *buf, size_t len) {
    if (len < WC_KNOWN_HEADER_SIZE) {
        return false;
    }
    const uint8_t *p = buf;
    if (memcmp(p, k_magic, 4) != 0) {
        return false;
    }
    p += 4;
    if (get_u16(p) != WC_KNOWN_VERSION) {
        return false;
    }
    p += 2;
    uint16_t count = get_u16(p);
    p += 2;
    if (count > WC_KNOWN_MAX) {
        return false;
    }
    if (len != (size_t)WC_KNOWN_HEADER_SIZE + (size_t)count * WC_KNOWN_ITEM_SIZE) {
        return false;
    }
    wc_known_init(db);
    for (uint16_t i = 0; i < count; i++) {
        WcKnown *k = &db->items[i];
        memset(k, 0, sizeof(*k));
        memcpy(k->label, p, WC_KNOWN_LABEL_MAX);
        k->label[WC_KNOWN_LABEL_MAX] = '\0';
        p += WC_KNOWN_LABEL_SLOT;
        k->type = (WcKnownRuleType)*p++;
        memcpy(k->mac, p, 6);
        p += 6;
        memcpy(k->ssid, p, WC_SSID_MAX_LEN);
        k->ssid[WC_SSID_MAX_LEN] = '\0';
        p += WC_KNOWN_SSID_SLOT;
    }
    db->count = count;
    return true;
}
