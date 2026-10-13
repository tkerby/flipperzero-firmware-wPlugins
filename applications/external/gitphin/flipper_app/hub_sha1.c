#include "hub_sha1.h"

#include <string.h>
#include "hub_mem.h"

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_block(uint32_t h[5], const uint8_t* p) {
    uint32_t w[16];
    for(int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for(int i = 0; i < 80; i++) {
        uint32_t f, k;
        if(i >= 16) {
            /* rolling 16 word schedule */
            uint32_t t = w[(i - 3) & 15] ^ w[(i - 8) & 15] ^ w[(i - 14) & 15] ^ w[i & 15];
            w[i & 15] = ROL(t, 1);
        }
        if(i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999;
        } else if(i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1;
        } else if(i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdc;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6;
        }
        uint32_t t = ROL(a, 5) + f + e + k + w[i & 15];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void hub_sha1_init(HubSha1* s) {
    s->h[0] = 0x67452301;
    s->h[1] = 0xefcdab89;
    s->h[2] = 0x98badcfe;
    s->h[3] = 0x10325476;
    s->h[4] = 0xc3d2e1f0;
    s->len = 0;
    s->n = 0;
}

void hub_sha1_update(HubSha1* s, const void* data, size_t len) {
    const uint8_t* p = data;
    s->len += len;
    if(s->n > 0) {
        size_t take = 64 - s->n;
        if(take > len) take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += (uint32_t)take;
        p += take;
        len -= take;
        if(s->n < 64) return;
        sha1_block(s->h, s->buf);
        s->n = 0;
    }
    while(len >= 64) {
        sha1_block(s->h, p);
        p += 64;
        len -= 64;
    }
    if(len > 0) {
        memcpy(s->buf, p, len);
        s->n = (uint32_t)len;
    }
}

void hub_sha1_final(HubSha1* s, uint8_t out[20]) {
    uint64_t bits = s->len * 8;
    s->buf[s->n++] = 0x80;
    if(s->n > 56) {
        memset(s->buf + s->n, 0, 64 - s->n);
        sha1_block(s->h, s->buf);
        s->n = 0;
    }
    memset(s->buf + s->n, 0, 56 - s->n);
    for(int i = 0; i < 8; i++)
        s->buf[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_block(s->h, s->buf);
    for(int i = 0; i < 5; i++) {
        out[4 * i] = (uint8_t)(s->h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8);
        out[4 * i + 3] = (uint8_t)s->h[i];
    }
}
