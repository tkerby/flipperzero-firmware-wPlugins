#include "hub_deflate.h"

#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define WSIZE     2048 /* window = ring buffer */
#define BLK       512 /* input per block; ring aligned, so a block never wraps */
#define HBITS     9
#define MAX_CHAIN 12
#define MIN_MATCH 3
#define MAX_MATCH 258
#define OB_SIZE   256
#define TB_SIZE   (BLK + BLK / 8 + 64) /* fixed-Huffman worst case is 9 bits per byte */

struct HubDeflate {
    HubDeflateOut out;
    void* ctx;
    bool err;
    uint32_t adler_a, adler_b;
    uint32_t pos; /* bytes taken in so far */
    uint16_t fill; /* bytes of the current block */

    uint32_t bitbuf; /* main bit writer */
    int bitcnt;
    uint16_t on;

    uint16_t head[1 << HBITS];
    uint16_t prev[WSIZE];
    uint8_t ring[WSIZE];
    uint8_t ob[OB_SIZE];
    uint8_t tb[TB_SIZE]; /* the fixed-Huffman block under construction */
};

static const uint16_t lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dbase[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* ---------- main output ---------- */

static void ob_flush(HubDeflate* d) {
    if(d->on && !d->err) {
        if(!d->out(d->ctx, d->ob, d->on)) d->err = true;
    }
    d->on = 0;
}

static inline void ob_byte(HubDeflate* d, uint8_t b) {
    if(d->on == OB_SIZE) ob_flush(d);
    d->ob[d->on++] = b;
}

/* n <= 16 bits, LSB first */
static void put_bits(HubDeflate* d, uint32_t v, int n) {
    d->bitbuf |= v << d->bitcnt;
    d->bitcnt += n;
    while(d->bitcnt >= 8) {
        ob_byte(d, (uint8_t)d->bitbuf);
        d->bitbuf >>= 8;
        d->bitcnt -= 8;
    }
}

static void align(HubDeflate* d) {
    if(d->bitcnt) put_bits(d, 0, 8 - d->bitcnt);
}

/* ---------- temp bit writer for one block ---------- */

typedef struct {
    uint8_t* b;
    uint32_t nbits;
    bool over;
} TB;

static void tb_put(TB* t, uint32_t v, int n) {
    for(int i = 0; i < n; i++) {
        if((t->nbits >> 3) >= TB_SIZE) {
            t->over = true;
            return;
        }
        if((t->nbits & 7) == 0) t->b[t->nbits >> 3] = 0;
        if((v >> i) & 1) t->b[t->nbits >> 3] |= (uint8_t)(1u << (t->nbits & 7));
        t->nbits++;
    }
}

/* Huffman codes go MSB first */
static void tb_code(TB* t, uint32_t code, int len) {
    uint32_t r = 0;
    for(int i = 0; i < len; i++)
        r |= ((code >> i) & 1u) << (len - 1 - i);
    tb_put(t, r, len);
}

static void tb_lit(TB* t, unsigned sym) { /* literal or length symbol 0..285 */
    if(sym < 144)
        tb_code(t, 0x30 + sym, 8);
    else if(sym < 256)
        tb_code(t, 0x190 + (sym - 144), 9);
    else if(sym < 280)
        tb_code(t, sym - 256, 7);
    else
        tb_code(t, 0xC0 + (sym - 280), 8);
}

static void tb_match(TB* t, unsigned len, unsigned dist) {
    unsigned li = 28;
    while(lbase[li] > len)
        li--;
    tb_lit(t, 257 + li);
    tb_put(t, len - lbase[li], lext[li]);
    unsigned di = 29;
    while(dbase[di] > dist)
        di--;
    tb_code(t, di, 5);
    tb_put(t, dist - dbase[di], dext[di]);
}

/* ---------- LZ77 ---------- */

static inline unsigned hash3(const uint8_t* p) {
    return (((uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]) * 2654435761u) >> (32 - HBITS);
}

static inline void insert(HubDeflate* d, uint32_t abs, const uint8_t* p) {
    unsigned h = hash3(p);
    d->prev[abs & (WSIZE - 1)] = d->head[h];
    d->head[h] = (uint16_t)abs;
}

/* Compress the current block (fill bytes) and write it. */
static void do_block(HubDeflate* d, bool final) {
    uint32_t S = d->pos - d->fill;
    const uint8_t* data = d->ring + (S & (WSIZE - 1));
    unsigned fill = d->fill;
    uint32_t end = S + fill;
    uint32_t lowest = end > WSIZE ? end - WSIZE : 0;

    TB t = {d->tb, 0, false};
    tb_put(&t, final ? 1 : 0, 1);
    tb_put(&t, 1, 2);

    unsigned i = 0;
    while(i < fill && !t.over) {
        uint32_t p = S + i;
        unsigned best = 0, bestd = 0;
        unsigned maxlen = fill - i < MAX_MATCH ? fill - i : MAX_MATCH;
        if(fill - i >= MIN_MATCH) {
            unsigned h = hash3(data + i);
            uint16_t cand = d->head[h];
            unsigned lastd = 0;
            for(int chain = 0; chain < MAX_CHAIN; chain++) {
                unsigned dist = (uint16_t)(p - cand);
                if(dist == 0 || dist > p - lowest || dist <= lastd) break;
                lastd = dist;
                unsigned l = 0;
                while(l < maxlen && d->ring[(p - dist + l) & (WSIZE - 1)] == data[i + l])
                    l++;
                if(l > best) {
                    best = l;
                    bestd = dist;
                    if(l == maxlen) break;
                }
                cand = d->prev[cand & (WSIZE - 1)];
            }
            insert(d, p, data + i);
        }
        if(best >= MIN_MATCH) {
            tb_match(&t, best, bestd);
            for(unsigned k = 1; k < best; k++) {
                if(fill - (i + k) >= MIN_MATCH) insert(d, p + k, data + i + k);
            }
            i += best;
        } else {
            tb_lit(&t, data[i]);
            i++;
        }
    }
    tb_lit(&t, 256);

    if(t.over || t.nbits > 8u * fill + 40u) {
        /* stored block */
        put_bits(d, final ? 1 : 0, 1);
        put_bits(d, 0, 2);
        align(d);
        put_bits(d, fill, 16);
        put_bits(d, ~fill & 0xffff, 16);
        for(unsigned k = 0; k < fill; k++)
            ob_byte(d, data[k]);
    } else {
        unsigned full = t.nbits >> 3;
        for(unsigned k = 0; k < full; k++)
            put_bits(d, d->tb[k], 8);
        if(t.nbits & 7) put_bits(d, d->tb[full] & ((1u << (t.nbits & 7)) - 1), (int)(t.nbits & 7));
    }
}

/* ---------- API ---------- */

void hub_deflate_reset(HubDeflate* d, HubDeflateOut out, void* ctx) {
    memset(d, 0, offsetof(HubDeflate, ring)); /* includes head and prev */
    d->out = out;
    d->ctx = ctx;
    d->adler_a = 1;
    ob_byte(d, 0x38); /* CINFO 3: the window is 2^(3+8) = 2048 bytes = WSIZE */
    ob_byte(d, 0x11); /* FCHECK: 0x3811 is a multiple of 31 */
}

HubDeflate* hub_deflate_new(HubDeflateOut out, void* ctx) {
    HubDeflate* d = malloc(sizeof(HubDeflate));
    if(!d) return NULL;
    hub_deflate_reset(d, out, ctx);
    return d;
}

void hub_deflate_free(HubDeflate* d) {
    free(d);
}

static void adler_update(HubDeflate* d, const uint8_t* p, size_t n) {
    uint32_t a = d->adler_a, b = d->adler_b;
    while(n) {
        size_t k = n > 5552 ? 5552 : n;
        n -= k;
        while(k--) {
            a += *p++;
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    d->adler_a = a;
    d->adler_b = b;
}

bool hub_deflate_write(HubDeflate* d, const void* data, size_t len) {
    const uint8_t* p = data;
    adler_update(d, p, len);
    while(len && !d->err) {
        size_t room = BLK - d->fill;
        size_t k = len < room ? len : room;
        memcpy(d->ring + (d->pos & (WSIZE - 1)), p, k);
        d->pos += (uint32_t)k;
        d->fill += (uint16_t)k;
        p += k;
        len -= k;
        if(d->fill == BLK) {
            do_block(d, false);
            d->fill = 0;
        }
    }
    return !d->err;
}

bool hub_deflate_finish(HubDeflate* d) {
    if(d->err) return false;
    do_block(d, true);
    d->fill = 0;
    align(d);
    uint32_t a = (d->adler_b << 16) | d->adler_a;
    ob_byte(d, (uint8_t)(a >> 24));
    ob_byte(d, (uint8_t)(a >> 16));
    ob_byte(d, (uint8_t)(a >> 8));
    ob_byte(d, (uint8_t)a);
    ob_flush(d);
    return !d->err;
}
