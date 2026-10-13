/* gitobj_test part: inflate tests against zlib. Included by ../gitobj_test.c (one translation unit). */
/* ---------- inflate tests (against zlib) ---------- */

typedef struct {
    const uint8_t* p;
    size_t n, pos;
    unsigned chunk; /* 0 = as much as asked */
    unsigned seed;
} MemIn;

static size_t mem_rd(void* c, uint8_t* buf, size_t max) {
    MemIn* m = c;
    size_t k = m->n - m->pos;
    if(k > max) k = max;
    if(m->chunk) {
        m->seed = m->seed * 1103515245u + 12345u;
        size_t lim = 1 + (m->seed >> 16) % m->chunk;
        if(k > lim) k = lim;
    }
    memcpy(buf, m->p + m->pos, k);
    m->pos += k;
    return k;
}

typedef struct {
    uint8_t* v;
    size_t n, cap;
} Sink;
static bool sink_write(void* c, const uint8_t* p, size_t n) {
    Sink* s = c;
    if(s->n + n > s->cap) {
        s->cap = (s->n + n) * 2;
        s->v = realloc(s->v, s->cap);
    }
    memcpy(s->v + s->n, p, n);
    s->n += n;
    return true;
}
/* far window: the bytes the sink has taken so far */
static unsigned long far_calls;
static bool sink_far(void* c, uint64_t pos, uint8_t* b, size_t n) {
    Sink* s = c;
    far_calls++;
    if(pos + n > s->n) return false;
    memcpy(b, s->v + pos, n);
    return true;
}
static bool sink_far_fail(void* c, uint64_t pos, uint8_t* b, size_t n) {
    (void)c; (void)pos; (void)b; (void)n;
    return false;
}
static bool sink_fail(void* c, const uint8_t* p, size_t n) {
    (void)c;
    (void)p;
    (void)n;
    return false;
}

static uint8_t* make_data(int kind, size_t n, unsigned seed) {
    uint8_t* d = malloc(n + 1);
    unsigned x = seed * 2654435761u + 1;
    for(size_t i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        switch(kind) {
        case 0: d[i] = 0; break;                                  /* zeros */
        case 1: d[i] = (uint8_t)(x >> 24); break;                 /* random */
        case 2: d[i] = "abcdefgh \n"[(x >> 24) % 10]; break;      /* text-ish */
        default: /* repeats with far matches */
            d[i] = (i > 40000 && (x >> 28) != 0) ? d[i - 20000 - ((x >> 12) & 8191)] : (uint8_t)(x >> 24);
        }
    }
    return d;
}

static int zwbits = 15;
static uint8_t* zdeflate(const uint8_t* d, size_t n, int level, int strategy, bool raw, size_t* outn) {
    z_stream z;
    memset(&z, 0, sizeof z);
    deflateInit2(&z, level, Z_DEFLATED, raw ? -zwbits : zwbits, 8, strategy);
    size_t cap = deflateBound(&z, n) + 64;
    uint8_t* o = malloc(cap);
    z.next_in = (Bytef*)d;
    z.avail_in = (uInt)n;
    z.next_out = o;
    z.avail_out = (uInt)cap;
    int r = deflate(&z, Z_FINISH);
    if(r != Z_STREAM_END) {
        fprintf(stderr, "zlib deflate failed\n");
        exit(2);
    }
    *outn = z.total_out;
    deflateEnd(&z);
    return o;
}


/* Ring window (8 KB) and far reads: back-references beyond the ring at every
 * distance around the ring size, with match lengths 3..258 so that copies start
 * at every offset relative to the ring flush. */
static uint8_t* make_far_data(size_t n, unsigned seed, size_t dist) {
    uint8_t* d = malloc(n);
    unsigned x = seed * 2654435761u + 7;
    size_t i = 0;
    while(i < n) {
        x = x * 1664525u + 1013904223u;
        if(i > dist + 10 && ((x >> 24) & 3) != 0) { /* a copy of 3..258 bytes from dist back */
            size_t len = 3 + (x >> 8) % 256;
            for(size_t k = 0; k < len && i < n; k++, i++) d[i] = d[i - dist];
        } else {
            size_t len = 1 + (x >> 8) % 20;
            for(size_t k = 0; k < len && i < n; k++, i++) {
                x = x * 1664525u + 1013904223u;
                d[i] = (uint8_t)(x >> 24);
            }
        }
    }
    return d;
}

