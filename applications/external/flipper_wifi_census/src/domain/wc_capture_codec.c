#include "include/domain/wc_capture_codec.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const uint8_t k_magic[4] = {'W', 'C', 'E', 'N'};

#define WC_HEADER_SIZE (4 + 2 + WC_LABEL_MAX + 4 + 4 + 2 + 1 + 2)
#define WC_SSID_SLOT (WC_SSID_MAX_LEN + 1)
// v1 record layout; v2 appends ie_hash (4) + ie_vendor (1) at the end, so v1 is a prefix of
// v2 and old captures still read.
#define WC_RECORD_V1 (6 + 1 + 1 + 1 + 1 + 4 + 4 + 4 + WC_SIG_MAX_SSIDS * WC_SSID_SLOT)
#define WC_RECORD_V2 (WC_RECORD_V1 + 4 + 1)

static size_t record_size(uint16_t version) {
    return version >= 2 ? WC_RECORD_V2 : WC_RECORD_V1;
}

// --- little-endian primitives (endian-independent on any host) ---

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t wc_capture_size(const WcCensus *c) {
    return WC_HEADER_SIZE + (size_t)c->count * WC_RECORD_V2;
}

size_t wc_capture_header_size(void) {
    return WC_HEADER_SIZE;
}

size_t wc_capture_record_size(void) {
    return WC_RECORD_V2;
}

size_t wc_capture_record_size_of(uint16_t version) {
    return record_size(version);
}

void wc_capture_put_header(uint8_t *out, const WcCaptureMeta *meta, uint16_t count) {
    uint8_t *p = out;
    memcpy(p, k_magic, 4);
    p += 4;
    put_u16(p, WC_CAP_VERSION);
    p += 2;
    memset(p, 0, WC_LABEL_MAX);
    strncpy((char *)p, meta->label, WC_LABEL_MAX);
    p += WC_LABEL_MAX;
    put_u32(p, meta->epoch);
    p += 4;
    put_u32(p, meta->duration_s);
    p += 4;
    put_u16(p, meta->channels_mask);
    p += 2;
    *p++ = meta->mode;
    put_u16(p, count);
}

void wc_capture_put_record(uint8_t *out, const WcSignature *d) {
    uint8_t *p = out;
    memcpy(p, d->mac, 6);
    p += 6;
    *p++ = d->mac_random ? 1 : 0;
    *p++ = (uint8_t)d->type;
    *p++ = (uint8_t)d->rssi_max;
    *p++ = d->ssid_count;
    put_u32(p, d->obs_count);
    p += 4;
    put_u32(p, d->first_seen);
    p += 4;
    put_u32(p, d->last_seen);
    p += 4;
    for (uint8_t s = 0; s < WC_SIG_MAX_SSIDS; s++) {
        memset(p, 0, WC_SSID_SLOT);
        if (s < d->ssid_count) {
            strncpy((char *)p, d->ssids[s], WC_SSID_MAX_LEN);
        }
        p += WC_SSID_SLOT;
    }
    put_u32(p, d->ie_hash); // v2 fields
    p += 4;
    *p++ = (uint8_t)d->ie_vendor;
}

size_t wc_capture_write(uint8_t *buf, size_t cap, const WcCaptureMeta *meta, const WcCensus *c) {
    size_t need = wc_capture_size(c);
    if (cap < need) {
        return 0;
    }
    wc_capture_put_header(buf, meta, c->count);
    uint8_t *p = buf + WC_HEADER_SIZE;
    for (uint16_t i = 0; i < c->count; i++) {
        wc_capture_put_record(p, &c->devices[i]);
        p += WC_RECORD_V2;
    }
    return need;
}

uint16_t wc_capture_peek_count(const uint8_t *buf, size_t len) {
    if (len < WC_HEADER_SIZE || memcmp(buf, k_magic, sizeof(k_magic)) != 0) {
        return 0;
    }
    return get_u16(buf + WC_HEADER_SIZE - 2);
}

bool wc_capture_get_header(const uint8_t *in, size_t len, WcCaptureMeta *meta, uint16_t *count,
                           uint16_t *version) {
    if (len < WC_HEADER_SIZE || memcmp(in, k_magic, 4) != 0) {
        return false;
    }
    const uint8_t *p = in + 4;
    uint16_t v = get_u16(p);
    if (v < 1 || v > WC_CAP_VERSION) {
        return false;
    }
    p += 2;
    memset(meta, 0, sizeof(*meta));
    memcpy(meta->label, p, WC_LABEL_MAX);
    meta->label[WC_LABEL_MAX] = '\0';
    p += WC_LABEL_MAX;
    meta->epoch = get_u32(p);
    p += 4;
    meta->duration_s = get_u32(p);
    p += 4;
    meta->channels_mask = get_u16(p);
    p += 2;
    meta->mode = *p++;
    *count = get_u16(p);
    *version = v;
    return true;
}

