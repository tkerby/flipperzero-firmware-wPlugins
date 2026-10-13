/* Host test harness for flipper_app/hub_inflate.c and hub_gitobj.c.
 *
 *   gitobj_test --inflate
 *   gitobj_test <repo.git> [--limit N]       full comparison against git
 *   gitobj_test <repo.git> --fuzz            corrupt a COPY of the repo, no crash expected
 *
 * Everything in the library is compared with the real git. malloc of the library
 * code is wrapped (malloc_wrap.h) to measure the peak heap per operation. */
#define _GNU_SOURCE
#include "../../flipper_app/hub_gitobj.h"
#include "../../flipper_app/hub_inflate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ---------- heap accounting (library code only) ---------- */

static size_t heap_cur, heap_peak, heap_base, heap_fail_after = (size_t)-1, heap_calls;

typedef struct {
    size_t size;
    size_t pad;
} MHdr;

void* t_malloc(size_t n) {
    if(++heap_calls > heap_fail_after) return NULL;
    MHdr* h = malloc(sizeof(MHdr) + n);
    if(!h) return NULL;
    h->size = n;
    heap_cur += n;
    if(heap_cur > heap_peak) heap_peak = heap_cur;
    return h + 1;
}
void* t_calloc(size_t a, size_t b) {
    void* p = t_malloc(a * b);
    if(p) memset(p, 0, a * b);
    return p;
}
void t_free(void* p) {
    if(!p) return;
    MHdr* h = (MHdr*)p - 1;
    heap_cur -= h->size;
    free(h);
}
void* t_realloc(void* p, size_t n) {
    if(!p) return t_malloc(n);
    MHdr* h = (MHdr*)p - 1;
    void* q = t_malloc(n);
    if(!q) return NULL;
    memcpy(q, p, h->size < n ? h->size : n);
    t_free(p);
    return q;
}
static void heap_mark(void) {
    heap_base = heap_cur;
    heap_peak = heap_cur;
}
static size_t heap_used(void) {
    return heap_peak - heap_base;
}

/* ---------- check helpers ---------- */