static int test_far(void) {
    static const size_t dists[] = {100, 8191, 8192, 8193, 8200, 9000, 12000, 16384, 20000, 32000, 32767};
    static const int wbits[] = {15, 14, 13, 12, 9};
    unsigned cases = 0;
    n_checks = n_fail = 0;
    for(unsigned wi = 0; wi < 5; wi++)
        for(unsigned di = 0; di < sizeof dists / sizeof *dists; di++)
            for(unsigned seed = 0; seed < 6; seed++)
                for(int level = 1; level <= 9; level += 4) {
                    size_t dist = dists[di], n = 120000 + seed * 977;
                    if(dist >= ((size_t)1 << wbits[wi])) continue; /* zlib cannot reach that far */
                    uint8_t* d = make_far_data(n, seed + 1, dist);
                    zwbits = wbits[wi];
                    size_t zn;
                    uint8_t* z = zdeflate(d, n, level, Z_DEFAULT_STRATEGY, false, &zn);
                    cases++;
                    for(unsigned chunk = 0; chunk < 3; chunk++) {
                        unsigned ck = chunk == 0 ? 0 : (chunk == 1 ? 1 : 700);
                        Sink sk = {0};
                        MemIn in = {z, zn, 0, ck, 3};
                        size_t olen = 0, used = 0;
                        unsigned long before = far_calls;
                        int r = hub_inflate_stream(mem_rd, &in, sink_write, &sk, HUB_INF_ADLER, &olen, &used, sink_far, &sk);
                        CHECK(r == 0 && olen == n && sk.n == n && used == zn && memcmp(sk.v, d, n) == 0,
                              "far wbits=%d dist=%zu seed=%u level=%d ck=%u r=%d", wbits[wi], dist, seed, level, ck, r);
                        if(wbits[wi] > 13 && dist > 8192 && level > 1 && dist + 262 < ((size_t)1 << wbits[wi]))
                            CHECK(far_calls > before, "no far read for dist %zu wbits %d", dist, wbits[wi]);
                        free(sk.v);
                    }
                    /* without a far callback: a window over the ring is refused before any output */
                    {
                        Sink sk = {0};
                        MemIn in = {z, zn, 0, 0, 0};
                        int r = hub_inflate_stream(mem_rd, &in, sink_write, &sk, HUB_INF_ADLER, NULL, NULL, NULL, NULL);
                        if(wbits[wi] > 13) {
                            CHECK(r == HUB_INF_ERR_FAR && sk.n == 0, "no far: r=%d out=%zu wbits=%d", r, sk.n, wbits[wi]);
                        } else {
                            CHECK(r == 0 && sk.n == n && memcmp(sk.v, d, n) == 0, "small window without far r=%d wbits=%d", r, wbits[wi]);
                        }
                        free(sk.v);
                    }
                    /* a far read that fails is an error, not garbage */
                    if(wbits[wi] > 13 && dist > 8192 && level > 1 && dist + 262 < ((size_t)1 << wbits[wi])) {
                        Sink sk = {0};
                        MemIn in = {z, zn, 0, 0, 0};
                        int r = hub_inflate_stream(mem_rd, &in, sink_write, &sk, HUB_INF_ADLER, NULL, NULL, sink_far_fail, NULL);
                        CHECK(r != 0, "failing far read accepted");
                        free(sk.v);
                    }
                    free(z);
                    free(d);
                }
    zwbits = 15;
    CHECK(heap_cur == 0, "far test leaked %zu bytes", heap_cur);
    printf("far window: %u streams, far reads %lu, checks=%u fail=%u\n", cases, far_calls, n_checks, n_fail);
    return n_fail != 0;
}