bool wc_capture_get_record(const uint8_t *in, size_t len, uint16_t version, WcSignature *out) {
    if (len < record_size(version)) {
        return false;
    }
    const uint8_t *p = in;
    memset(out, 0, sizeof(*out));
    memcpy(out->mac, p, 6);
    p += 6;
    out->mac_random = (*p++ != 0);
    out->type = (WcDeviceType)*p++;
    out->rssi_max = (int8_t)*p++;
    uint8_t ssid_count = *p++;
    if (ssid_count > WC_SIG_MAX_SSIDS) {
        return false;
    }
    out->ssid_count = ssid_count;
    out->obs_count = get_u32(p);
    p += 4;
    out->first_seen = get_u32(p);
    p += 4;
    out->last_seen = get_u32(p);
    p += 4;
    for (uint8_t i = 0; i < WC_SIG_MAX_SSIDS; i++) {
        memcpy(out->ssids[i], p, WC_SSID_MAX_LEN);
        out->ssids[i][WC_SSID_MAX_LEN] = '\0';
        p += WC_SSID_SLOT;
    }
    if (version >= 2) {
        out->ie_hash = get_u32(p);
        p += 4;
        out->ie_vendor = (WcVendor)*p++;
    }
    return true;
}

bool wc_capture_read(WcCaptureMeta *meta, WcCensus *c, const uint8_t *buf, size_t len) {
    const uint16_t ceiling = c->max; // the caller's, not this build's
    uint16_t count = 0, version = 0;
    if (!wc_capture_get_header(buf, len, meta, &count, &version)) {
        return false;
    }
    const uint8_t *p = buf + WC_HEADER_SIZE;

    // Refused whole rather than loaded in part, against the caller's ceiling: a host tool that
    // opened its census wide reads a file the Flipper has to turn down.
    if (count > ceiling) {
        return false;
    }
    if (len != (size_t)WC_HEADER_SIZE + (size_t)count * record_size(version)) {
        return false;
    }

    wc_census_free(c);
    wc_census_set_max(c, ceiling);
    wc_census_reserve(c, count);
    for (uint16_t i = 0; i < count; i++) {
        WcSignature d;
        if (!wc_capture_get_record(p, record_size(version), version, &d) || !wc_census_add(c, &d)) {
            wc_census_free(c);
            return false;
        }
        p += record_size(version);
    }
    return true;
}

// --- CSV ---

// Append to a bounded buffer, tracking the length a full render needs even past `cap`.
static void csv_appendf(char *out, size_t cap, size_t *len, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t room = (*len < cap) ? (cap - *len) : 0;
    int n = vsnprintf(out + (*len < cap ? *len : cap), room, fmt, ap);
    va_end(ap);
    if (n > 0) {
        *len += (size_t)n;
    }
}

size_t wc_capture_csv_header(char *out, size_t cap) {
    size_t len = 0;
    if (cap > 0) {
        out[0] = '\0';
    }
    csv_appendf(
        out, cap, &len,
        "mac,random,type,vendor,fingerprint,rssi_max,obs_count,first_seen,last_seen,ssids\n");
    return len;
}

size_t wc_capture_csv_row(char *out, size_t cap, const WcSignature *d) {
    size_t len = 0;
    if (cap > 0) {
        out[0] = '\0';
    }
    csv_appendf(out, cap, &len, "%02X:%02X:%02X:%02X:%02X:%02X,%d,%s,%s,%08lx,%d,%lu,%lu,%lu,\"",
                d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5],
                d->mac_random ? 1 : 0, wc_device_type_name(d->type), wc_signature_vendor(d),
                (unsigned long)d->ie_hash, d->rssi_max, (unsigned long)d->obs_count,
                (unsigned long)d->first_seen, (unsigned long)d->last_seen);
    for (uint8_t s = 0; s < d->ssid_count; s++) {
        // Escape embedded quotes so the quoted SSID field stays valid CSV.
        const char *ss = d->ssids[s];
        csv_appendf(out, cap, &len, "%s", s ? ";" : "");
        for (const char *ch = ss; *ch; ch++) {
            if (*ch == '"') {
                csv_appendf(out, cap, &len, "\"\"");
            } else {
                csv_appendf(out, cap, &len, "%c", *ch);
            }
        }
    }
    csv_appendf(out, cap, &len, "\"\n");
    return len;
}

size_t wc_capture_to_csv(char *out, size_t cap, const WcCaptureMeta *meta, const WcCensus *c) {
    (void)meta;
    size_t len = wc_capture_csv_header(out, cap);
    for (uint16_t i = 0; i < c->count; i++) {
        len += wc_capture_csv_row((len < cap) ? out + len : out + cap, (len < cap) ? cap - len : 0,
                                  &c->devices[i]);
    }
    return len;
}
