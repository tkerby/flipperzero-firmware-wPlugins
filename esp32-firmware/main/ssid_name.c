#include "ssid_name.h"

#include <stdbool.h>
#include <string.h>

// Printable ASCII only. This keeps the name renderable on the Flipper screen
// and blocks control characters and partial UTF-8 sequences, which some phones
// display as a blank or garbled network name.
static bool ssid_char_ok(char c) {
    return c >= 0x20 && c <= 0x7E;
}

static size_t ssid_sanitize_suffix(const char* in, char* out, size_t out_cap) {
    if(!in || !out_cap) return 0;
    size_t n = 0;
    bool pending_space = false;
    for(size_t i = 0; in[i] && n + 1 < out_cap; i++) {
        char c = in[i];
        if(!ssid_char_ok(c)) continue;
        if(c == ' ') {
            // Collapse runs, and never lead with a space.
            if(n > 0) pending_space = true;
            continue;
        }
        if(pending_space) {
            if(n + 2 >= out_cap) break;
            out[n++] = ' ';
            pending_space = false;
        }
        out[n++] = c;
    }
    out[n] = '\0'; // trailing space is simply never emitted
    return n;
}

size_t ssid_build(const char* suffix, char* out, size_t out_cap) {
    if(!out || !out_cap) return 0;

    const size_t prefix_len = strlen(SSID_PREFIX);
    const size_t sep_len = strlen(SSID_SEPARATOR);

    // The prefix is non-negotiable; if it cannot fit, emit what does and stop.
    size_t limit = (out_cap - 1 < SSID_MAX) ? out_cap - 1 : SSID_MAX;
    if(limit <= prefix_len) {
        memcpy(out, SSID_PREFIX, limit);
        out[limit] = '\0';
        return limit;
    }

    memcpy(out, SSID_PREFIX, prefix_len);
    size_t len = prefix_len;
    out[len] = '\0';

    char clean[SSID_MAX + 1];
    if(ssid_sanitize_suffix(suffix, clean, sizeof(clean)) == 0) return len;

    // Only append the separator if at least one suffix character fits after it.
    if(len + sep_len + 1 > limit) return len;
    memcpy(out + len, SSID_SEPARATOR, sep_len);
    len += sep_len;

    size_t room = limit - len;
    size_t take = strlen(clean);
    if(take > room) take = room;
    memcpy(out + len, clean, take);
    len += take;

    // Truncation can leave a dangling space; drop it.
    while(len > prefix_len + sep_len && out[len - 1] == ' ') len--;
    // If truncation ate the whole suffix, drop the separator too.
    if(len == prefix_len + sep_len) len = prefix_len;

    out[len] = '\0';
    return len;
}
