#include "hub_inflate.h"

#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define INBUF_SIZE 256
#define FAST_BITS  8
#define FAST_SIZE  (1 << FAST_BITS)
#define MAX_BITS   15
#define STOPPED    (-100) /* internal: partial output complete */

typedef struct {
    uint16_t count[MAX_BITS + 1];
    uint16_t symbol[288];
    uint16_t fast[FAST_SIZE]; /* sym | len << 9, 0 = not in table */
} Huff;

typedef struct {
    HubInfRead rd;
    void* rctx;
    HubInfWrite wr;
    void* wctx;

    uint8_t* win;
    size_t wsize;
    uint8_t wbits; /* log2 of the stream window, 0 = not known yet */
    HubInfFar far;
    void* fctx;
    uint64_t fc_pos; /* far window: the block read last */
    size_t fc_len;
    uint8_t fc[512];
    size_t pos;
    size_t base; /* bytes already flushed (stream mode) */
    bool stream;
    bool partial;

    uint32_t bitbuf;
    int bitcnt;
    size_t inpos;
    size_t inlen;
    size_t in_total; /* bytes taken from the callback */
    bool in_eof;

    int err;
    uint32_t adler_a, adler_b;

    Huff lit, dist;
    uint8_t lens[320];
    uint8_t in[INBUF_SIZE];
} Inf;

