#include "hub_gitdocutil.h"

#include <string.h>
#include "hub_mem.h"

static bool key_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-';
}

/* seg..seg+n is one path segment */
static bool seg_ok(const char* seg, size_t n) {
    if(n == 0 || seg[0] == '.') return false;
    if(n >= 5 && memcmp(seg + n - 5, ".part", 5) == 0) return false;
    for(size_t i = 0; i < n; i++) {
        if(!key_char(seg[i])) return false;
        if(seg[i] == '.' && i + 1 < n && seg[i + 1] == '.') return false;
    }
    return true;
}

static bool path_ok(const char* s, size_t len) {
    size_t start = 0;
    for(size_t i = 0; i <= len; i++) {
        if(i == len || s[i] == '/') {
            if(!seg_ok(s + start, i - start)) return false;
            start = i + 1;
        }
    }
    return true;
}

bool hub_gitdoc_key_ok(const char* key) {
    size_t len = strlen(key);
    if(len == 0 || len > HUB_GITDOC_KEY_MAX) return false;
    return path_ok(key, len);
}

bool hub_gitdoc_prefix_ok(const char* prefix) {
    size_t len = strlen(prefix);
    if(len == 0) return true;
    if(len > HUB_GITDOC_KEY_MAX + 1) return false;
    if(prefix[len - 1] == '/') len--;
    if(len == 0 || len > HUB_GITDOC_KEY_MAX) return false;
    return path_ok(prefix, len);
}

uint32_t hub_crc32(uint32_t crc, const void* data, size_t len) {
    static const uint32_t tab[16] = {
        0x00000000,
        0x1db71064,
        0x3b6e20c8,
        0x26d930ac,
        0x76dc4190,
        0x6b6b51f4,
        0x4db26158,
        0x5005713c,
        0xedb88320,
        0xf00f9344,
        0xd6d6a3e8,
        0xcb61b38c,
        0x9b64c2b0,
        0x86d3d2d4,
        0xa00ae278,
        0xbdbdf21c};
    const uint8_t* p = data;
    crc = ~crc;
    for(size_t i = 0; i < len; i++) {
        crc ^= p[i];
        crc = (crc >> 4) ^ tab[crc & 15];
        crc = (crc >> 4) ^ tab[crc & 15];
    }
    return ~crc;
}

void hub_gitdoc_etag(uint32_t crc, char out[11]) {
    static const char hex[] = "0123456789abcdef";
    out[0] = '"';
    for(int i = 0; i < 8; i++)
        out[1 + i] = hex[(crc >> (28 - 4 * i)) & 15];
    out[9] = '"';
    out[10] = '\0';
}

static const char* skip_ws(const char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    return s;
}

/* Does the header value (a list of entity tags or *) hold the tag of crc? */
static bool list_has(const char* v, uint32_t crc) {
    char want[11];
    hub_gitdoc_etag(crc, want);
    v = skip_ws(v);
    if(v[0] == '*' && (v[1] == '\0' || v[1] == ' ' || v[1] == '\t')) return true;
    while(*v) {
        v = skip_ws(v);
        if(*v == ',') {
            v++;
            continue;
        }
        bool weak = (v[0] == 'W' || v[0] == 'w') && v[1] == '/';
        if(weak) v += 2;
        const char* t = v;
        while(*v && *v != ',' && *v != ' ' && *v != '\t')
            v++;
        size_t n = (size_t)(v - t);
        if(!weak) {
            /* "xxxxxxxx" (quoted) or xxxxxxxx (lenient) */
            bool quoted = n == 10 && t[0] == '"' && t[9] == '"';
            const char* h = quoted ? t + 1 : t;
            size_t hn = quoted ? 8 : n;
            if(hn == 8 && (quoted || n == 8)) {
                bool eq = true;
                for(int i = 0; i < 8; i++) {
                    char c = h[i];
                    if(c >= 'A' && c <= 'F') c += 'a' - 'A';
                    if(c != want[1 + i]) eq = false;
                }
                if(eq) return true;
            }
        }
    }
    return false;
}

static bool is_star(const char* v) {
    v = skip_ws(v);
    if(v[0] != '*') return false;
    return *skip_ws(v + 1) == '\0';
}

int hub_gitdoc_precond(
    const char* if_match,
    const char* if_none_match,
    bool need_match,
    bool exists,
    uint32_t cur_crc) {
    bool have = false;
    if(if_none_match) {
        if(need_match || !is_star(if_none_match)) return 400;
        have = true;
        if(exists) return 412;
    }
    if(if_match) {
        have = true;
        if(!exists || !list_has(if_match, cur_crc)) return 412;
    }
    if(!have) return 400;
    return 0;
}

const char* hub_gitdoc_hdr(const char* line, const char* name) {
    for(; *name; line++, name++) {
        char a = *line;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(a != *name) return NULL;
    }
    if(*line != ':') return NULL;
    return skip_ws(line + 1);
}

void hub_gitdoc_scan_head(
    const char* head,
    size_t len,
    const char** if_match,
    const char** if_none_match) {
    size_t pos = 0;
    while(pos < len) {
        if(head[pos] == '\n' || head[pos] == '\r') {
            pos++;
            continue;
        }
        const char* line = head + pos;
        size_t n = 0;
        while(pos + n < len && line[n])
            n++;
        const char* v;
        if((v = hub_gitdoc_hdr(line, "if-match"))) {
            *if_match = v;
        } else if((v = hub_gitdoc_hdr(line, "if-none-match"))) {
            *if_none_match = v;
        }
        pos += n + 1;
    }
}