static unsigned n_checks, n_fail;
static void fail(const char* fmt, ...) {
    n_fail++;
    if(n_fail <= 25) {
        va_list ap;
        va_start(ap, fmt);
        fprintf(stderr, "FAIL: ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        va_end(ap);
    }
}
#define CHECK(cond, ...) \
    do { \
        n_checks++; \
        if(!(cond)) fail(__VA_ARGS__); \
    } while(0)

static const char* errname(int e) {
    switch(e) {
    case HUB_GIT_OK: return "OK";
    case HUB_GIT_ERR_NOTFOUND: return "NOTFOUND";
    case HUB_GIT_ERR_TOOLARGE: return "TOOLARGE";
    case HUB_GIT_ERR_CORRUPT: return "CORRUPT";
    case HUB_GIT_ERR_NOMEM: return "NOMEM";
    case HUB_GIT_ERR_IO: return "IO";
    case HUB_GIT_ERR_BADARG: return "BADARG";
    case HUB_GIT_ERR_WRITE: return "WRITE";
    }
    return "?";
}

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

/* ---------- git helpers ---------- */

static char repo[512];

static FILE* gitp(const char* fmt, ...) {
    char cmd[1024], full[1600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    snprintf(full, sizeof full, "git --git-dir='%s' %s", repo, cmd);
    FILE* p = popen(full, "r");
    if(!p) {
        perror("popen");
        exit(2);
    }
    return p;
}

static char* slurp(FILE* f, size_t* n) {
    size_t cap = 4096, len = 0;
    char* b = malloc(cap);
    size_t k;
    while((k = fread(b + len, 1, cap - len - 1, f)) > 0) {
        len += k;
        if(len + 1 >= cap) {
            cap *= 2;
            b = realloc(b, cap);
        }
    }
    b[len] = 0;
    pclose(f);
    if(n) *n = len;
    return b;
}

static char* git_out(const char* fmt, ...) { /* single arg string only */
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    return slurp(gitp("%s", cmd), NULL);
}

static void hexstr(const HubOid* o, char* out) { hub_oid_to_hex(o, out); }

/* ---------- object matrix ---------- */

typedef struct {
    char name[32];
    size_t peak, at_size, over; /* peak heap, size of that object, peak minus size */
    unsigned n;
} HeapClass;
static HeapClass classes[32];
static unsigned nclasses;

static void heap_note(const char* name, size_t peak, size_t objsize) {
    HeapClass* c = NULL;
    for(unsigned i = 0; i < nclasses; i++)
        if(strcmp(classes[i].name, name) == 0) c = &classes[i];
    if(!c) {
        c = &classes[nclasses++];
        snprintf(c->name, sizeof c->name, "%s", name);
    }
    c->n++;
    if(peak > c->peak) {
        c->peak = peak;
        c->at_size = objsize;
        c->over = peak > objsize ? peak - objsize : 0;
    }
}

static const char* tname(int t) {
    return t == HUB_OBJ_COMMIT ? "commit" : t == HUB_OBJ_TREE ? "tree" : t == HUB_OBJ_BLOB ? "blob" : "tag";
}
static int tnum(const char* s) {
    return !strcmp(s, "commit") ? 1 : !strcmp(s, "tree") ? 2 : !strcmp(s, "blob") ? 3 : 4;
}

typedef struct {
    const uint8_t* want;
    size_t size, pos;
    bool bad;
    unsigned calls;
} StreamChk;
static bool chk_sink(void* c, const uint8_t* p, size_t n) {
    StreamChk* s = c;
    s->calls++;
    if(s->pos + n > s->size || memcmp(s->want + s->pos, p, n) != 0) s->bad = true;
    s->pos += n;
    return true;
}

typedef struct {
    HubOid oid;
    char hex[41];
    int type;
    size_t size;
    bool delta;
} ObjRow;

extern long gitio_fault;
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned tmp_count(void) {
    char p[700];
    snprintf(p, sizeof p, "%s/hub/tmp", repo);
    DIR* d = opendir(p);
    if(!d) return 0;
    unsigned n = 0;
    struct dirent* e;
    while((e = readdir(d)))
        if(strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
    closedir(d);
    return n;
}
static unsigned long cache_total(unsigned* files) {
    char p[700];
    snprintf(p, sizeof p, "%s/hub/cache", repo);
    DIR* d = opendir(p);
    unsigned long tot = 0;
    if(files) *files = 0;
    if(!d) return 0;
    struct dirent* e;
    while((e = readdir(d))) {
        if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char f[1000];
        struct stat st;
        snprintf(f, sizeof f, "%s/%s", p, e->d_name);
        if(stat(f, &st) == 0) tot += (unsigned long)st.st_size;
        if(files) (*files)++;
    }
    closedir(d);
    return tot;
}
static long cache_file_size(const char* hex) {
    char f[800];
    struct stat st;
    snprintf(f, sizeof f, "%s/hub/cache/%s", repo, hex);
    return stat(f, &st) == 0 ? (long)st.st_size : -1;
}
static void cache_rm(const char* hex) {
    char f[800];
    snprintf(f, sizeof f, "%s/hub/cache/%s", repo, hex);
    unlink(f);
}

typedef struct {
    HubOid oid;
    char hex[41];
    int type;
    size_t size;
    uint8_t* want; /* only for the first few */
} Spilled;
static Spilled spilled[64];
static unsigned nspilled, st_spill, st_cachehit;

static unsigned st_ok, st_toolarge, st_inter, st_stream_big, st_delta;
static size_t max_read_peak;

static bool nullsink(void* c, const uint8_t* p, size_t n) {
    (void)c; (void)p; (void)n;
    return true;
}

static void test_spill_extra(HubGit* g, size_t limit) {
    (void)limit;
    unsigned nfault_runs = 0, nfault_err = 0, nmem_runs = 0;
    /* 1. injected I/O errors during the spill: tmp empty, no poisoned cache */
    for(unsigned s = 0; s < nspilled && s < 2; s++) {
        Spilled* sp = &spilled[s];
        if(!sp->want) continue;
        for(long k = 0;; k += (k < 120 ? 1 : 11)) {
            cache_rm(sp->hex);
            Sink sk = {0};
            extern long gitio_fault_hits;
            gitio_fault_hits = 0;
            gitio_fault = k;
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &sp->oid, sink_write, &sk, &t, &sz);
            gitio_fault = -1;
            bool hit = gitio_fault_hits > 0;
            nfault_runs++;
            if(e) nfault_err++;
            CHECK(tmp_count() == 0, "tmp not empty after fault k=%ld (%s)", k, errname(e));
            CHECK(e == 0 || e == HUB_GIT_ERR_IO || e == HUB_GIT_ERR_CORRUPT || e == HUB_GIT_ERR_NOTFOUND,
                  "fault k=%ld unexpected %s", k, errname(e));
            if(e == 0) CHECK(sk.n == sp->size && !memcmp(sk.v, sp->want, sp->size), "fault k=%ld OK but wrong data", k);
            free(sk.v);
            /* a clean run afterwards must give the right bytes (cache not poisoned) */
            Sink s2 = {0};
            e = hub_git_stream(g, &sp->oid, sink_write, &s2, &t, &sz);
            CHECK(e == 0 && s2.n == sp->size && !memcmp(s2.v, sp->want, sp->size), "after fault k=%ld: clean run %s", k, errname(e));
            free(s2.v);
            if(!hit) break;
        }
    }
    /* 2. out of memory at every allocation of the spill path */
    if(nspilled) {
        Spilled* sp = &spilled[0];
        for(size_t k = 0; k < 40; k++) {
            cache_rm(sp->hex);
            size_t before = heap_cur;
            heap_fail_after = heap_calls + k;
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &sp->oid, nullsink, NULL, &t, &sz);
            heap_fail_after = (size_t)-1;
            nmem_runs++;
            CHECK(e == 0 || e == HUB_GIT_ERR_NOMEM, "spill nomem k=%zu got %s", k, errname(e));
            CHECK(tmp_count() == 0, "tmp not empty after nomem k=%zu", k);
            CHECK(heap_cur == before, "spill nomem k=%zu leaked %zd", k, (ssize_t)(heap_cur - before));
        }
    }
    /* 3. cache cap */
    {
        const uint64_t cap = 300000;
        hub_git_set_cache_limit(g, cap);
        char cmd[800];
        snprintf(cmd, sizeof cmd, "rm -rf '%s/hub/cache'", repo);
        if(system(cmd)) {}
        unsigned bad = 0, maxfiles = 0;
        for(unsigned s = 0; s < nspilled; s++) {
            uint8_t t;
            size_t sz;
            int e = hub_git_stream(g, &spilled[s].oid, nullsink, NULL, &t, &sz);
            CHECK(e == 0, "cap run stream %s", errname(e));
            unsigned files;
            unsigned long tot = cache_total(&files);
            if(files > maxfiles) maxfiles = files;
            if(tot > cap && files > 1) bad++;
        }
        CHECK(bad == 0, "cache folder exceeded the cap %u times", bad);
        printf("cache cap: %u streams with cap %llu, max %u files kept, final %lu bytes\n", nspilled,
               (unsigned long long)cap, maxfiles, cache_total(NULL));
        hub_git_set_cache_limit(g, HUB_GIT_CACHE_MAX);
    }
    for(unsigned s = 0; s < nspilled; s++) free(spilled[s].want), spilled[s].want = NULL;
    printf("spill robustness: %u fault-injection runs (%u ended with an error, tmp always empty), %u out-of-memory runs\n",
           nfault_runs, nfault_err, nmem_runs);
}

static void test_matrix(HubGit* g, size_t limit) {
    {
        char cmd[800];
        snprintf(cmd, sizeof cmd, "rm -rf '%s/hub'", repo);
        if(system(cmd)) {}
        nspilled = st_spill = st_cachehit = 0;
    }
    FILE* lp = gitp("cat-file --batch-all-objects --batch-check='%%(objectname) %%(objecttype) %%(objectsize) %%(deltabase)'");
    size_t ln;
    char* list = slurp(lp, &ln);
    unsigned nrows = 0;
    for(char* p = list; *p; p++) nrows += (*p == '\n');
    ObjRow* rows = calloc(nrows, sizeof(ObjRow));
    unsigned i = 0;
    for(char* line = strtok(list, "\n"); line && i < nrows; line = strtok(NULL, "\n"), i++) {
        char type[16], base[64];
        unsigned long sz;
        if(sscanf(line, "%40s %15s %lu %63s", rows[i].hex, type, &sz, base) < 3) {
            fail("bad batch-check line '%s'", line);
            continue;
        }
        base[40] = 0;
        rows[i].delta = strcmp(base, "0000000000000000000000000000000000000000") != 0 && base[0];
        rows[i].type = tnum(type);
        rows[i].size = sz;
        hub_oid_from_hex(rows[i].hex, &rows[i].oid);
    }
    nrows = i;

    FILE* bp = gitp("cat-file --batch-all-objects --batch");
    heap_mark();
    size_t base_heap = heap_cur;
    (void)base_heap;
    for(i = 0; i < nrows; i++) {
        ObjRow* r = &rows[i];
        char hdr[256];
        if(!fgets(hdr, sizeof hdr, bp)) {
            fail("git cat-file --batch ended early at %u", i);
            break;
        }
        uint8_t* want = malloc(r->size + 2);
        if(fread(want, 1, r->size + 1, bp) != r->size + 1) fail("short read from git for %s", r->hex);
        if(strncmp(hdr, r->hex, 40) != 0) {
            fail("order mismatch %s vs %s", hdr, r->hex);
            free(want);
            break;
        }

        /* info */
        uint8_t t = 0;
        size_t sz = 0;
        heap_mark();
        int e = hub_git_info(g, &r->oid, &t, &sz);
        heap_note("info", heap_used(), 0);
        CHECK(e == 0 && t == r->type && sz == r->size, "info %s: e=%s type %d/%d size %zu/%zu", r->hex,
              errname(e), t, r->type, sz, r->size);

        /* read */
        uint8_t* data = NULL;
        size_t rsz = 0;
        uint8_t rt = 0;
        heap_mark();
        e = hub_git_read(g, &r->oid, &rt, &data, &rsz);
        size_t pk = heap_used();
        char cls[32];
        snprintf(cls, sizeof cls, "read.%s.%s", tname(r->type), r->delta ? "delta" : "plain");
        if(e == 0) {
            heap_note(cls, pk, rsz);
            if(pk > max_read_peak) max_read_peak = pk;
            CHECK(rt == r->type && rsz == r->size && memcmp(data, want, r->size) == 0 && data[rsz] == 0,
                  "read %s content/type mismatch (type %d/%d size %zu/%zu)", r->hex, rt, r->type, rsz, r->size);
            hub_git_free(data);
            st_ok++;
        } else if(e == HUB_GIT_ERR_TOOLARGE) {
            st_toolarge++;
            CHECK(r->size > limit || r->delta, "read %s: TOOLARGE but size %zu <= limit %zu and not delta", r->hex, r->size, limit);
            if(r->size <= limit) st_inter++;
            if(pk) heap_note("read.toolarge", pk, 0);
        } else {
            fail("read %s: %s (size %zu delta %d)", r->hex, errname(e), r->size, r->delta);
        }
        int read_e = e;
        if(r->size > limit && !r->delta) CHECK(e == HUB_GIT_ERR_TOOLARGE, "read %s: expected TOOLARGE got %s", r->hex, errname(e));
        if(r->delta) st_delta++;

        /* stream: every object must work now, whatever its size */
        bool needs_spill = r->delta && (r->size > limit || read_e == HUB_GIT_ERR_TOOLARGE);
        for(int pass = 0; pass < (needs_spill ? 2 : 1); pass++) {
            StreamChk sc = {want, r->size, 0, false, 0};
            heap_mark();
            e = hub_git_stream(g, &r->oid, chk_sink, &sc, &rt, &rsz);
            pk = heap_used();
            if(needs_spill)
                snprintf(cls, sizeof cls, "%s", pass == 0 ? "stream.spill" : "stream.cachehit");
            else
                snprintf(cls, sizeof cls, "stream.%s", r->delta ? "delta" : (r->size > limit ? "plain.big" : "plain"));
            CHECK(e == 0, "stream %s (pass %d): %s", r->hex, pass, errname(e));
            if(e == 0) {
                heap_note(cls, pk, r->size);
                CHECK(sc.pos == r->size && !sc.bad && rt == r->type && rsz == r->size,
                      "stream %s mismatch pos=%zu bad=%d", r->hex, sc.pos, sc.bad);
                if(r->size > limit && !r->delta) st_stream_big++;
            }
            CHECK(tmp_count() == 0, "tmp not empty after stream of %s", r->hex);
            if(needs_spill) {
                if(pass == 0) st_spill++;
                else st_cachehit++;
                long cs = cache_file_size(r->hex);
                CHECK(cs == (long)r->size, "cache file of %s has size %ld, want %zu", r->hex, cs, r->size);
                if(pass == 0 && nspilled < 64) {
                    Spilled* sp = &spilled[nspilled++];
                    sp->oid = r->oid;
                    memcpy(sp->hex, r->hex, 41);
                    sp->type = r->type;
                    sp->size = r->size;
                    sp->want = nspilled <= 3 ? memcpy(malloc(r->size + 1), want, r->size) : NULL;
                }
            }
        }
        free(want);
    }
    pclose(bp);
    printf("objects: %u  read ok=%u  toolarge=%u (intermediate-too-large %u)  deltas=%u  big plain streamed=%u\n",
           nrows, st_ok, st_toolarge, st_inter, st_delta, st_stream_big);
    printf("spill: %u big delta objects streamed via temp files, %u cache hits re-streamed identical; stream failures: none expected\n",
           st_spill, st_cachehit);
    free(rows);
    free(list);
    test_spill_extra(g, limit);
}

/* ---------- trees, commits, refs ---------- */

typedef struct {
    char* v;
    size_t n, cap;
} Str;
static void sadd(Str* s, const char* p, size_t n) {
    if(s->n + n + 1 > s->cap) {
        s->cap = (s->n + n + 1) * 2;
        s->v = realloc(s->v, s->cap);
    }
    memcpy(s->v + s->n, p, n);
    s->n += n;
    s->v[s->n] = 0;
}

/* Recursive in the harness only: the library iterates without recursion. */
static void walk_tree(HubGit* g, const HubOid* tree, const char* prefix, Str* out) {
    uint8_t type;
    uint8_t* data;
    size_t size;
    int e = hub_git_read(g, tree, &type, &data, &size);
    if(e) {
        fail("walk_tree read: %s", errname(e));
        return;
    }
    HubTreeIter it;
    HubTreeEntry te;
    hub_tree_iter_init(&it, data, size);
    while((e = hub_tree_next(&it, &te)) > 0) {
        char hex[41], line[1500], path[1100];
        hexstr(te.oid, hex);
        snprintf(path, sizeof path, "%s%s%.*s", prefix, prefix[0] ? "/" : "", (int)te.name_len, te.name);
        const char* ty = hub_tree_mode_type(te.mode);
        if(!strcmp(ty, "link")) ty = "blob";
        int n = snprintf(line, sizeof line, "%06o %s %s\t%s", te.mode, ty, hex, path);
        sadd(out, line, (size_t)n + 1); /* include NUL like ls-tree -z */
        if(te.mode == 040000) walk_tree(g, te.oid, path, out);
    }
    CHECK(e == 0, "tree iteration error %d", e);
    hub_git_free(data);
}

static void test_trees(HubGit* g) {
    char* revs = git_out("rev-list --all");
    unsigned idx = 0, tested = 0;
    HubOid root_head = {{0}};
    for(char* c = strtok(revs, "\n"); c; c = strtok(NULL, "\n"), idx++) {
        if(idx % 23 != 0 && idx != 1) continue;
        HubOid co, tree;
        hub_oid_from_hex(c, &co);
        int e = hub_git_tree_of(g, &co, &tree);
        CHECK(e == 0, "tree_of %s: %s", c, errname(e));
        if(e) continue;
        char expect_tree[64];
        char* t = git_out("rev-parse %s^{tree}", c);
        t[40] = 0;
        hexstr(&tree, expect_tree);
        CHECK(strcmp(t, expect_tree) == 0, "tree_of mismatch %s", c);
        free(t);

        Str mine = {0};
        heap_mark();
        walk_tree(g, &tree, "", &mine);
        size_t exp_n;
        char* exp = slurp(gitp("ls-tree -r -t -z %s", c), &exp_n);
        CHECK(mine.n == exp_n && memcmp(mine.v, exp, exp_n) == 0, "ls-tree -r -t mismatch for commit %s (mine %zu, git %zu)", c, mine.n, exp_n);
        if(idx == 0) {
            root_head = tree;
            /* tree_find for every path */
            for(char* p = exp; p < exp + exp_n;) {
                char mode[8], ty[16], hex[48];
                char* tab = strchr(p, '\t');
                sscanf(p, "%7s %15s %47s", mode, ty, hex);
                const char* path = tab + 1;
                HubOid o, want;
                uint32_t m = 0;
                hub_oid_from_hex(hex, &want);
                heap_mark();
                e = hub_git_tree_find(g, &root_head, path, &o, &m);
                heap_note("tree_find(deep)", heap_used(), 0);
                CHECK(e == 0 && memcmp(&o, &want, 20) == 0 && m == strtoul(mode, NULL, 8), "tree_find '%s': %s", path, errname(e));
                p = (char*)path + strlen(path) + 1;
            }
            HubOid o;
            uint32_t m;
            CHECK(hub_git_tree_find(g, &root_head, "", &o, &m) == 0 && !memcmp(&o, &root_head, 20) && m == 040000, "tree_find root");
            CHECK(hub_git_tree_find(g, &root_head, "nope", &o, &m) == HUB_GIT_ERR_NOTFOUND, "tree_find missing");
            CHECK(hub_git_tree_find(g, &root_head, "README.md/x", &o, &m) == HUB_GIT_ERR_NOTFOUND, "tree_find through blob");
            CHECK(hub_git_tree_find(g, &root_head, "src/../x", &o, &m) == HUB_GIT_ERR_BADARG, "tree_find ..");
            CHECK(hub_git_tree_find(g, &root_head, "/src//core/", &o, &m) == 0 && m == 040000, "tree_find odd slashes");
        }
        free(exp);
        free(mine.v);
        tested++;
    }
    free(revs);
    printf("trees: %u commits' full trees compared with git ls-tree -r -t\n", tested);
}

static void trim_nl(const char* s, size_t* n) {
    while(*n > 0 && (s[*n - 1] == '\n' || s[*n - 1] == ' ')) (*n)--;
}

static void test_commits(HubGit* g) {
    size_t n;
    char* log = slurp(gitp("log --all --format='%%H%%x01%%T%%x01%%P%%x01%%an%%x01%%ae%%x01%%at%%x01%%cn%%x01%%ce%%x01%%ct%%x01%%B%%x02' --date=raw"), &n);
    char* tzs = git_out("log --all --format='%%ad%%x01%%cd' --date=format:%%z");
    unsigned cnt = 0, merges = 0, big_merge = 0;
    char* tzp = tzs;
    for(char* rec = strtok(log, "\x02"); rec; rec = strtok(NULL, "\x02")) {
        while(*rec == '\n') rec++;
        if(!*rec) break;
        char* f[10];
        char* p = rec;
        for(int k = 0; k < 10; k++) {
            f[k] = p;
            if(k < 9) {
                p = strchr(p, 1);
                if(!p) {
                    fail("log format");
                    return;
                }
                *p++ = 0;
            }
        }
        char* tz_a = tzp;
        char* tz_c = strchr(tz_a, 1);
        *tz_c++ = 0;
        char* nl = strchr(tz_c, '\n');
        *nl = 0;
        tzp = nl + 1;

        HubOid co;
        hub_oid_from_hex(f[0], &co);
        uint8_t type;
        uint8_t* data;
        size_t size;
        int e = hub_git_read(g, &co, &type, &data, &size);
        if(e) {
            fail("commit read %s: %s", f[0], errname(e));
            continue;
        }
        HubGitCommit c;
        e = hub_git_parse_commit(data, size, &c);
        CHECK(e == 0, "parse_commit %s", f[0]);
        char hex[41];
        hexstr(&c.tree, hex);
        CHECK(strcmp(hex, f[1]) == 0, "commit tree %s", f[0]);
        unsigned np = 0;
        for(char* q = f[2]; *q;) {
            if(np < HUB_GIT_MAX_PARENTS) {
                hexstr(&c.parent[np], hex);
                CHECK(strncmp(hex, q, 40) == 0, "commit parent %u of %s", np, f[0]);
            }
            np++;
            q += 40;
            if(*q == ' ') q++;
        }
        CHECK(c.nparents == np, "nparents %u/%u for %s", c.nparents, np, f[0]);
        if(np > 1) merges++;
        if(np > HUB_GIT_MAX_PARENTS) big_merge++;
        CHECK(c.author.name_len == strlen(f[3]) && !memcmp(c.author.name, f[3], c.author.name_len), "author name %s", f[0]);
        CHECK(c.author.email_len == strlen(f[4]) && !memcmp(c.author.email, f[4], c.author.email_len), "author email %s", f[0]);
        CHECK(c.author.time == atoll(f[5]) && strcmp(c.author.tz, tz_a) == 0, "author time/tz %s: %lld %s vs %s %s", f[0], (long long)c.author.time, c.author.tz, f[5], tz_a);
        CHECK(c.committer.name_len == strlen(f[6]) && !memcmp(c.committer.name, f[6], c.committer.name_len), "committer name %s", f[0]);
        CHECK(c.committer.email_len == strlen(f[7]) && !memcmp(c.committer.email, f[7], c.committer.email_len), "committer email %s", f[0]);
        CHECK(c.committer.time == atoll(f[8]) && strcmp(c.committer.tz, tz_c) == 0, "committer time/tz %s", f[0]);
        size_t ml = c.msg_len, el = strlen(f[9]);
        trim_nl(c.msg, &ml);
        trim_nl(f[9], &el);
        CHECK(ml == el && memcmp(c.msg, f[9], ml) == 0, "commit message %s", f[0]);
        hub_git_free(data);
        cnt++;
    }
    printf("commits: %u parsed and compared with git log (%u merges, %u with > %d parents)\n", cnt, merges, big_merge, HUB_GIT_MAX_PARENTS);
    CHECK(cnt >= 200, "fewer than 200 commits (%u)", cnt);
    free(log);
    free(tzs);
}

static void test_refs(HubGit* g) {
    char* refs = git_out("for-each-ref --format='%%(refname) %%(objectname) %%(*objectname) %%(objecttype) %%(taggername)'");
    unsigned cnt = 0, tags = 0;
    for(char* line = strtok(refs, "\n"); line; line = strtok(NULL, "\n")) {
        char name[256], oid[64], peeled[64] = "", ty[16];
        char tagger[128] = "";
        /* the *objectname field is empty for non-tags, so parse by hand */
        char* a = strchr(line, ' ');
        *a = 0;
        snprintf(name, sizeof name, "%s", line);
        snprintf(oid, sizeof oid, "%.40s", a + 1);
        char* rest = a + 1 + 40 + 1; /* "<peeled or empty> <type> <taggername>" */
        if(rest[0] != ' ') {
            snprintf(peeled, sizeof peeled, "%.40s", rest);
            rest += 41;
        } else {
            rest += 1;
        }
        sscanf(rest, "%15s %127[^\n]", ty, tagger);
        /* %(*objectname) peels one level only in older git (2.43); a tag of a tag must
         * resolve to the final object, so ask rev-parse for the full peel. */
        if(!strcmp(ty, "tag")) {
            char* full = git_out("rev-parse '%s^{}'", name);
            snprintf(peeled, sizeof peeled, "%.40s", full);
            free(full);
        }
        HubOid o;
        int e = hub_git_ref_read(g, name, &o);
        char hex[41];
        hexstr(&o, hex);
        CHECK(e == 0 && strcmp(hex, oid) == 0, "ref_read %s: %s %s vs %s", name, errname(e), hex, oid);
        e = hub_git_resolve(g, name, &o);
        hexstr(&o, hex);
        const char* want = peeled[0] ? peeled : oid;
        CHECK(e == 0 && strcmp(hex, want) == 0, "resolve %s: %s", name, errname(e));
        /* short names */
        const char* shortn = NULL;
        if(!strncmp(name, "refs/heads/", 11)) shortn = name + 11;
        if(!strncmp(name, "refs/tags/", 10)) shortn = name + 10;
        if(shortn) {
            e = hub_git_resolve(g, shortn, &o);
            hexstr(&o, hex);
            CHECK(e == 0 && strcmp(hex, want) == 0, "resolve short %s: %s", shortn, errname(e));
        }
        if(!strcmp(ty, "tag")) {
            tags++;
            HubOid to;
            hub_oid_from_hex(oid, &to);
            uint8_t type;
            uint8_t* data;
            size_t size;
            e = hub_git_read(g, &to, &type, &data, &size);
            CHECK(e == 0 && type == HUB_OBJ_TAG, "tag read %s", name);
            if(e == 0) {
                HubGitTag t;
                e = hub_git_parse_tag(data, size, &t);
                CHECK(e == 0, "parse_tag %s", name);
                char th[41];
                hexstr(&t.object, th);
                char* tn = strrchr(name, '/') + 1;
                CHECK(t.tag_len == strlen(tn) && !memcmp(t.tag, tn, t.tag_len), "tag name %s", name);
                CHECK(t.has_tagger && t.tagger.name_len == strlen(tagger) && !memcmp(t.tagger.name, tagger, t.tagger.name_len), "tagger %s", name);
                CHECK(t.type != 0, "tag type %s", name);
                CHECK(t.msg_len > 0, "tag message %s", name);
                hub_git_free(data);
            }
            /* peel directly */
            HubOid po = to;
            e = hub_git_peel(g, &po);
            hexstr(&po, hex);
            CHECK(e == 0 && strcmp(hex, want) == 0, "peel %s", name);
        }
        cnt++;
    }
    free(refs);

    char* head = git_out("rev-parse HEAD");
    head[40] = 0;
    HubOid o;
    char hex[41];
    int e = hub_git_resolve(g, "HEAD", &o);
    hexstr(&o, hex);
    CHECK(e == 0 && !strcmp(hex, head), "resolve HEAD");
    e = hub_git_resolve(g, head, &o);
    hexstr(&o, hex);
    CHECK(e == 0 && !strcmp(hex, head), "resolve 40-hex");
    free(head);
    CHECK(hub_git_resolve(g, "no-such-ref", &o) == HUB_GIT_ERR_NOTFOUND, "resolve unknown");
    CHECK(hub_git_resolve(g, "refs/heads/nope", &o) == HUB_GIT_ERR_NOTFOUND, "resolve unknown full");
    CHECK(hub_git_resolve(g, "../../etc/passwd", &o) == HUB_GIT_ERR_BADARG, "resolve ..");
    CHECK(hub_git_resolve(g, "refs/heads/a b", &o) == HUB_GIT_ERR_BADARG, "resolve space");
    CHECK(hub_git_resolve(g, "", &o) == HUB_GIT_ERR_BADARG, "resolve empty");
    HubOid missing;
    hub_oid_from_hex("1234567890123456789012345678901234567890", &missing);
    uint8_t t;
    size_t sz;
    CHECK(hub_git_info(g, &missing, &t, &sz) == HUB_GIT_ERR_NOTFOUND, "info missing object");
    uint8_t* d;
    CHECK(hub_git_read(g, &missing, &t, &d, &sz) == HUB_GIT_ERR_NOTFOUND, "read missing object");
    printf("refs: %u refs compared (%u annotated tags)\n", cnt, tags);
    CHECK(tags >= 3, "expected at least 3 annotated tags, got %u", tags);

    /* resolve cost measure */
    heap_mark();
    hub_git_resolve(g, "v1.0", &o);
    heap_note("resolve(tag)", heap_used(), 0);
    heap_mark();
    hub_git_resolve(g, "HEAD", &o);
    heap_note("resolve(HEAD)", heap_used(), 0);
}

/* ---------- out of memory ---------- */

static void test_nomem(HubGit* g, const HubOid* some) {
    for(size_t k = 0; k < 12; k++) {
        heap_fail_after = heap_calls + k;
        uint8_t t;
        uint8_t* d = NULL;
        size_t sz;
        int e = hub_git_read(g, some, &t, &d, &sz);
        if(e == 0) hub_git_free(d);
        CHECK(e == 0 || e == HUB_GIT_ERR_NOMEM, "nomem read k=%zu got %s", k, errname(e));
        heap_fail_after = (size_t)-1;
    }
}

/* ---------- fuzz: corrupt a copy of the repo ---------- */

static void test_fuzz(const char* rp) {
    snprintf(repo, sizeof repo, "%s", rp);
    FILE* lp = gitp("cat-file --batch-all-objects --batch-check='%%(objectname)'");
    size_t ln;
    char* list = slurp(lp, &ln);
    unsigned n = 0;
    for(char* p = list; *p; p++) n += (*p == '\n');
    HubOid* oids = calloc(n, sizeof(HubOid));
    unsigned i = 0;
    for(char* l = strtok(list, "\n"); l && i < n; l = strtok(NULL, "\n")) hub_oid_from_hex(l, &oids[i++]);

    char cmd[1100], packf[1000];
    FILE* pp;
    snprintf(cmd, sizeof cmd, "ls %s/objects/pack/*.pack", rp);
    pp = popen(cmd, "r");
    if(!pp || !fgets(packf, sizeof packf, pp)) {
        fail("no pack in %s", rp);
        return;
    }
    pclose(pp);
    packf[strcspn(packf, "\n")] = 0;
    strcpy(cmd, packf);
    strcpy(cmd + strlen(cmd) - 5, ".idx");
    char idxf[1100];
    memcpy(idxf, cmd, strlen(cmd) + 1);

    unsigned long counts[8] = {0};
    unsigned rounds = 24;
    srand(1234);
    for(unsigned round = 0; round < rounds; round++) {
        const char* victim = (round % 4 == 3) ? idxf : packf;
        FILE* f = fopen(victim, "r+b");
        if(!f) {
            fail("open %s", victim);
            return;
        }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        long offs[24];
        uint8_t saved[24];
        for(int k = 0; k < 24; k++) {
            offs[k] = (long)(((unsigned long)rand() * 65537ul + (unsigned long)rand()) % (unsigned long)len);
            fseek(f, offs[k], SEEK_SET);
            if(fread(&saved[k], 1, 1, f) != 1) saved[k] = 0;
            uint8_t b = saved[k] ^ (uint8_t)(1 + rand() % 255);
            fseek(f, offs[k], SEEK_SET);
            fwrite(&b, 1, 1, f);
        }
        fflush(f);
        HubGit* g = hub_git_open(NULL, rp);
        for(unsigned k = 0; k < n; k++) {
            uint8_t t;
            size_t sz;
            uint8_t* d = NULL;
            hub_git_info(g, &oids[k], &t, &sz);
            int e = hub_git_read(g, &oids[k], &t, &d, &sz);
            counts[e < 0 ? -e : 0]++;
            if(e == 0) {
                if(t == HUB_OBJ_COMMIT) {
                    HubGitCommit c;
                    hub_git_parse_commit(d, sz, &c);
                } else if(t == HUB_OBJ_TREE) {
                    HubTreeIter it;
                    HubTreeEntry te;
                    hub_tree_iter_init(&it, d, sz);
                    while(hub_tree_next(&it, &te) > 0) {}
                } else if(t == HUB_OBJ_TAG) {
                    HubGitTag tg;
                    hub_git_parse_tag(d, sz, &tg);
                }
                hub_git_free(d);
            }
            Sink sk = {0};
            hub_git_stream(g, &oids[k], sink_write, &sk, &t, &sz);
            free(sk.v);
        }
        hub_git_close(g);
        CHECK(heap_cur == 0, "fuzz round %u leaked %zu bytes", round, heap_cur);
        for(int k = 23; k >= 0; k--) {
            fseek(f, offs[k], SEEK_SET);
            fwrite(&saved[k], 1, 1, f);
        }
        fclose(f);
    }
    printf("fuzz: %u rounds x %u objects, results ok=%lu notfound=%lu toolarge=%lu corrupt=%lu io=%lu (no crash, no leak)\n",
           rounds, n, counts[0], counts[1], counts[2], counts[3], counts[5]);
    free(oids);
    free(list);
}

/* ---------- main ---------- */

int main(int argc, char** argv) {
    if(argc >= 2 && strcmp(argv[1], "--inflate") == 0) return test_inflate() | test_far();
    if(argc < 2) {
        fprintf(stderr, "usage: %s --inflate | <repo.git> [--limit N] [--fuzz]\n", argv[0]);
        return 2;
    }
    snprintf(repo, sizeof repo, "%s", argv[1]);
    size_t limit = HUB_GIT_MAX_OBJ;
    bool fuzz = false;
    for(int i = 2; i < argc; i++) {
        if(!strcmp(argv[i], "--limit") && i + 1 < argc) limit = (size_t)atol(argv[++i]);
        if(!strcmp(argv[i], "--fuzz")) fuzz = true;
    }
    if(fuzz) {
        test_fuzz(argv[1]);
        printf("%s: checks=%u fail=%u\n", argv[1], n_checks, n_fail);
        return n_fail != 0;
    }

    heap_mark();
    HubGit* g = hub_git_open(NULL, repo);
    if(!g) {
        fprintf(stderr, "cannot open %s\n", repo);
        return 2;
    }
    hub_git_set_limit(g, limit);
    printf("== %s  limit=%zu\n", argv[1], limit);

    test_matrix(g, limit);

    /* the structure tests need room for big trees */
    hub_git_set_limit(g, 1024 * 1024);
    test_trees(g);
    test_commits(g);
    test_refs(g);
    {
        char* head = git_out("rev-parse HEAD");
        head[40] = 0;
        HubOid o;
        hub_oid_from_hex(head, &o);
        free(head);
        test_nomem(g, &o);
    }
    hub_git_close(g);
    CHECK(heap_cur == 0, "leak: %zu bytes still allocated after close", heap_cur);

    printf("peak heap per operation (bytes above the open handle; limit %zu):\n", limit);
    printf("  %-20s %8s %10s %9s %6s\n", "operation", "peak", "at size", "overhead", "count");
    for(unsigned i = 0; i < nclasses; i++) {
        printf("  %-20s %8zu %10zu %9zu %6u\n", classes[i].name, classes[i].peak, classes[i].at_size,
               classes[i].over, classes[i].n);
    }
    printf("%s: checks=%u fail=%u\n", argv[1], n_checks, n_fail);
    return n_fail != 0;
}