static int test_inflate(void) {
    static const size_t sizes[] = {0, 1, 2, 100, 1000, 40000, 70000, 300000};
    static const int levels[] = {0, 1, 6, 9};
    static const int strategies[] = {Z_DEFAULT_STRATEGY, Z_FIXED, Z_HUFFMAN_ONLY, Z_RLE};
    size_t peak_buf = 0, peak_stream = 0;
    unsigned cases = 0;

    for(unsigned si = 0; si < sizeof sizes / sizeof *sizes; si++)
        for(int kind = 0; kind < 4; kind++)
            for(unsigned li = 0; li < 4; li++)
                for(unsigned st = 0; st < 4; st++) {
                    size_t n = sizes[si];
                    uint8_t* d = make_data(kind, n, (unsigned)(si * 100 + kind * 10 + li));
                    for(int raw = 0; raw < 2; raw++) {
                        size_t zn;
                        uint8_t* z = zdeflate(d, n, levels[li], strategies[st], raw, &zn);
                        uint8_t* zg = malloc(zn + 33);
                        memcpy(zg, z, zn);
                        memset(zg + zn, 0xA5, 33); /* garbage after the stream */
                        uint32_t fl = raw ? HUB_INF_RAW : HUB_INF_ADLER;
                        cases++;

                        for(unsigned chunk = 0; chunk < 3; chunk++) {
                            unsigned ck = chunk == 0 ? 0 : (chunk == 1 ? 1 : 700);
                            /* (a) buffer */
                            uint8_t* out = malloc(n + 1);
                            MemIn in = {zg, zn + 33, 0, ck, 5};
                            size_t olen = 0, used = 0;
                            heap_mark();
                            int r = hub_inflate_buf(mem_rd, &in, out, n, fl, &olen, &used);
                            if(heap_used() > peak_buf) peak_buf = heap_used();
                            CHECK(r == 0 && olen == n && used == zn && memcmp(out, d, n) == 0,
                                  "buf s=%zu k=%d l=%d st=%d raw=%d ck=%u r=%d olen=%zu used=%zu/%zu",
                                  n, kind, levels[li], strategies[st], raw, ck, r, olen, used, zn);
                            free(out);
                            /* (b) stream */
                            Sink sk = {0};
                            MemIn in2 = {zg, zn + 33, 0, ck, 9};
                            heap_mark();
                            r = hub_inflate_stream(mem_rd, &in2, sink_write, &sk, fl, &olen, &used, sink_far, &sk);
                            if(heap_used() > peak_stream) peak_stream = heap_used();
                            CHECK(r == 0 && olen == n && sk.n == n && used == zn &&
                                      (n == 0 || memcmp(sk.v, d, n) == 0),
                                  "stream s=%zu k=%d l=%d st=%d raw=%d ck=%u r=%d", n, kind,
                                  levels[li], strategies[st], raw, ck, r);
                            free(sk.v);
                        }
                        if(n > 0) {
                            /* too small a buffer */
                            uint8_t* out = malloc(n);
                            MemIn in = {z, zn, 0, 0, 0};
                            int r = hub_inflate_buf(mem_rd, &in, out, n - 1, fl, NULL, NULL);
                            CHECK(r == HUB_INF_ERR_SPACE, "space check r=%d", r);
                            /* partial */
                            size_t part = n / 2 + 1, got = 0;
                            MemIn in2 = {z, zn, 0, 0, 0};
                            r = hub_inflate_buf(mem_rd, &in2, out, part, fl | HUB_INF_PARTIAL, &got, NULL);
                            CHECK(r == 0 && got == part && memcmp(out, d, part) == 0,
                                  "partial r=%d got=%zu/%zu", r, got, part);
                            free(out);
                            /* write failure */
                            MemIn in3 = {z, zn, 0, 0, 0};
                            r = hub_inflate_stream(mem_rd, &in3, sink_fail, NULL, fl, NULL, NULL, sink_far, &(Sink){0});
                            CHECK(r == HUB_INF_ERR_WRITE, "write fail r=%d", r);
                        }
                        if(!raw && zn > 6) {
                            /* wrong checksum */
                            z[zn - 1] ^= 0x55;
                            uint8_t* out = malloc(n + 1);
                            MemIn in = {z, zn, 0, 0, 0};
                            int r = hub_inflate_buf(mem_rd, &in, out, n, HUB_INF_ADLER, NULL, NULL);
                            CHECK(r == HUB_INF_ERR_ADLER, "adler r=%d", r);
                            in = (MemIn){z, zn, 0, 0, 0};
                            r = hub_inflate_buf(mem_rd, &in, out, n, 0, NULL, NULL);
                            CHECK(r == 0, "no adler check r=%d", r);
                            z[zn - 1] ^= 0x55;
                            free(out);
                        }
                        /* truncation and bit flips: no crash, error or (rarely) ok */
                        if(n <= 40000 && st == 0 && li == 2) {
                            uint8_t* out = malloc(n + 1);
                            for(unsigned t = 0; t < 12; t++) {
                                size_t cut = zn ? (zn * (t + 1)) / 14 : 0;
                                MemIn in = {z, cut, 0, 0, 0};
                                int r = hub_inflate_buf(mem_rd, &in, out, n, fl, NULL, NULL);
                                CHECK(cut >= zn || r != 0, "truncated stream accepted cut=%zu/%zu", cut, zn);
                            }
                            for(unsigned t = 0; t < 40 && zn; t++) {
                                uint8_t* y = malloc(zn);
                                memcpy(y, z, zn);
                                y[(t * 7919u) % zn] ^= (uint8_t)(1u << (t & 7));
                                MemIn in = {y, zn, 0, 0, 0};
                                Sink sk = {0};
                                hub_inflate_stream(mem_rd, &in, sink_write, &sk, fl, NULL, NULL, sink_far, &sk);
                                free(sk.v);
                                in = (MemIn){y, zn, 0, 0, 0};
                                hub_inflate_buf(mem_rd, &in, out, n, fl, NULL, NULL);
                                free(y);
                            }
                            free(out);
                        }
                        free(z);
                        free(zg);
                    }
                    free(d);
                }
    /* concatenated streams: in_used lets the caller find the next one */
    {
        size_t za, zb;
        uint8_t* a = make_data(2, 5000, 1);
        uint8_t* b = make_data(1, 3000, 2);
        uint8_t* ca = zdeflate(a, 5000, 6, 0, false, &za);
        uint8_t* cb = zdeflate(b, 3000, 6, 0, false, &zb);
        uint8_t* cat = malloc(za + zb);
        memcpy(cat, ca, za);
        memcpy(cat + za, cb, zb);
        uint8_t* out = malloc(5000);
        MemIn in = {cat, za + zb, 0, 0, 0};
        size_t used = 0;
        int r = hub_inflate_buf(mem_rd, &in, out, 5000, HUB_INF_ADLER, NULL, &used);
        CHECK(r == 0 && used == za, "concat first: r=%d used=%zu za=%zu", r, used, za);
        MemIn in2 = {cat + used, zb, 0, 0, 0};
        r = hub_inflate_buf(mem_rd, &in2, out, 3000, HUB_INF_ADLER, NULL, &used);
        CHECK(r == 0 && used == zb && memcmp(out, b, 3000) == 0, "concat second r=%d", r);
        free(a); free(b); free(ca); free(cb); free(cat); free(out);
    }
    /* malloc failure */
    {
        uint8_t* d = make_data(2, 1000, 3);
        size_t zn;
        uint8_t* z = zdeflate(d, 1000, 6, 0, false, &zn);
        uint8_t out[1000];
        for(size_t k = 0; k < 3; k++) {
            heap_fail_after = heap_calls + k;
            MemIn in = {z, zn, 0, 0, 0};
            int r = hub_inflate_buf(mem_rd, &in, out, sizeof out, 0, NULL, NULL);
            CHECK((k == 0) == (r == HUB_INF_ERR_NOMEM), "nomem buf k=%zu r=%d", k, r);
            heap_fail_after = heap_calls + k;
            MemIn in2 = {z, zn, 0, 0, 0};
            Sink sk = {0};
            r = hub_inflate_stream(mem_rd, &in2, sink_write, &sk, 0, NULL, NULL, sink_far, &sk);
            CHECK((k < 2) == (r == HUB_INF_ERR_NOMEM), "nomem stream k=%zu r=%d", k, r);
            free(sk.v);
        }
        heap_fail_after = (size_t)-1;
        free(d); free(z);
    }
    CHECK(heap_cur == 0, "inflate leaked %zu bytes", heap_cur);
    printf("inflate: %u stream cases, checks=%u fail=%u\n", cases, n_checks, n_fail);
    printf("inflate peak heap: buffer mode %zu bytes, stream mode %zu bytes\n", peak_buf, peak_stream);
    return n_fail != 0;
}