static const uint16_t lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dbase[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const uint8_t clorder[19] =
    {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* ---------- input ---------- */

static void refill(Inf* s) {
    while(s->bitcnt <= 24) {
        if(s->inpos >= s->inlen) {
            if(s->in_eof) return;
            size_t n = s->rd(s->rctx, s->in, INBUF_SIZE);
            if(n == 0) {
                s->in_eof = true;
                return;
            }
            if(n > INBUF_SIZE) n = INBUF_SIZE;
            s->inpos = 0;
            s->inlen = n;
        }
        s->bitbuf |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->in_total++;
        s->bitcnt += 8;
    }
}

/* Read n bits (0..16). On shortage sets the error and returns 0. */
static uint32_t bits(Inf* s, int n) {
    if(n == 0) return 0;
    if(s->bitcnt < n) {
        refill(s);
        if(s->bitcnt < n) {
            if(!s->err) s->err = HUB_INF_ERR_INPUT;
            return 0;
        }
    }
    uint32_t v = s->bitbuf & ((1u << n) - 1);
    s->bitbuf >>= n;
    s->bitcnt -= n;
    return v;
}

/* ---------- output ---------- */

static void adler_update(Inf* s, const uint8_t* p, size_t n) {
    uint32_t a = s->adler_a, b = s->adler_b;
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
    s->adler_a = a;
    s->adler_b = b;
}

/* stream mode: write out [0,pos) */
static bool flush(Inf* s) {
    if(s->pos == 0) return true;
    adler_update(s, s->win, s->pos);
    if(!s->wr(s->wctx, s->win, s->pos)) {
        if(!s->err) s->err = HUB_INF_ERR_WRITE;
        return false;
    }
    return true;
}

static inline bool put(Inf* s, uint8_t b) {
    if(s->pos == s->wsize) {
        if(s->stream) {
            if(!flush(s)) return false;
            s->base += s->pos;
            s->pos = 0;
        } else {
            if(!s->err) s->err = s->partial ? STOPPED : HUB_INF_ERR_SPACE;
            return false;
        }
    }
    s->win[s->pos++] = b;
    return true;
}

/* ---------- huffman ---------- */

static bool huff_build(Huff* h, const uint8_t* lens, int n) {
    uint16_t offs[MAX_BITS + 2];
    int left = 1;

    memset(h->count, 0, sizeof(h->count));
    memset(h->fast, 0, sizeof(h->fast));
    for(int i = 0; i < n; i++)
        h->count[lens[i]]++;
    for(int len = 1; len <= MAX_BITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if(left < 0) return false; /* over-subscribed */
    }
    offs[1] = 0;
    for(int len = 1; len < MAX_BITS; len++)
        offs[len + 1] = offs[len] + h->count[len];
    for(int i = 0; i < n; i++) {
        if(lens[i]) h->symbol[offs[lens[i]]++] = (uint16_t)i;
    }

    /* fast table: canonical codes, bit-reversed */
    uint32_t code = 0;
    int idx = 0;
    for(int len = 1; len <= MAX_BITS; len++) {
        for(int i = 0; i < h->count[len]; i++, idx++, code++) {
            if(len > FAST_BITS) continue;
            uint32_t rev = 0;
            for(int b = 0; b < len; b++) {
                if(code & (1u << b)) rev |= 1u << (len - 1 - b);
            }
            for(uint32_t j = rev; j < FAST_SIZE; j += 1u << len) {
                h->fast[j] = (uint16_t)(h->symbol[idx] | (len << 9));
            }
        }
        code <<= 1;
    }
    return true;
}

static int huff_sym(Inf* s, const Huff* h) {
    refill(s);
    uint32_t e = h->fast[s->bitbuf & (FAST_SIZE - 1)];
    if(e) {
        int l = (int)(e >> 9);
        if(l > s->bitcnt) {
            if(!s->err) s->err = HUB_INF_ERR_INPUT;
            return -1;
        }
        s->bitbuf >>= l;
        s->bitcnt -= l;
        return (int)(e & 511);
    }
    int code = 0, first = 0, index = 0;
    uint32_t buf = s->bitbuf;
    for(int len = 1; len <= MAX_BITS; len++) {
        if(len > s->bitcnt) {
            if(!s->err) s->err = HUB_INF_ERR_INPUT;
            return -1;
        }
        code |= (int)(buf & 1);
        buf >>= 1;
        int count = h->count[len];
        if(code - count < first) {
            s->bitbuf >>= len;
            s->bitcnt -= len;
            return h->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    if(!s->err) s->err = HUB_INF_ERR_DATA;
    return -1;
}

/* ---------- blocks ---------- */

static void do_stored(Inf* s) {
    uint32_t drop = (uint32_t)s->bitcnt & 7;
    s->bitbuf >>= drop;
    s->bitcnt -= (int)drop;
    uint32_t len = bits(s, 16);
    uint32_t nlen = bits(s, 16);
    if(s->err) return;
    if(len != (~nlen & 0xffff)) {
        s->err = HUB_INF_ERR_DATA;
        return;
    }
    while(len--) {
        uint32_t b = bits(s, 8);
        if(s->err || !put(s, (uint8_t)b)) return;
    }
}

static void do_codes(Inf* s) {
    for(;;) {
        int v = huff_sym(s, &s->lit);
        if(s->err) return;
        if(v < 256) {
            if(!put(s, (uint8_t)v)) return;
        } else if(v == 256) {
            return;
        } else {
            v -= 257;
            if(v >= 29) {
                s->err = HUB_INF_ERR_DATA;
                return;
            }
            uint32_t len = lbase[v] + bits(s, lext[v]);
            int d = huff_sym(s, &s->dist);
            if(s->err) return;
            if(d >= 30) {
                s->err = HUB_INF_ERR_DATA;
                return;
            }
            uint32_t dist = dbase[d] + bits(s, dext[d]);
            if(s->err) return;
            if(dist > s->base + s->pos) {
                s->err = HUB_INF_ERR_DATA;
                return;
            }
            if(dist >
               s->wsize) { /* beyond the ring: the output file has it (read in 512 byte blocks) */
                while(len) {
                    uint64_t from = s->base + s->pos - dist;
                    if(from >= s->base) { /* still in the ring */
                        if(!put(s, s->win[from - s->base])) return;
                        len--;
                        continue;
                    }
                    if(!(s->fc_len && from >= s->fc_pos && from < s->fc_pos + s->fc_len)) {
                        size_t k = s->base - from < sizeof(s->fc) ? (size_t)(s->base - from) :
                                                                    sizeof(s->fc);
                        if(!s->far(s->fctx, from, s->fc, k)) {
                            s->err = HUB_INF_ERR_DATA;
                            return;
                        }
                        s->fc_pos = from;
                        s->fc_len = k;
                    }
                    size_t off = (size_t)(from - s->fc_pos);
                    size_t k = s->fc_len - off < len ? s->fc_len - off : len;
                    for(size_t i = 0; i < k; i++) {
                        if(!put(s, s->fc[off + i])) return;
                    }
                    len -= (uint32_t)k;
                }
                continue;
            }
            size_t src = s->stream ? ((s->pos + s->wsize - dist) & (s->wsize - 1)) : s->pos - dist;
            while(len--) {
                uint8_t b = s->win[src];
                if(!put(s, b)) return;
                src++;
                if(s->stream && src == s->wsize) src = 0;
            }
        }
    }
}

static void do_fixed(Inf* s) {
    int i = 0;
    for(; i < 144; i++)
        s->lens[i] = 8;
    for(; i < 256; i++)
        s->lens[i] = 9;
    for(; i < 280; i++)
        s->lens[i] = 7;
    for(; i < 288; i++)
        s->lens[i] = 8;
    huff_build(&s->lit, s->lens, 288);
    for(i = 0; i < 30; i++)
        s->lens[i] = 5;
    huff_build(&s->dist, s->lens, 30);
    do_codes(s);
}

static void do_dynamic(Inf* s) {
    int nlen = (int)bits(s, 5) + 257;
    int ndist = (int)bits(s, 5) + 1;
    int ncode = (int)bits(s, 4) + 4;
    if(s->err) return;
    if(nlen > 286 || ndist > 30) {
        s->err = HUB_INF_ERR_DATA;
        return;
    }
    memset(s->lens, 0, 19);
    for(int i = 0; i < ncode; i++)
        s->lens[clorder[i]] = (uint8_t)bits(s, 3);
    if(s->err) return;
    /* the dist table holds the code length code for now */
    if(!huff_build(&s->dist, s->lens, 19)) {
        s->err = HUB_INF_ERR_DATA;
        return;
    }
    int total = nlen + ndist;
    int idx = 0;
    uint8_t* L = s->lens;
    /* the code length code is already in s->dist; its 19 lengths are not needed
     * any more, so the real lengths can overwrite s->lens */
    while(idx < total) {
        int sym = huff_sym(s, &s->dist);
        if(s->err) return;
        if(sym < 16) {
            L[idx++] = (uint8_t)sym;
        } else {
            int rep;
            uint8_t val = 0;
            if(sym == 16) {
                if(idx == 0) {
                    s->err = HUB_INF_ERR_DATA;
                    return;
                }
                val = L[idx - 1];
                rep = 3 + (int)bits(s, 2);
            } else if(sym == 17) {
                rep = 3 + (int)bits(s, 3);
            } else {
                rep = 11 + (int)bits(s, 7);
            }
            if(s->err) return;
            if(idx + rep > total) {
                s->err = HUB_INF_ERR_DATA;
                return;
            }
            while(rep--)
                L[idx++] = val;
        }
    }
    if(L[256] == 0) {
        s->err = HUB_INF_ERR_DATA;
        return;
    }
    if(!huff_build(&s->lit, L, nlen) || !huff_build(&s->dist, L + nlen, ndist)) {
        s->err = HUB_INF_ERR_DATA;
        return;
    }
    do_codes(s);
}

/* ---------- driver ---------- */

static void run(Inf* s, uint32_t flags) {
    if(!(flags & HUB_INF_RAW)) {
        uint32_t cmf = bits(s, 8);
        uint32_t flg = bits(s, 8);
        if(s->err) return;
        if((cmf & 15) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20)) {
            s->err = HUB_INF_ERR_DATA;
            return;
        }
        s->wbits = (cmf >> 4) + 8; /* CINFO: the window the writer used */
    }
    if(s->stream) { /* the window is 2^(CINFO+8) bytes (32 KB for raw streams), at most the ring */
        s->wsize = (size_t)1 << (s->wbits ? s->wbits : 15);
        if(s->wsize > HUB_INF_RING) {
            if(!s->far) {
                s->err = HUB_INF_ERR_FAR;
                return;
            }
            s->wsize = HUB_INF_RING;
        }
        s->win = malloc(s->wsize);
        if(!s->win) {
            s->err = HUB_INF_ERR_NOMEM;
            return;
        }
    }
    uint32_t last;
    do {
        last = bits(s, 1);
        uint32_t type = bits(s, 2);
        if(s->err) return;
        if(type == 0) {
            do_stored(s);
        } else if(type == 1) {
            do_fixed(s);
        } else if(type == 2) {
            do_dynamic(s);
        } else {
            s->err = HUB_INF_ERR_DATA;
        }
        if(s->err) return;
    } while(!last);

    if(s->stream) {
        if(!flush(s)) return;
    } else if(flags & HUB_INF_ADLER) {
        adler_update(s, s->win, s->pos);
    }
    if(!(flags & HUB_INF_RAW)) {
        uint32_t drop = (uint32_t)s->bitcnt & 7;
        s->bitbuf >>= drop;
        s->bitcnt -= (int)drop;
        uint32_t a = bits(s, 8) << 24;
        a |= bits(s, 8) << 16;
        a |= bits(s, 8) << 8;
        a |= bits(s, 8);
        if(s->err) return;
        if((flags & HUB_INF_ADLER) && a != ((s->adler_b << 16) | s->adler_a)) {
            s->err = HUB_INF_ERR_ADLER;
        }
    }
}

static int finish(Inf* s, int err, size_t* out_len, size_t* in_used) {
    if(err == STOPPED) err = HUB_INF_OK;
    if(out_len) *out_len = s->base + s->pos;
    if(in_used) *in_used = s->in_total - (size_t)(s->bitcnt / 8);
    return err;
}

static Inf* inf_new(HubInfRead rd, void* rctx) {
    Inf* s = malloc(sizeof(Inf));
    if(!s) return NULL;
    memset(s, 0, offsetof(Inf, lit));
    s->rd = rd;
    s->rctx = rctx;
    s->adler_a = 1;
    return s;
}

int hub_inflate_buf(
    HubInfRead rd,
    void* rctx,
    uint8_t* out,
    size_t cap,
    uint32_t flags,
    size_t* out_len,
    size_t* in_used) {
    Inf* s = inf_new(rd, rctx);
    if(!s) return HUB_INF_ERR_NOMEM;
    s->win = out;
    s->wsize = cap;
    s->partial = (flags & HUB_INF_PARTIAL) != 0;
    run(s, flags);
    int r = finish(s, s->err, out_len, in_used);
    free(s);
    return r;
}

int hub_inflate_stream(
    HubInfRead rd,
    void* rctx,
    HubInfWrite wr,
    void* wctx,
    uint32_t flags,
    size_t* out_len,
    size_t* in_used,
    HubInfFar far,
    void* fctx) {
    Inf* s = inf_new(rd, rctx);
    if(!s) return HUB_INF_ERR_NOMEM;
    s->far = far;
    s->fctx = fctx;
    s->stream = true;
    s->wr = wr;
    s->wctx = wctx;
    run(s, flags);
    int r = finish(s, s->err, out_len, in_used);
    free(s->win);
    free(s);
    return r;
}
