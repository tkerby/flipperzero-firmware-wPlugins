#include "hub_req.h"
#include "hub_gitobj.h"

#include <string.h>
#include <stdio.h>
#include "hub_mem.h"

/* JSON text helpers and the chunked writer (split from hub_http.c so the host tests build them). */

/* Length of the valid UTF-8 sequence at p, or 0 when it is not valid. */
static size_t hub_utf8_len(const unsigned char* p, size_t n) {
    unsigned char c = p[0];
    if(c < 0x80) return 1;
    size_t len;
    uint32_t cp;
    if(c >= 0xC2 && c <= 0xDF) {
        len = 2;
        cp = c & 0x1F;
    } else if(c >= 0xE0 && c <= 0xEF) {
        len = 3;
        cp = c & 0x0F;
    } else if(c >= 0xF0 && c <= 0xF4) {
        len = 4;
        cp = c & 0x07;
    } else {
        return 0;
    }
    if(n < len) return 0;
    for(size_t i = 1; i < len; i++) {
        if((p[i] & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    if((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) ||
       (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)))
        return 0;
    return len;
}

/* Quoted JSON string from bytes that may be anything. Invalid UTF-8 becomes U+FFFD. */
void hub_json_bytes(FuriString* s, const char* v, size_t n) {
    furi_string_push_back(s, '"');
    for(size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)v[i];
        if(c == '"' || c == '\\') {
            furi_string_push_back(s, '\\');
            furi_string_push_back(s, (char)c);
            i++;
        } else if(c < 0x20 || c == 0x7f) {
            furi_string_cat_printf(s, "\\u%04x", c);
            i++;
        } else if(c < 0x80) {
            furi_string_push_back(s, (char)c);
            i++;
        } else {
            size_t l = hub_utf8_len((const unsigned char*)v + i, n - i);
            if(l == 0) {
                furi_string_cat_str(s, "\\ufffd");
                i++;
            } else {
                for(size_t k = 0; k < l; k++)
                    furi_string_push_back(s, v[i + k]);
                i += l;
            }
        }
    }
    furi_string_push_back(s, '"');
}

void hub_json_str(FuriString* s, const char* v) {
    hub_json_bytes(s, v, strlen(v));
}

const char* hub_cache_hdr(const char* ref) {
    HubOid o; /* 40 hex digits = an id; the object never changes */
    return hub_oid_from_hex(ref, &o) ? "Cache-Control: public, max-age=31536000, immutable\r\n" :
                                       "Cache-Control: no-cache\r\n";
}

/* ---------- chunked answers: no buffer grows with the answer ---------- */

#define CH_HDR 8 /* room before the data for "<hex>\r\n" */

static void chunk_flush(HubChunk* c) {
    if(c->n == 0 || !c->ok) return;
    char* p = c->b + CH_HDR;
    p[c->n] = '\r';
    p[c->n + 1] = '\n';
    size_t hl = 2;
    char* q = p - 2;
    q[0] = '\r';
    q[1] = '\n';
    for(size_t v = c->n; v; v >>= 4) {
        *--q = "0123456789abcdef"[v & 15];
        hl++;
    }
    c->ok = hub_conn_write(c->r->t, q, hl + c->n + 2);
    c->n = 0;
}

bool hub_chunk_begin(HubChunk* c, HubReq* r, const char* ctype, const char* extra) {
    c->r = r;
    c->n = 0;
    c->ok = hub_send_head(r, 200, ctype, HUB_CHUNKED, extra);
    return c->ok;
}

void hub_chunk_write(HubChunk* c, const char* d, size_t n) {
    while(n && c->ok) {
        size_t k = HUB_CHUNK_SIZE - c->n;
        if(k > n) k = n;
        memcpy(c->b + CH_HDR + c->n, d, k);
        c->n += k;
        d += k;
        n -= k;
        if(c->n == HUB_CHUNK_SIZE) chunk_flush(c);
    }
}

/* Quoted JSON string of bytes, escaped straight into the chunks. */
void hub_chunk_json(HubChunk* c, const char* v, size_t n) {
    char t[8];
    hub_chunk_write(c, "\"", 1);
    for(size_t i = 0; i < n && c->ok;) {
        unsigned char ch = (unsigned char)v[i];
        size_t l = 1, k = 1;
        t[0] = (char)ch;
        if(ch == '"' || ch == '\\') {
            t[0] = '\\';
            t[1] = (char)ch;
            k = 2;
        } else if(ch < 0x20 || ch == 0x7f) {
            k = (size_t)snprintf(t, sizeof(t), "\\u%04x", ch);
        } else if(ch >= 0x80) {
            l = hub_utf8_len((const unsigned char*)v + i, n - i);
            if(l == 0) {
                memcpy(t, "\\ufffd", 6);
                k = 6;
                l = 1;
            } else {
                hub_chunk_write(c, v + i, l);
                i += l;
                continue;
            }
        }
        hub_chunk_write(c, t, k);
        i += l;
    }
    hub_chunk_write(c, "\"", 1);
}

void hub_chunk_fs(HubChunk* c, FuriString* s) {
    hub_chunk_write(c, furi_string_get_cstr(s), furi_string_size(s));
}

/* false: the connection broke or the work failed; the final chunk is left out
 * then, so the client sees a failed transfer and not a short valid answer. */
bool hub_chunk_end(HubChunk* c, bool good) {
    chunk_flush(c);
    if(good && c->ok && !hub_deadline_expired()) {
        c->ok = hub_conn_write(c->r->t, "0\r\n\r\n", 5);
    } else {
        c->ok = false;
    }
    return c->ok;
}
