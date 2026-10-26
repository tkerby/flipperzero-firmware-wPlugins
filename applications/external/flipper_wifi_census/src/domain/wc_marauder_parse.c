#include "include/domain/wc_marauder_parse.h"

#include <string.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Parse exactly "hh:hh:hh:hh:hh:hh" at the start of s (which has at least 17 readable bytes).
static bool parse_mac_at(const char *s, uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        int hi = hex_val(s[i * 3]);
        int lo = hex_val(s[i * 3 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        if (i < 5 && s[i * 3 + 2] != ':')
            return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Find the first MAC anywhere in the NUL-terminated buffer. Returns true and fills mac.
static bool find_mac(const char *buf, size_t buflen, uint8_t mac[6]) {
    if (buflen < 17)
        return false;
    for (size_t i = 0; i + 17 <= buflen; i++) {
        if (parse_mac_at(buf + i, mac))
            return true;
    }
    return false;
}

static bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// A label only counts where a word starts: "SSID" must not match inside "BSSID" or "ESSID", or
// an access point's MAC text is stored as the network a client was asking for, and the real
// name is never recorded. Returns the character after the label, or NULL.
static const char *after_label(const char *buf, const char *label) {
    const size_t n = strlen(label);
    for (const char *p = strstr(buf, label); p; p = strstr(p + 1, label)) {
        if (p == buf || !is_word_char(p[-1])) {
            return p + n;
        }
    }
    return NULL;
}

// After `key`, skip separators (: = space) and read an optional sign and decimal digits.
// Returns true and sets *val when a number is found.
static bool int_after(const char *buf, const char *key, int *val) {
    const char *p = after_label(buf, key);
    if (!p)
        return false;
    while (*p == ':' || *p == '=' || *p == ' ')
        p++;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return false;
    int n = 0;
    int digits = 0;
    // Cap the digit count so untrusted input cannot overflow the accumulator (UB). Six
    // digits is far more than any real RSSI/channel; extra digits are ignored.
    while (*p >= '0' && *p <= '9' && digits < 6) {
        n = n * 10 + (*p - '0');
        p++;
        digits++;
    }
    *val = sign * n;
    return true;
}

static int8_t clamp_i8(int v) {
    if (v > 127)
        return 127;
    if (v < -128)
        return -128;
    return (int8_t)v;
}

// Copy a quoted string ("...") starting at `q` (which points at the opening quote) into out.
static void copy_quoted(const char *q, char *out) {
    out[0] = '\0';
    const char *p = q + 1;
    size_t n = 0;
    while (*p && *p != '"' && n < WC_SSID_MAX_LEN) {
        out[n++] = *p++;
    }
    out[n] = '\0';
}

// Copy the rest of an unquoted value (until CR/LF/end) into out, trimming trailing spaces.
static void copy_unquoted(const char *p, char *out) {
    size_t n = 0;
    while (*p && n < WC_SSID_MAX_LEN) {
        if (*p == '\r' || *p == '\n') {
            break;
        }
        out[n++] = *p++;
    }
    while (n > 0 && out[n - 1] == ' ') {
        n--;
    }
    out[n] = '\0';
}

// Return a pointer just past `label` and any following separators (: = space), or NULL if the
// label is not present in buf.
static const char *value_after_label(const char *buf, const char *label) {
    const char *p = after_label(buf, label);
    if (!p)
        return NULL;
    while (*p == ':' || *p == '=' || *p == ' ')
        p++;
    return p;
}

// Extract a directed SSID into out (max WC_SSID_MAX_LEN). Marauder v1.17 probe-sniff prints the
// probed network after a "Requesting:" label; other builds/paths use an "SSID" label, and some
// quote the name. Tries each in turn, then a bare quoted string. Leaves out[0]=0 when the value
// is absent or empty (a wildcard probe).
static void extract_ssid(const char *buf, char *out) {
    out[0] = '\0';
    const char *p = value_after_label(buf, "SSID");
    if (!p)
        p = value_after_label(buf, "ESSID"); // a real label; only BSSID must never match
    if (!p)
        p = value_after_label(buf, "Requesting");
    if (!p) {
        const char *q = strchr(buf, '"');
        if (q) {
            copy_quoted(q, out);
        }
        return;
    }
    if (*p == '"') {
        copy_quoted(p, out);
        return;
    }
    copy_unquoted(p, out);
}

bool wc_parse_summary_line(const char *line, size_t len, WcObservation *out) {
    char buf[WC_LINE_MAX];
    size_t n = len < (WC_LINE_MAX - 1) ? len : (WC_LINE_MAX - 1);
    memcpy(buf, line, n);
    buf[n] = '\0';

    memset(out, 0, sizeof(*out));
    if (!find_mac(buf, n, out->mac)) {
        return false;
    }
    out->mac_random = wc_mac_is_random(out->mac);

    int rssi = 0;
    if (int_after(buf, "RSSI", &rssi)) {
        out->rssi = clamp_i8(rssi);
    } else {
        // Marauder v1.17 probe lines have no "RSSI" label: they begin with the RSSI as a bare
        // signed dBm value (e.g. "-86 Ch: 1 ..."). Read a leading "-<digits>" if present.
        const char *p = buf;
        while (*p == ' ')
            p++;
        if (*p == '-' && p[1] >= '0' && p[1] <= '9') {
            p++;
            int v = 0, digits = 0;
            while (*p >= '0' && *p <= '9' && digits < 6) {
                v = v * 10 + (*p - '0');
                p++;
                digits++;
            }
            out->rssi = clamp_i8(-v);
        }
    }
    int ch = 0;
    // Accept both "CH" (older labeled output) and Marauder v1.17's "Ch".
    if (int_after(buf, "CH", &ch) || int_after(buf, "Ch", &ch)) {
        if (ch < 0)
            ch = 0;
        if (ch > 14)
            ch = 14;
        out->channel = (uint8_t)ch;
    }
    extract_ssid(buf, out->probed_ssid);

    // Marauder's beacon-list output labels access points; probe-sniff lines are clients.
    // Only what precedes the network name can say what kind of frame this is: a phone probing
    // for a network called "BEACON" is a phone, and typing it as an access point would drop it
    // out of both the linking rule and every comparison.
    const char *ssid_at = value_after_label(buf, "SSID");
    if (!ssid_at) {
        ssid_at = value_after_label(buf, "ESSID");
    }
    size_t head = ssid_at ? (size_t)(ssid_at - buf) : strlen(buf);
    char kind[WC_LINE_MAX];
    size_t kn = head < sizeof(kind) - 1 ? head : sizeof(kind) - 1;
    memcpy(kind, buf, kn);
    kind[kn] = '\0';
    out->is_beacon = (after_label(kind, "BEACON") != NULL) || (after_label(kind, "AP:") != NULL);
    return true;
}
