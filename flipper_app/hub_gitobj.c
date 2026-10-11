#include "hub_gitobj.h"

#include "hub_gitio.h"
#include "hub_inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define MAX_PACKS   64
#define IDX_HDR     8
#define IDX_FANOUT  1024
#define IDX_TABLE   (IDX_HDR + IDX_FANOUT)
#define HEAD_PEEK   32 /* bytes of a loose object to inflate for its header */
#define PEEL_MAX    8
#define SYMREF_MAX  6

typedef struct {
    char name[46]; /* "pack-" + 40 hex */
    uint8_t state; /* 0 not checked, 1 ok, 2 bad */
    uint32_t nobj;
} IdxEnt;

struct HubGit {
    void* io;
    size_t max_obj;
    uint64_t cache_max;

    IdxEnt* idx;
    unsigned nidx;
    unsigned last_pack;
    bool idx_loaded;
    bool too_many; /* more than MAX_PACKS idx files exist */

    HubGitFile* pack_f;
    int pack_i;

    HubGitFile* gf; /* graph.bin */
    uint8_t* fan;   /* its fanout table (1 KB), NULL if there was no memory */
    uint32_t gcount;
    uint8_t gstate; /* 0 not tried, 1 open, 2 none */
    bool stale, quiet;

    char base[HUB_GIT_PATH_MAX + 1];
    char path[HUB_GIT_PATH_MAX + HUB_GIT_REF_MAX + 64];
    char refname[HUB_GIT_REF_MAX + 8];
    char cand[HUB_GIT_REF_MAX + 16];
    char line[256];
    uint8_t buf[256];
};

typedef struct {
    int pack; /* -1 = loose object */
    uint64_t off;
    HubOid oid; /* used for loose objects */
} Loc;

typedef struct {
    uint8_t type; /* 1..4 plain, 6 ofs-delta, 7 ref-delta */
    uint32_t size;
    uint64_t data_off;
    uint64_t base_off;
    HubOid base_oid;
} PackHdr;

typedef struct {
    int pack;
    uint32_t dsize;
    uint64_t data_off;
} ChainEnt;

typedef struct {
    ChainEnt* v;
    unsigned n, cap;
} Chain;

/* ---------- small helpers ---------- */

static int hexv(int c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool hub_oid_from_hex(const char* hex, HubOid* oid) {
    for(int i = 0; i < 20; i++) {
        int a = hexv((unsigned char)hex[2 * i]);
        int b = a < 0 ? -1 : hexv((unsigned char)hex[2 * i + 1]);
        if(a < 0 || b < 0) return false;
        oid->b[i] = (uint8_t)(a << 4 | b);
    }
    return hex[40] == '\0';
}

void hub_oid_to_hex(const HubOid* oid, char out[41]) {
    static const char d[] = "0123456789abcdef";
    for(int i = 0; i < 20; i++) {
        out[2 * i] = d[oid->b[i] >> 4];
        out[2 * i + 1] = d[oid->b[i] & 15];
    }
    out[40] = '\0';
}

static uint32_t be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* Heartbeat hook of the host app: called often, returns false to abort the work. */
bool (*hub_gitobj_beat)(void);
static inline bool obj_beat(void) {
    return !hub_gitobj_beat || hub_gitobj_beat();
}

static bool read_at(HubGitFile* f, uint64_t off, void* buf, size_t n) {
    if(!obj_beat()) return false;
    if(!hub_gitio_seek(f, off)) return false;
    return hub_gitio_read(f, buf, n) == n;
}

static size_t rd_file(void* ctx, uint8_t* buf, size_t max) {
    if(!obj_beat()) return 0;
    return hub_gitio_read((HubGitFile*)ctx, buf, max);
}

static int inf_err(int r) {
    switch(r) {
    case HUB_INF_OK:
        return HUB_GIT_OK;
    case HUB_INF_ERR_NOMEM:
        return HUB_GIT_ERR_NOMEM;
    case HUB_INF_ERR_WRITE:
        return HUB_GIT_ERR_WRITE;
    case HUB_INF_ERR_FAR:
        return HUB_GIT_ERR_FAR;
    default:
        return HUB_GIT_ERR_CORRUPT;
    }
}

static void clean_tmp(HubGit* g);

/* ---------- handle ---------- */

HubGit* hub_git_open(void* io, const char* repo_dir) {
    size_t len = strlen(repo_dir);
    if(len == 0 || len > HUB_GIT_PATH_MAX - 40) return NULL;
    HubGit* g = calloc(1, sizeof(HubGit));
    if(!g) return NULL;
    g->io = io;
    g->max_obj = HUB_GIT_MAX_OBJ;
    g->cache_max = HUB_GIT_CACHE_MAX;
    g->pack_i = -1;
    memcpy(g->base, repo_dir, len);
    while(len > 1 && g->base[len - 1] == '/') g->base[--len] = '\0';

    snprintf(g->path, sizeof(g->path), "%s/objects", g->base);
    HubGitDir* d = hub_gitio_dir_open(io, g->path);
    if(!d) {
        free(g);
        return NULL;
    }
    hub_gitio_dir_close(d);
    clean_tmp(g);
    return g;
}

void (*hub_gitobj_stale)(const char* repo_dir);

void hub_git_close(HubGit* g) {
    if(!g) return;
    if(g->stale && !g->quiet && hub_gitobj_stale) hub_gitobj_stale(g->base);
    if(g->pack_f) hub_gitio_close(g->pack_f);
    if(g->gf) hub_gitio_close(g->gf);
    free(g->fan);
    free(g->idx);
    free(g);
}

void hub_git_set_limit(HubGit* g, size_t max_obj) {
    g->max_obj = max_obj;
}

void hub_git_set_cache_limit(HubGit* g, uint64_t bytes) {
    g->cache_max = bytes;
}

size_t hub_git_limit(const HubGit* g) {
    return g->max_obj;
}

void hub_git_free(void* p) {
    free(p);
}

/* ---------- idx list and lookup ---------- */

static int idx_load(HubGit* g) {
    if(g->idx_loaded) return HUB_GIT_OK;
    snprintf(g->path, sizeof(g->path), "%s/objects/pack", g->base);
    HubGitDir* d = hub_gitio_dir_open(g->io, g->path);
    if(d) {
        unsigned cap = 0;
        char name[64];
        bool isdir;
        while(hub_gitio_dir_read(d, name, sizeof(name), &isdir)) {
            size_t n = strlen(name);
            if(isdir || n != 49 || strncmp(name, "pack-", 5) != 0 ||
               strcmp(name + 45, ".idx") != 0)
                continue;
            if(g->nidx >= MAX_PACKS) {
                g->too_many = true;
                break;
            }
            if(g->nidx == cap) {
                cap += 4;
                IdxEnt* v = realloc(g->idx, cap * sizeof(IdxEnt));
                if(!v) {
                    hub_gitio_dir_close(d);
                    return HUB_GIT_ERR_NOMEM;
                }
                g->idx = v;
            }
            IdxEnt* e = &g->idx[g->nidx++];
            memcpy(e->name, name, 45);
            e->name[45] = '\0';
            e->state = 0;
            e->nobj = 0;
        }
        hub_gitio_dir_close(d);
    }
    g->idx_loaded = true;
    return HUB_GIT_OK;
}

/* Look for oid in idx file i. 1 = found (off set), 0 = not here, <0 = error */
static int idx_find(HubGit* g, unsigned i, const HubOid* oid, uint64_t* off) {
    IdxEnt* e = &g->idx[i];
    if(e->state == 2) return 0;
    snprintf(g->path, sizeof(g->path), "%s/objects/pack/%s.idx", g->base, e->name);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return 0;
    int res = 0;
    uint8_t* b = g->buf;

    if(e->state == 0) {
        if(!read_at(f, 0, b, IDX_HDR) || memcmp(b, "\377tOc", 4) != 0 || be32(b + 4) != 2 ||
           !read_at(f, IDX_HDR + 4 * 255, b, 4)) {
            e->state = 2;
            goto out;
        }
        e->nobj = be32(b);
        e->state = 1;
    }
    uint32_t nobj = e->nobj;
    uint32_t lo = 0, hi;
    unsigned c = oid->b[0];
    if(c > 0) {
        if(!read_at(f, IDX_HDR + 4 * (uint64_t)(c - 1), b, 8)) goto ioerr;
        lo = be32(b);
        hi = be32(b + 4);
    } else {
        if(!read_at(f, IDX_HDR, b, 4)) goto ioerr;
        hi = be32(b);
    }
    if(lo > hi || hi > nobj) {
        res = HUB_GIT_ERR_CORRUPT;
        goto out;
    }
    while(hi - lo > 12) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(!read_at(f, IDX_TABLE + 20 * (uint64_t)mid, b, 20)) goto ioerr;
        int cmp = memcmp(oid->b, b, 20);
        if(cmp == 0) {
            lo = mid;
            hi = mid + 1;
            goto found_range;
        }
        if(cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
found_range:;
    uint32_t n = hi - lo;
    if(n == 0) goto out;
    if(!read_at(f, IDX_TABLE + 20 * (uint64_t)lo, b, 20 * (size_t)n)) goto ioerr;
    uint32_t hit = 0;
    bool got = false;
    for(uint32_t k = 0; k < n; k++) {
        if(memcmp(oid->b, b + 20 * k, 20) == 0) {
            hit = lo + k;
            got = true;
            break;
        }
    }
    if(!got) goto out;

    {
        uint64_t t4 = IDX_TABLE + 24 * (uint64_t)nobj + 4 * (uint64_t)hit;
        if(!read_at(f, t4, b, 4)) goto ioerr;
        uint32_t v = be32(b);
        if(v & 0x80000000u) {
            uint64_t t8 = IDX_TABLE + 28 * (uint64_t)nobj + 8 * (uint64_t)(v & 0x7fffffffu);
            if(!read_at(f, t8, b, 8)) goto ioerr;
            *off = (uint64_t)be32(b) << 32 | be32(b + 4);
        } else {
            *off = v;
        }
        res = 1;
    }
    goto out;
ioerr:
    res = HUB_GIT_ERR_IO;
out:
    hub_gitio_close(f);
    return res;
}

static void loose_path(HubGit* g, const HubOid* oid) {
    char hex[41];
    hub_oid_to_hex(oid, hex);
    snprintf(g->path, sizeof(g->path), "%s/objects/%.2s/%s", g->base, hex, hex + 2);
}

static int locate(HubGit* g, const HubOid* oid, Loc* loc) {
    loose_path(g, oid);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(f) {
        hub_gitio_close(f);
        loc->pack = -1;
        loc->off = 0;
        loc->oid = *oid;
        return HUB_GIT_OK;
    }
    int r = idx_load(g);
    if(r) return r;
    for(unsigned k = 0; k < g->nidx; k++) {
        unsigned i = (g->last_pack + k) % g->nidx;
        uint64_t off = 0;
        r = idx_find(g, i, oid, &off);
        if(r < 0) return r;
        if(r == 1) {
            g->last_pack = i;
            loc->pack = (int)i;
            loc->off = off;
            return HUB_GIT_OK;
        }
    }
    return g->too_many ? HUB_GIT_ERR_TOOMANY : HUB_GIT_ERR_NOTFOUND;
}

/* ---------- pack entries ---------- */

static HubGitFile* pack_open(HubGit* g, int pack) {
    if(g->pack_f && g->pack_i == pack) return g->pack_f;
    if(g->pack_f) {
        hub_gitio_close(g->pack_f);
        g->pack_f = NULL;
        g->pack_i = -1;
    }
    snprintf(g->path, sizeof(g->path), "%s/objects/pack/%s.pack", g->base, g->idx[pack].name);
    g->pack_f = hub_gitio_open(g->io, g->path);
    if(g->pack_f) g->pack_i = pack;
    return g->pack_f;
}

static int pack_hdr(HubGit* g, int pack, uint64_t off, PackHdr* h) {
    HubGitFile* f = pack_open(g, pack);
    if(!f) return HUB_GIT_ERR_IO;
    if(!hub_gitio_seek(f, off)) return HUB_GIT_ERR_IO;
    size_t n = hub_gitio_read(f, g->buf, 48);
    const uint8_t* b = g->buf;
    size_t i = 0;
    if(n < 2) return HUB_GIT_ERR_CORRUPT;

    uint8_t c = b[i++];
    h->type = (c >> 4) & 7;
    uint64_t size = c & 15;
    unsigned shift = 4;
    while(c & 0x80) {
        if(i >= n || shift > 28) return HUB_GIT_ERR_CORRUPT;
        c = b[i++];
        size |= (uint64_t)(c & 0x7f) << shift;
        shift += 7;
    }
    if(size > 0xFFFFFFFFull) return HUB_GIT_ERR_CORRUPT;
    h->size = (uint32_t)size;

    if(h->type == 6) {
        if(i >= n) return HUB_GIT_ERR_CORRUPT;
        c = b[i++];
        uint64_t ofs = c & 0x7f;
        while(c & 0x80) {
            if(i >= n || ofs > (1ull << 56)) return HUB_GIT_ERR_CORRUPT;
            c = b[i++];
            ofs = ((ofs + 1) << 7) | (c & 0x7f);
        }
        if(ofs == 0 || ofs > off) return HUB_GIT_ERR_CORRUPT;
        h->base_off = off - ofs;
    } else if(h->type == 7) {
        if(i + 20 > n) return HUB_GIT_ERR_CORRUPT;
        memcpy(h->base_oid.b, b + i, 20);
        i += 20;
    } else if(h->type < 1 || h->type > 4) {
        return HUB_GIT_ERR_CORRUPT;
    }
    h->data_off = off + i;
    return HUB_GIT_OK;
}

/* ---------- chains ---------- */

static int chain_push(Chain* ch, int pack, const PackHdr* h) {
    if(ch->n >= HUB_GIT_MAX_DEPTH) return HUB_GIT_ERR_CORRUPT;
    if(ch->n == ch->cap) {
        unsigned cap = ch->cap ? ch->cap * 2 : 8;
        ChainEnt* v = realloc(ch->v, cap * sizeof(ChainEnt));
        if(!v) return HUB_GIT_ERR_NOMEM;
        ch->v = v;
        ch->cap = cap;
    }
    ChainEnt* e = &ch->v[ch->n++];
    e->pack = pack;
    e->dsize = h->size;
    e->data_off = h->data_off;
    return HUB_GIT_OK;
}

/* Starting at a delta entry, collect the chain (top delta first) and find the
 * plain base. */
static int walk_chain(HubGit* g, Loc loc, PackHdr h, Chain* ch, Loc* base) {
    for(;;) {
        if(h.type >= 1 && h.type <= 4) {
            *base = loc;
            return HUB_GIT_OK;
        }
        int r = chain_push(ch, loc.pack, &h);
        if(r) return r;
        if(h.type == 6) {
            loc.off = h.base_off;
        } else {
            Loc l2;
            r = locate(g, &h.base_oid, &l2);
            if(r == HUB_GIT_ERR_NOTFOUND) return HUB_GIT_ERR_CORRUPT;
            if(r) return r;
            if(l2.pack < 0) {
                *base = l2;
                return HUB_GIT_OK;
            }
            loc = l2;
        }
        r = pack_hdr(g, loc.pack, loc.off, &h);
        if(r) return r;
    }
}

/* ---------- loose objects ---------- */

static int type_from_name(const char* s, size_t n) {
    if(n == 4 && memcmp(s, "blob", 4) == 0) return HUB_OBJ_BLOB;
    if(n == 4 && memcmp(s, "tree", 4) == 0) return HUB_OBJ_TREE;
    if(n == 6 && memcmp(s, "commit", 6) == 0) return HUB_OBJ_COMMIT;
    if(n == 3 && memcmp(s, "tag", 3) == 0) return HUB_OBJ_TAG;
    return 0;
}

/* Open the loose object (path already in g->path) and parse "<type> <size>\0".
 * On success the file is open and the caller closes it. */
static int loose_head(
    HubGit* g,
    const HubOid* oid,
    HubGitFile** fout,
    uint8_t* type,
    size_t* size,
    size_t* hlen) {
    uint8_t h[HEAD_PEEK];
    loose_path(g, oid);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return HUB_GIT_ERR_NOTFOUND;
    size_t got = 0;
    int r = hub_inflate_buf(rd_file, f, h, sizeof(h), HUB_INF_PARTIAL, &got, NULL);
    if(r) {
        hub_gitio_close(f);
        return inf_err(r);
    }
    size_t i = 0;
    while(i < got && h[i] != ' ') i++;
    int t = i < got ? type_from_name((const char*)h, i) : 0;
    if(!t) goto bad;
    i++;
    uint64_t sz = 0;
    size_t digits = 0;
    while(i < got && h[i] >= '0' && h[i] <= '9') {
        sz = sz * 10 + (h[i] - '0');
        i++;
        digits++;
        if(sz > 0xFFFFFFFFull) goto bad;
    }
    if(digits == 0 || i >= got || h[i] != '\0') goto bad;
    *type = (uint8_t)t;
    *size = (size_t)sz;
    *hlen = i + 1;
    *fout = f;
    return HUB_GIT_OK;
bad:
    hub_gitio_close(f);
    return HUB_GIT_ERR_CORRUPT;
}

typedef struct {
    HubGitSink sink;
    void* ctx;
    size_t skip;
    size_t total;
} SkipCtx;

static bool skip_write(void* c, const uint8_t* p, size_t n) {
    SkipCtx* s = c;
    if(s->skip) {
        size_t k = n < s->skip ? n : s->skip;
        s->skip -= k;
        p += k;
        n -= k;
    }
    if(n == 0) return true;
    s->total += n;
    return s->sink(s->ctx, p, n);
}

/* ---------- plain objects (loose or pack, no delta) ---------- */

static int plain_info(HubGit* g, Loc loc, uint8_t* type, size_t* size) {
    if(loc.pack < 0) {
        HubGitFile* f;
        size_t hl;
        int r = loose_head(g, &loc.oid, &f, type, size, &hl);
        if(r == HUB_GIT_OK) hub_gitio_close(f);
        return r;
    }
    PackHdr h;
    int r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
    *type = h.type;
    *size = h.size;
    return HUB_GIT_OK;
}

static int plain_read(
    HubGit* g,
    Loc loc,
    uint8_t* type,
    uint8_t** data,
    size_t* size) {
    HubGitFile* f;
    size_t hl = 0, sz = 0, got = 0, in_off = 0;
    uint8_t t = 0;
    int r;
    if(loc.pack < 0) {
        r = loose_head(g, &loc.oid, &f, &t, &sz, &hl);
        if(r) return r;
        if(!hub_gitio_seek(f, 0)) {
            hub_gitio_close(f);
            return HUB_GIT_ERR_IO;
        }
    } else {
        PackHdr h;
        r = pack_hdr(g, loc.pack, loc.off, &h);
        if(r) return r;
        if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
        t = h.type;
        sz = h.size;
        f = g->pack_f;
        in_off = h.data_off;
    }
    if(sz > g->max_obj) {
        r = HUB_GIT_ERR_TOOLARGE;
        goto out;
    }
    uint8_t* buf = malloc(hl + sz + 1);
    if(!buf) {
        r = HUB_GIT_ERR_NOMEM;
        goto out;
    }
    if(loc.pack >= 0 && !hub_gitio_seek(f, in_off)) {
        free(buf);
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    r = inf_err(hub_inflate_buf(rd_file, f, buf, hl + sz, 0, &got, NULL));
    if(r == HUB_GIT_OK && got != hl + sz) r = HUB_GIT_ERR_CORRUPT;
    if(r) {
        free(buf);
        goto out;
    }
    if(hl) memmove(buf, buf + hl, sz);
    buf[sz] = 0;
    *type = t;
    *data = buf;
    *size = sz;
out:
    if(loc.pack < 0) hub_gitio_close(f);
    return r;
}

/* ---------- delta ---------- */

static bool delta_varint(const uint8_t** p, const uint8_t* end, size_t* v) {
    size_t r = 0;
    unsigned shift = 0;
    for(;;) {
        if(*p >= end || shift > 28) return false;
        uint8_t c = *(*p)++;
        r |= (size_t)(c & 0x7f) << shift;
        shift += 7;
        if(!(c & 0x80)) break;
    }
    *v = r;
    return true;
}

/* Apply the delta of chain entry ce to *cur (size *cur_size). */
static int apply_delta(HubGit* g, const ChainEnt* ce, uint8_t** cur, size_t* cur_size) {
    if(ce->dsize > g->max_obj) return HUB_GIT_ERR_TOOLARGE;
    HubGitFile* f = pack_open(g, ce->pack);
    if(!f || !hub_gitio_seek(f, ce->data_off)) return HUB_GIT_ERR_IO;
    uint8_t* d = malloc((size_t)ce->dsize + 1);
    if(!d) return HUB_GIT_ERR_NOMEM;
    size_t got = 0;
    int r = inf_err(hub_inflate_buf(rd_file, f, d, ce->dsize, 0, &got, NULL));
    if(r == HUB_GIT_OK && got != ce->dsize) r = HUB_GIT_ERR_CORRUPT;
    if(r) {
        free(d);
        return r;
    }
    const uint8_t* p = d;
    const uint8_t* end = d + got;
    size_t src_size, dst_size;
    if(!delta_varint(&p, end, &src_size) || !delta_varint(&p, end, &dst_size) ||
       src_size != *cur_size) {
        free(d);
        return HUB_GIT_ERR_CORRUPT;
    }
    if(dst_size > g->max_obj) {
        free(d);
        return HUB_GIT_ERR_TOOLARGE;
    }
    uint8_t* out = malloc(dst_size + 1);
    if(!out) {
        free(d);
        return HUB_GIT_ERR_NOMEM;
    }
    size_t o = 0;
    const uint8_t* src = *cur;
    r = HUB_GIT_OK;
    while(p < end) {
        uint8_t cmd = *p++;
        if(cmd & 0x80) {
            size_t off = 0, len = 0;
            for(int k = 0; k < 4; k++) {
                if(cmd & (1 << k)) {
                    if(p >= end) goto bad;
                    off |= (size_t)(*p++) << (8 * k);
                }
            }
            for(int k = 0; k < 3; k++) {
                if(cmd & (0x10 << k)) {
                    if(p >= end) goto bad;
                    len |= (size_t)(*p++) << (8 * k);
                }
            }
            if(len == 0) len = 0x10000;
            if(off > src_size || len > src_size - off || len > dst_size - o) goto bad;
            memcpy(out + o, src + off, len);
            o += len;
        } else if(cmd) {
            if(cmd > (size_t)(end - p) || cmd > dst_size - o) goto bad;
            memcpy(out + o, p, cmd);
            p += cmd;
            o += cmd;
        } else {
            goto bad;
        }
    }
    if(o != dst_size) goto bad;
    out[o] = 0;
    free(d);
    free(*cur);
    *cur = out;
    *cur_size = dst_size;
    return HUB_GIT_OK;
bad:
    free(d);
    free(out);
    return HUB_GIT_ERR_CORRUPT;
}

/* Result size of a delta: first two varints of the top delta. */
static int delta_result_size(HubGit* g, const ChainEnt* ce, size_t* size) {
    HubGitFile* f = pack_open(g, ce->pack);
    if(!f || !hub_gitio_seek(f, ce->data_off)) return HUB_GIT_ERR_IO;
    uint8_t d[HEAD_PEEK];
    size_t got = 0;
    int r = hub_inflate_buf(rd_file, f, d, sizeof(d), HUB_INF_PARTIAL, &got, NULL);
    if(r) return inf_err(r);
    const uint8_t *p = d, *end = d + got;
    size_t a, b;
    if(!delta_varint(&p, end, &a) || !delta_varint(&p, end, &b)) return HUB_GIT_ERR_CORRUPT;
    *size = b;
    return HUB_GIT_OK;
}

/* ---------- public object API ---------- */

int hub_git_info(HubGit* g, const HubOid* oid, uint8_t* type, size_t* size) {
    Loc loc;
    int r = locate(g, oid, &loc);
    if(r) return r;
    if(loc.pack < 0) return plain_info(g, loc, type, size);

    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type >= 1 && h.type <= 4) {
        *type = h.type;
        *size = h.size;
        return HUB_GIT_OK;
    }
    Chain ch = {0};
    Loc base;
    r = walk_chain(g, loc, h, &ch, &base);
    if(r == HUB_GIT_OK) r = delta_result_size(g, &ch.v[0], size);
    if(r == HUB_GIT_OK) {
        size_t bsz;
        r = plain_info(g, base, type, &bsz);
    }
    free(ch.v);
    return r;
}

int hub_git_read(HubGit* g, const HubOid* oid, uint8_t* type, uint8_t** data, size_t* size) {
    Loc loc;
    int r = locate(g, oid, &loc);
    if(r) return r;
    if(loc.pack < 0) return plain_read(g, loc, type, data, size);

    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type >= 1 && h.type <= 4) return plain_read(g, loc, type, data, size);

    Chain ch = {0};
    Loc base;
    r = walk_chain(g, loc, h, &ch, &base);
    if(r) goto out;
    uint8_t* cur = NULL;
    size_t cur_size = 0;
    r = plain_read(g, base, type, &cur, &cur_size);
    if(r) goto out;
    for(unsigned i = ch.n; i > 0; i--) {
        r = apply_delta(g, &ch.v[i - 1], &cur, &cur_size);
        if(r) {
            free(cur);
            goto out;
        }
    }
    *data = cur;
    *size = cur_size;
out:
    free(ch.v);
    return r;
}

/* ---------- streaming and the spill-to-SD path ---------- */

/* Output file that also serves the far window of the inflater: bytes already
 * written are read back from it. For a loose object the inflater counts the
 * "<type> <size>\0" header too (hl bytes, kept in hdr), the file does not. */
typedef struct {
    HubGit* g;
    HubGitFile* f;
    bool failed;
    uint64_t w; /* bytes written */
    size_t hl;
    char hdr[24];
} FileSink;

static bool file_sink(void* c, const uint8_t* p, size_t n) {
    FileSink* s = c;
    if(hub_gitio_write(s->f, p, n) != n) {
        s->failed = true;
        return false;
    }
    s->w += n;
    return true;
}

#ifdef HUB_TEST_COUNTERS
unsigned long hub_far_reads, hub_far_bytes; /* host tests only */
#endif

static bool far_read(void* c, uint64_t pos, uint8_t* b, size_t n) {
    FileSink* s = c;
#ifdef HUB_TEST_COUNTERS
    hub_far_reads++;
    hub_far_bytes += n;
#endif
    for(; n && pos < s->hl; n--) *b++ = (uint8_t)s->hdr[pos++];
    if(!n) return true;
    if(!obj_beat() || !hub_gitio_seek(s->f, pos - s->hl) || hub_gitio_read(s->f, b, n) != n) return false;
    return hub_gitio_seek(s->f, s->w); /* back to the write position */
}

static int plain_stream(
    HubGit* g,
    Loc loc,
    HubGitSink sink,
    void* ctx,
    uint8_t* type,
    size_t* size,
    FileSink* fs) { /* fs: the sink's own file, for the far window (NULL: none) */
    int r;
    size_t out = 0;
    if(loc.pack < 0) {
        HubGitFile* f;
        size_t hl;
        r = loose_head(g, &loc.oid, &f, type, size, &hl);
        if(r) return r;
        if(!hub_gitio_seek(f, 0)) {
            hub_gitio_close(f);
            return HUB_GIT_ERR_IO;
        }
        SkipCtx sc = {sink, ctx, hl, 0};
        if(fs) {
            static const char* const tn[5] = {"", "commit", "tree", "blob", "tag"};
            fs->hl = hl;
            snprintf(fs->hdr, sizeof(fs->hdr), "%s %u", tn[*type], (unsigned)*size);
        }
        r = inf_err(hub_inflate_stream(rd_file, f, skip_write, &sc, 0, &out, NULL, fs ? far_read : NULL, fs));
        hub_gitio_close(f);
        if(r == HUB_GIT_OK && (out != hl + *size || sc.total != *size)) r = HUB_GIT_ERR_CORRUPT;
        return r;
    }
    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
    *type = h.type;
    *size = h.size;
    if(!hub_gitio_seek(g->pack_f, h.data_off)) return HUB_GIT_ERR_IO;
    r = inf_err(hub_inflate_stream(rd_file, g->pack_f, sink, ctx, 0, &out, NULL, fs ? far_read : NULL, fs));
    if(r == HUB_GIT_OK && out != h.size) r = HUB_GIT_ERR_CORRUPT;
    return r;
}

#define SPILL_BUF 1024
#define SPILL_PATH (HUB_GIT_PATH_MAX + 56)

/* States of the delta instruction parser (it sees the inflated bytes in
 * arbitrary chunks). */
enum { DS_SRC, DS_DST, DS_CMD, DS_ARGS, DS_INS };

typedef struct {
    HubGit* g;
    HubGitFile* src;
    HubGitFile* dst;
    uint8_t* cb;
    int st, err;
    uint32_t acc;
    unsigned shift, bit;
    uint8_t cmd;
    size_t src_size, dst_size, out, off, len, ins_left;
} DeltaSpill;

static bool ds_write(DeltaSpill* d, const uint8_t* p, size_t n) {
    if(n > d->dst_size - d->out) {
        d->err = HUB_GIT_ERR_CORRUPT;
        return false;
    }
    if(hub_gitio_write(d->dst, p, n) != n) {
        d->err = HUB_GIT_ERR_IO;
        return false;
    }
    d->out += n;
    return true;
}

static bool ds_copy(DeltaSpill* d) {
    size_t off = d->off, len = d->len ? d->len : 0x10000;
    if(off > d->src_size || len > d->src_size - off) {
        d->err = HUB_GIT_ERR_CORRUPT;
        return false;
    }
    if(!hub_gitio_seek(d->src, off)) {
        d->err = HUB_GIT_ERR_IO;
        return false;
    }
    while(len) {
        size_t k = len < SPILL_BUF ? len : SPILL_BUF;
        if(!obj_beat() || hub_gitio_read(d->src, d->cb, k) != k) {
            d->err = HUB_GIT_ERR_IO;
            return false;
        }
        if(!ds_write(d, d->cb, k)) return false;
        len -= k;
    }
    return true;
}

/* Skip the argument bits that are not set; run the copy when all are read. */
static bool ds_args_step(DeltaSpill* d) {
    while(d->bit < 7 && !(d->cmd & (1u << d->bit))) d->bit++;
    if(d->bit == 7) {
        d->st = DS_CMD;
        return ds_copy(d);
    }
    return true;
}

static bool ds_sink(void* c, const uint8_t* p, size_t n) {
    DeltaSpill* d = c;
    while(n) {
        switch(d->st) {
        case DS_SRC:
        case DS_DST: {
            uint8_t b = *p++;
            n--;
            if(d->shift > 28) {
                d->err = HUB_GIT_ERR_CORRUPT;
                return false;
            }
            d->acc |= (uint32_t)(b & 0x7f) << d->shift;
            d->shift += 7;
            if(!(b & 0x80)) {
                if(d->st == DS_SRC) {
                    if(d->acc != d->src_size) {
                        d->err = HUB_GIT_ERR_CORRUPT;
                        return false;
                    }
                    d->st = DS_DST;
                } else {
                    d->dst_size = d->acc;
                    d->st = DS_CMD;
                }
                d->acc = 0;
                d->shift = 0;
            }
            break;
        }
        case DS_CMD:
            d->cmd = *p++;
            n--;
            if(d->cmd & 0x80) {
                d->off = d->len = 0;
                d->bit = 0;
                d->st = DS_ARGS;
                if(!ds_args_step(d)) return false;
            } else if(d->cmd) {
                d->ins_left = d->cmd;
                d->st = DS_INS;
            } else {
                d->err = HUB_GIT_ERR_CORRUPT;
                return false;
            }
            break;
        case DS_ARGS: {
            uint8_t b = *p++;
            n--;
            if(d->bit < 4)
                d->off |= (size_t)b << (8 * d->bit);
            else
                d->len |= (size_t)b << (8 * (d->bit - 4));
            d->bit++;
            if(!ds_args_step(d)) return false;
            break;
        }
        default: { /* DS_INS */
            size_t k = n < d->ins_left ? n : d->ins_left;
            if(!ds_write(d, p, k)) return false;
            p += k;
            n -= k;
            d->ins_left -= k;
            if(d->ins_left == 0) d->st = DS_CMD;
        }
        }
    }
    return true;
}

typedef struct {
    char tmp[2][SPILL_PATH];
    char cache[SPILL_PATH];
    char cdir[SPILL_PATH];
    char vpath[SPILL_PATH + 72];
    uint8_t cb[SPILL_BUF];
} Spill;

static void spill_paths(HubGit* g, Spill* s, const HubOid* oid) {
    char hex[41];
    hub_oid_to_hex(oid, hex);
    snprintf(s->tmp[0], SPILL_PATH, "%s/hub/tmp/w0", g->base);
    snprintf(s->tmp[1], SPILL_PATH, "%s/hub/tmp/w1", g->base);
    snprintf(s->cdir, SPILL_PATH, "%s/hub/cache", g->base);
    snprintf(s->cache, SPILL_PATH, "%s/hub/cache/%s", g->base, hex);
}

static void clean_tmp(HubGit* g) {
    char p[SPILL_PATH];
    for(int i = 0; i < 3; i++) {
        snprintf(p, sizeof p, "%s/hub/tmp/w%d", g->base, i);
        hub_gitio_remove(g->io, p);
    }
}

/* Send a file of known size to the sink. */
static int file_stream(HubGit* g, const char* path, size_t size, uint8_t* cb, HubGitSink sink, void* ctx) {
    HubGitFile* f = hub_gitio_open(g->io, path);
    if(!f) return HUB_GIT_ERR_NOTFOUND;
    int r = HUB_GIT_OK;
    if(hub_gitio_size(f) != size) {
        hub_gitio_close(f);
        return HUB_GIT_ERR_NOTFOUND;
    }
    size_t left = size;
    while(left) {
        size_t k = left < SPILL_BUF ? left : SPILL_BUF;
        if(!obj_beat() || hub_gitio_read(f, cb, k) != k) {
            r = HUB_GIT_ERR_IO;
            break;
        }
        if(!sink(ctx, cb, k)) {
            r = HUB_GIT_ERR_WRITE;
            break;
        }
        left -= k;
    }
    hub_gitio_close(f);
    return r;
}

/* Keep hub/cache under the cap. keep is a file name that must stay. */
static void cache_trim(HubGit* g, Spill* s, const char* keep) {
    for(int pass = 0; pass < 64; pass++) {
        HubGitDir* d = hub_gitio_dir_open(g->io, s->cdir);
        if(!d) return;
        char name[64], victim[64];
        bool isdir, have = false;
        uint64_t sz, total = 0;
        while(hub_gitio_dir_read_ex(d, name, sizeof name, &isdir, &sz)) {
            total += sz;
            if(!have && strcmp(name, keep) != 0) {
                strcpy(victim, name);
                have = true;
            }
        }
        hub_gitio_dir_close(d);
        if(total <= g->cache_max || !have) return;
        snprintf(s->vpath, sizeof s->vpath, "%s/%s", s->cdir, victim);
        hub_gitio_remove(g->io, s->vpath);
    }
}

/* A delta that declares a big window: inflate it to a temp file (with the far
 * window), then run the delta parser over the file. */
static int delta_via_file(HubGit* g, Spill* s, HubGitFile* pf, const ChainEnt* ce, DeltaSpill* d, size_t* olen) {
    snprintf(s->vpath, sizeof s->vpath, "%s/hub/tmp/w2", g->base);
    HubGitFile* f = hub_gitio_create(g->io, s->vpath);
    if(!f || !hub_gitio_seek(pf, ce->data_off)) {
        if(f) hub_gitio_close(f);
        return HUB_GIT_ERR_IO;
    }
    FileSink fs = {.g = g, .f = f};
    int r = inf_err(hub_inflate_stream(rd_file, pf, file_sink, &fs, 0, olen, NULL, far_read, &fs));
    hub_gitio_close(f);
    if(r == HUB_GIT_ERR_WRITE) r = HUB_GIT_ERR_IO;
    if(r == HUB_GIT_OK && *olen != ce->dsize) r = HUB_GIT_ERR_CORRUPT;
    uint8_t* cb = r == HUB_GIT_OK ? malloc(SPILL_BUF) : NULL; /* not s->cb: the parser copies with it */
    if(r == HUB_GIT_OK) {
        r = cb ? file_stream(g, s->vpath, *olen, cb, ds_sink, d) : HUB_GIT_ERR_NOMEM;
        if(r == HUB_GIT_ERR_WRITE) r = HUB_GIT_ERR_CORRUPT;
    }
    free(cb);
    hub_gitio_remove(g->io, s->vpath);
    return r;
}

static void spill_dirs(HubGit* g, Spill* s);

/* Resolve a delta chain through temp files and send the result. */
static int spill_stream(
    HubGit* g,
    const HubOid* oid,
    Loc loc,
    PackHdr h,
    size_t rsz,
    HubGitSink sink,
    void* ctx) {
    Spill* s = malloc(sizeof(Spill));
    if(!s) return HUB_GIT_ERR_NOMEM;
    spill_paths(g, s, oid);

    /* 1. cache hit? */
    int r = file_stream(g, s->cache, rsz, s->cb, sink, ctx);
    if(r != HUB_GIT_ERR_NOTFOUND) {
        free(s);
        return r;
    }
    hub_gitio_remove(g->io, s->cache); /* wrong size: stale */

    Chain ch = {0};
    Loc base;
    spill_dirs(g, s);

    r = walk_chain(g, loc, h, &ch, &base);
    if(r) goto out;

    /* 2. base into temp file 0 */
    int cur = 0;
    HubGitFile* f = hub_gitio_create(g->io, s->tmp[0]);
    if(!f) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    FileSink fs = {.g = g, .f = f};
    uint8_t bt;
    size_t cur_size = 0;
    r = plain_stream(g, base, file_sink, &fs, &bt, &cur_size, &fs);
    hub_gitio_close(f);
    if(r == HUB_GIT_ERR_WRITE) r = HUB_GIT_ERR_IO;
    if(r) goto out;

    /* 3. every delta, from the base up */
    for(unsigned i = ch.n; i > 0; i--) {
        const ChainEnt* ce = &ch.v[i - 1];
        DeltaSpill d;
        memset(&d, 0, sizeof d);
        d.g = g;
        d.cb = s->cb;
        d.st = DS_SRC;
        d.src_size = cur_size;
        d.src = hub_gitio_open(g->io, s->tmp[cur]);
        d.dst = hub_gitio_create(g->io, s->tmp[cur ^ 1]);
        HubGitFile* pf = pack_open(g, ce->pack);
        if(!d.src || !d.dst || !pf || !hub_gitio_seek(pf, ce->data_off)) {
            r = HUB_GIT_ERR_IO;
        } else {
            size_t olen = 0;
            r = inf_err(hub_inflate_stream(rd_file, pf, ds_sink, &d, 0, &olen, NULL, NULL, NULL));
            if(r == HUB_GIT_ERR_FAR) r = delta_via_file(g, s, pf, ce, &d, &olen);
            if(r == HUB_GIT_ERR_WRITE && d.err) r = d.err;
            if(r == HUB_GIT_OK &&
               (olen != ce->dsize || d.st != DS_CMD || d.out != d.dst_size))
                r = HUB_GIT_ERR_CORRUPT;
        }
        if(d.src) hub_gitio_close(d.src);
        if(d.dst) hub_gitio_close(d.dst);
        if(r) goto out;
        hub_gitio_remove(g->io, s->tmp[cur]);
        cur ^= 1;
        cur_size = d.dst_size;
    }
    if(cur_size != rsz) {
        r = HUB_GIT_ERR_CORRUPT;
        goto out;
    }

    /* 4. cache it, then send */
    {
        const char* serve = s->tmp[cur];
        if(rsz <= g->cache_max && hub_gitio_rename(g->io, s->tmp[cur], s->cache)) {
            serve = s->cache;
            char hex[41];
            hub_oid_to_hex(oid, hex);
            cache_trim(g, s, hex);
        }
        r = file_stream(g, serve, rsz, s->cb, sink, ctx);
        if(r == HUB_GIT_ERR_NOTFOUND) r = HUB_GIT_ERR_IO;
    }
out:
    free(ch.v);
    hub_gitio_remove(g->io, s->tmp[0]);
    hub_gitio_remove(g->io, s->tmp[1]);
    free(s);
    return r;
}

static void spill_dirs(HubGit* g, Spill* s) {
    snprintf(s->vpath, sizeof s->vpath, "%s/hub", g->base);
    hub_gitio_mkdir(g->io, s->vpath);
    hub_gitio_mkdir(g->io, s->cdir);
    snprintf(s->vpath, sizeof s->vpath, "%s/hub/tmp", g->base);
    hub_gitio_mkdir(g->io, s->vpath);
}

/* A non-delta object for a sink. When its stream declares a window bigger than
 * the ring (git's own objects: 32 KB), it is inflated into hub/cache/<sha> first
 * (with the far window) and that file is sent. */
static int plain_cached(HubGit* g, const HubOid* oid, Loc loc, HubGitSink sink, void* ctx, uint8_t* type, size_t* size) {
    int r = plain_stream(g, loc, sink, ctx, type, size, NULL);
    if(r != HUB_GIT_ERR_FAR) return r;
    Spill* s = malloc(sizeof(Spill));
    if(!s) return HUB_GIT_ERR_NOMEM;
    spill_paths(g, s, oid);
    bool keep = *size <= g->cache_max;
    r = keep ? file_stream(g, s->cache, *size, s->cb, sink, ctx) : HUB_GIT_ERR_NOTFOUND;
    if(r == HUB_GIT_ERR_NOTFOUND) {
        spill_dirs(g, s);
        const char* path = keep ? s->cache : s->tmp[0];
        HubGitFile* f = hub_gitio_create(g->io, path);
        if(!f) {
            free(s);
            return HUB_GIT_ERR_IO;
        }
        FileSink fs = {.g = g, .f = f};
        r = plain_stream(g, loc, file_sink, &fs, type, size, &fs);
        hub_gitio_close(f);
        if(r == HUB_GIT_ERR_WRITE) r = HUB_GIT_ERR_IO;
        if(r == HUB_GIT_OK) {
            if(keep) {
                char hex[41];
                hub_oid_to_hex(oid, hex);
                cache_trim(g, s, hex);
            }
            r = file_stream(g, path, *size, s->cb, sink, ctx);
            if(r == HUB_GIT_ERR_NOTFOUND) r = HUB_GIT_ERR_IO;
        }
        if(!keep || (r != HUB_GIT_OK && r != HUB_GIT_ERR_WRITE)) hub_gitio_remove(g->io, path);
    }
    free(s);
    return r;
}

int hub_git_stream(
    HubGit* g,
    const HubOid* oid,
    HubGitSink sink,
    void* ctx,
    uint8_t* type,
    size_t* size) {
    Loc loc;
    int r = locate(g, oid, &loc);
    if(r) return r;
    /* An object that fits the RAM limit is read whole: its buffer is the inflate
     * window, so the 32 KB streaming window is needed only for bigger objects. */
    {
        uint8_t t0;
        size_t s0;
        if(hub_git_info(g, oid, &t0, &s0) == HUB_GIT_OK && s0 <= g->max_obj) {
            uint8_t* data = NULL;
            r = hub_git_read(g, oid, &t0, &data, &s0);
            if(r == HUB_GIT_OK) {
                *type = t0;
                *size = s0;
                if(s0 && !sink(ctx, data, s0)) r = HUB_GIT_ERR_WRITE;
                free(data);
                return r;
            }
            if(r != HUB_GIT_ERR_TOOLARGE && r != HUB_GIT_ERR_NOMEM) return r;
        }
    }
    if(loc.pack < 0) return plain_cached(g, oid, loc, sink, ctx, type, size);

    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type >= 1 && h.type <= 4) return plain_cached(g, oid, loc, sink, ctx, type, size);

    /* delta: RAM if it fits, else through temp files and the cache */
    size_t rsz;
    uint8_t t;
    r = hub_git_info(g, oid, &t, &rsz);
    if(r) return r;
    *type = t;
    *size = rsz;
    if(rsz <= g->max_obj) {
        uint8_t* data = NULL;
        r = hub_git_read(g, oid, &t, &data, &rsz);
        if(r == HUB_GIT_OK) {
            if(rsz && !sink(ctx, data, rsz)) r = HUB_GIT_ERR_WRITE;
            free(data);
            return r;
        }
        if(r != HUB_GIT_ERR_TOOLARGE) return r;
    }
    return spill_stream(g, oid, loc, h, rsz, sink, ctx);
}

/* ---------- refs ---------- */

static bool ref_name_ok(const char* n) {
    size_t len = strlen(n);
    if(len == 0 || len > HUB_GIT_REF_MAX || n[0] == '/' || n[len - 1] == '/') return false;
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)n[i];
        if(c <= 0x20 || c == 0x7f || strchr("~^:?*[\\", c)) return false;
        if(c == '.' && n[i + 1] == '.') return false;
        if(c == '/' && n[i + 1] == '/') return false;
        if(c == '@' && n[i + 1] == '{') return false;
    }
    return true;
}

/* Find name in packed-refs. */
static int packed_ref(HubGit* g, const char* name, HubOid* oid) {
    snprintf(g->path, sizeof(g->path), "%s/packed-refs", g->base);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return HUB_GIT_ERR_NOTFOUND;
    size_t nlen = strlen(name);
    size_t ll = 0;
    bool skip = false, found = false;
    size_t n;
    while(!found && (n = hub_gitio_read(f, g->buf, sizeof(g->buf))) > 0) {
        for(size_t i = 0; i < n && !found; i++) {
            char c = (char)g->buf[i];
            if(c != '\n') {
                if(ll < sizeof(g->line) - 1)
                    g->line[ll++] = c;
                else
                    skip = true;
                continue;
            }
            g->line[ll] = '\0';
            if(!skip && ll >= 42 && g->line[0] != '#' && g->line[0] != '^' &&
               g->line[40] == ' ' && ll - 41 == nlen && memcmp(g->line + 41, name, nlen) == 0) {
                g->line[40] = '\0';
                found = hub_oid_from_hex(g->line, oid);
                if(!found) {
                    hub_gitio_close(f);
                    return HUB_GIT_ERR_CORRUPT;
                }
            }
            ll = 0;
            skip = false;
        }
    }
    hub_gitio_close(f);
    return found ? HUB_GIT_OK : HUB_GIT_ERR_NOTFOUND;
}

int hub_git_ref_read(HubGit* g, const char* name, HubOid* oid) {
    if(!ref_name_ok(name)) return HUB_GIT_ERR_BADARG;
    strcpy(g->refname, name);
    for(int depth = 0; depth < SYMREF_MAX; depth++) {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->refname);
        HubGitFile* f = hub_gitio_open(g->io, g->path);
        if(!f) return packed_ref(g, g->refname, oid);
        size_t n = hub_gitio_read(f, g->buf, 255);
        hub_gitio_close(f);
        g->buf[n] = '\0';
        while(n > 0 && (g->buf[n - 1] == '\n' || g->buf[n - 1] == '\r' || g->buf[n - 1] == ' '))
            g->buf[--n] = '\0';
        if(n > 5 && memcmp(g->buf, "ref: ", 5) == 0) {
            const char* t = (const char*)g->buf + 5;
            if(!ref_name_ok(t)) return HUB_GIT_ERR_CORRUPT;
            strcpy(g->refname, t);
            continue;
        }
        if(n == 40 && hub_oid_from_hex((const char*)g->buf, oid)) return HUB_GIT_OK;
        return HUB_GIT_ERR_CORRUPT;
    }
    return HUB_GIT_ERR_CORRUPT;
}

int hub_git_peel(HubGit* g, HubOid* oid) {
    for(int i = 0; i < PEEL_MAX; i++) {
        uint8_t type;
        size_t size;
        int r = hub_git_info(g, oid, &type, &size);
        if(r) return r;
        if(type != HUB_OBJ_TAG) return HUB_GIT_OK;
        uint8_t* data;
        r = hub_git_read(g, oid, &type, &data, &size);
        if(r) return r;
        HubGitTag t;
        r = hub_git_parse_tag(data, size, &t);
        if(r == HUB_GIT_OK) *oid = t.object;
        free(data);
        if(r) return r;
    }
    return HUB_GIT_ERR_CORRUPT;
}

int hub_git_resolve(HubGit* g, const char* ref, HubOid* oid) {
    size_t len = strlen(ref);
    if(len == 0 || len > HUB_GIT_REF_MAX - 12) return HUB_GIT_ERR_BADARG;
    int r = HUB_GIT_ERR_NOTFOUND;
    if(len == 40 && hub_oid_from_hex(ref, oid)) {
        r = HUB_GIT_OK;
    } else if(strcmp(ref, "HEAD") == 0) {
        r = hub_git_ref_read(g, "HEAD", oid);
    } else {
        if(strncmp(ref, "refs/", 5) == 0) r = hub_git_ref_read(g, ref, oid);
        if(r == HUB_GIT_ERR_NOTFOUND || r == HUB_GIT_ERR_BADARG) {
            snprintf(g->cand, sizeof(g->cand), "refs/heads/%s", ref);
            r = hub_git_ref_read(g, g->cand, oid);
        }
        if(r == HUB_GIT_ERR_NOTFOUND) {
            snprintf(g->cand, sizeof(g->cand), "refs/tags/%s", ref);
            r = hub_git_ref_read(g, g->cand, oid);
        }
    }
    if(r) return r;
    return hub_git_peel(g, oid);
}

int hub_git_tree_of(HubGit* g, const HubOid* oid, HubOid* tree) {
    HubOid cur = *oid;
    for(int i = 0; i < PEEL_MAX; i++) {
        uint8_t type;
        size_t size;
        uint8_t* data;
        int r = hub_git_read(g, &cur, &type, &data, &size);
        if(r) return r;
        if(type == HUB_OBJ_TREE) {
            free(data);
            *tree = cur;
            return HUB_GIT_OK;
        }
        if(type == HUB_OBJ_COMMIT) {
            HubGitCommit c;
            r = hub_git_parse_commit(data, size, &c);
            if(r == HUB_GIT_OK) *tree = c.tree;
            free(data);
            return r;
        }
        if(type == HUB_OBJ_TAG) {
            HubGitTag t;
            r = hub_git_parse_tag(data, size, &t);
            if(r == HUB_GIT_OK) cur = t.object;
            free(data);
            if(r) return r;
            continue;
        }
        free(data);
        return HUB_GIT_ERR_BADARG;
    }
    return HUB_GIT_ERR_CORRUPT;
}

/* ---------- parsing ---------- */

static bool parse_ident(const char* s, size_t len, HubGitIdent* id) {
    memset(id, 0, sizeof(*id));
    const char* end = s + len;
    const char* lt = memchr(s, '<', len);
    if(!lt) {
        id->name = s;
        id->name_len = len;
        id->email = s;
        strcpy(id->tz, "+0000");
        return true;
    }
    const char* gt = memchr(lt, '>', (size_t)(end - lt));
    id->name = s;
    id->name_len = (size_t)(lt - s);
    while(id->name_len > 0 && id->name[id->name_len - 1] == ' ') id->name_len--;
    id->email = lt + 1;
    id->email_len = gt ? (size_t)(gt - lt - 1) : (size_t)(end - lt - 1);
    strcpy(id->tz, "+0000");
    if(!gt) return true;
    const char* p = gt + 1;
    while(p < end && *p == ' ') p++;
    int64_t t = 0;
    while(p < end && *p >= '0' && *p <= '9') t = t * 10 + (*p++ - '0');
    id->time = t;
    while(p < end && *p == ' ') p++;
    size_t k = 0;
    while(p < end && k < sizeof(id->tz) - 1 && *p != ' ') id->tz[k++] = *p++;
    if(k) id->tz[k] = '\0';
    return true;
}

/* Walk the header lines. For each header calls cb(key, keylen, value, valuelen).
 * Returns the message start. */
typedef void (*HdrCb)(void* ctx, const char* k, size_t kl, const char* v, size_t vl);

static const char* parse_headers(const uint8_t* data, size_t len, HdrCb cb, void* ctx, const char** msg_end) {
    const char* p = (const char*)data;
    const char* end = p + len;
    while(p < end) {
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        const char* le = nl ? nl : end;
        if(le == p) { /* empty line: message follows */
            p = nl ? nl + 1 : end;
            break;
        }
        if(*p != ' ') {
            const char* sp = memchr(p, ' ', (size_t)(le - p));
            if(sp)
                cb(ctx, p, (size_t)(sp - p), sp + 1, (size_t)(le - sp - 1));
        }
        p = nl ? nl + 1 : end;
    }
    *msg_end = end;
    return p;
}

typedef struct {
    HubGitCommit* c;
    bool has_tree;
} CommitCtx;

static void commit_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    CommitCtx* cc = ctx;
    HubGitCommit* c = cc->c;
    char hex[41];
    if(kl == 4 && memcmp(k, "tree", 4) == 0 && vl == 40) {
        memcpy(hex, v, 40);
        hex[40] = '\0';
        cc->has_tree = hub_oid_from_hex(hex, &c->tree);
    } else if(kl == 6 && memcmp(k, "parent", 6) == 0 && vl == 40) {
        if(c->nparents < HUB_GIT_MAX_PARENTS) {
            memcpy(hex, v, 40);
            hex[40] = '\0';
            if(hub_oid_from_hex(hex, &c->parent[c->nparents])) c->nparents++;
        } else {
            c->nparents++;
        }
    } else if(kl == 6 && memcmp(k, "author", 6) == 0) {
        parse_ident(v, vl, &c->author);
    } else if(kl == 9 && memcmp(k, "committer", 9) == 0) {
        parse_ident(v, vl, &c->committer);
    }
}

int hub_git_parse_commit(const uint8_t* data, size_t len, HubGitCommit* c) {
    memset(c, 0, sizeof(*c));
    strcpy(c->author.tz, "+0000");
    strcpy(c->committer.tz, "+0000");
    CommitCtx cc = {c, false};
    const char* end;
    const char* m = parse_headers(data, len, commit_hdr, &cc, &end);
    if(!cc.has_tree) return HUB_GIT_ERR_CORRUPT;
    c->msg = m;
    c->msg_len = (size_t)(end - m);
    return HUB_GIT_OK;
}

typedef struct {
    HubGitTag* t;
    bool has_obj;
} TagCtx;

static void tag_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    TagCtx* tc = ctx;
    HubGitTag* t = tc->t;
    char hex[41];
    if(kl == 6 && memcmp(k, "object", 6) == 0 && vl == 40) {
        memcpy(hex, v, 40);
        hex[40] = '\0';
        tc->has_obj = hub_oid_from_hex(hex, &t->object);
    } else if(kl == 4 && memcmp(k, "type", 4) == 0) {
        t->type = (uint8_t)type_from_name(v, vl);
    } else if(kl == 3 && memcmp(k, "tag", 3) == 0) {
        t->tag = v;
        t->tag_len = vl;
    } else if(kl == 6 && memcmp(k, "tagger", 6) == 0) {
        parse_ident(v, vl, &t->tagger);
        t->has_tagger = true;
    }
}

int hub_git_parse_tag(const uint8_t* data, size_t len, HubGitTag* t) {
    memset(t, 0, sizeof(*t));
    TagCtx tc = {t, false};
    const char* end;
    const char* m = parse_headers(data, len, tag_hdr, &tc, &end);
    if(!tc.has_obj) return HUB_GIT_ERR_CORRUPT;
    t->msg = m;
    t->msg_len = (size_t)(end - m);
    return HUB_GIT_OK;
}

void hub_tree_iter_init(HubTreeIter* it, const uint8_t* data, size_t len) {
    it->p = data;
    it->end = data + len;
}

int hub_tree_next(HubTreeIter* it, HubTreeEntry* e) {
    const uint8_t* p = it->p;
    if(p >= it->end) return 0;
    uint32_t mode = 0;
    const uint8_t* q = p;
    while(q < it->end && *q != ' ') {
        if(*q < '0' || *q > '7' || mode > 0x1FFFFFFu) return HUB_GIT_ERR_CORRUPT;
        mode = mode * 8 + (*q - '0');
        q++;
    }
    if(q == p || q >= it->end) return HUB_GIT_ERR_CORRUPT;
    q++;
    const uint8_t* name = q;
    while(q < it->end && *q != '\0') q++;
    if(q >= it->end || q == name || (size_t)(it->end - q) < 21) return HUB_GIT_ERR_CORRUPT;
    e->mode = mode;
    e->name = (const char*)name;
    e->name_len = (size_t)(q - name);
    e->oid = (const HubOid*)(q + 1);
    it->p = q + 21;
    return 1;
}

const char* hub_tree_mode_type(uint32_t mode) {
    switch(mode & 0170000) {
    case 0040000:
        return "tree";
    case 0120000:
        return "link";
    case 0160000:
        return "commit";
    default:
        return "blob";
    }
}

int hub_git_tree_find(
    HubGit* g,
    const HubOid* tree,
    const char* path,
    HubOid* oid,
    uint32_t* mode) {
    HubOid cur = *tree;
    uint32_t cur_mode = 0040000;
    const char* p = path;
    for(;;) {
        while(*p == '/') p++;
        if(*p == '\0') break;
        const char* e = p;
        while(*e && *e != '/') e++;
        size_t cl = (size_t)(e - p);
        if((cl == 1 && p[0] == '.') || (cl == 2 && p[0] == '.' && p[1] == '.'))
            return HUB_GIT_ERR_BADARG;
        if((cur_mode & 0170000) != 0040000) return HUB_GIT_ERR_NOTFOUND;

        uint8_t type;
        uint8_t* data;
        size_t size;
        int r = hub_git_read(g, &cur, &type, &data, &size);
        if(r) return r;
        if(type != HUB_OBJ_TREE) {
            free(data);
            return HUB_GIT_ERR_CORRUPT;
        }
        HubTreeIter it;
        HubTreeEntry te;
        hub_tree_iter_init(&it, data, size);
        bool found = false;
        while((r = hub_tree_next(&it, &te)) > 0) {
            if(te.name_len == cl && memcmp(te.name, p, cl) == 0) {
                cur = *te.oid;
                cur_mode = te.mode;
                found = true;
                break;
            }
        }
        free(data);
        if(r < 0) return r;
        if(!found) return HUB_GIT_ERR_NOTFOUND;
        p = e;
    }
    *oid = cur;
    if(mode) *mode = cur_mode;
    return HUB_GIT_OK;
}

/* ---------- commit graph ---------- */

static bool graph_open(HubGit* g) {
    if(g->gstate) return g->gstate == 1;
    g->gstate = 2;
    snprintf(g->path, sizeof(g->path), "%s/hub/graph.bin", g->base);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return false;
    uint8_t* b = g->buf;
    uint32_t n = 0;
    if(read_at(f, 0, b, HUB_GRAPH_HDR) && memcmp(b, "HGRF\0\0\0\1", 8) == 0) {
        n = be32(b + 8);
        uint64_t want = HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * n + 20ull * be32(b + 12);
        if(n == 0 || hub_gitio_size(f) != want) n = 0;
    }
    if(n == 0) {
        hub_gitio_close(f);
        return false;
    }
    g->gf = f;
    g->gcount = n;
    g->gstate = 1;
    g->fan = malloc(1024); /* one read per lookup instead of two */
    if(g->fan && !read_at(f, HUB_GRAPH_HDR, g->fan, 1024)) {
        free(g->fan);
        g->fan = NULL;
    }
    return true;
}

/* 1 = rec holds the record, 0 = not in the graph */
static int graph_find(HubGit* g, const HubOid* oid, uint8_t* rec) {
    if(!graph_open(g)) return 0;
    uint8_t* b = g->buf;
    uint32_t lo = 0, hi;
    unsigned c = oid->b[0];
    if(g->fan) {
        lo = c > 0 ? be32(g->fan + 4 * (c - 1)) : 0;
        hi = be32(g->fan + 4 * c);
    } else if(c > 0) {
        if(!read_at(g->gf, HUB_GRAPH_HDR + 4 * (uint64_t)(c - 1), b, 8)) return 0;
        lo = be32(b);
        hi = be32(b + 4);
    } else {
        if(!read_at(g->gf, HUB_GRAPH_HDR, b, 4)) return 0;
        hi = be32(b);
    }
    if(lo > hi || hi > g->gcount) return 0;
    while(hi - lo > 3) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(!read_at(g->gf, HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * mid, b, 20)) return 0;
        int cmp = memcmp(oid->b, b, 20);
        if(cmp == 0) {
            lo = mid;
            hi = mid + 1;
        } else if(cmp < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if(hi == lo || !read_at(g->gf, HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * lo, b, (size_t)HUB_GRAPH_REC * (hi - lo)))
        return 0;
    for(uint32_t k = 0; k < hi - lo; k++) {
        if(memcmp(oid->b, b + HUB_GRAPH_REC * k, 20) == 0) {
            memmove(rec, b + HUB_GRAPH_REC * k, HUB_GRAPH_REC);
            return 1;
        }
    }
    return 0;
}

void hub_git_graph_ignore(HubGit* g) {
    g->quiet = true;
}

static void meta_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    HubCommitMeta* m = ctx;
    char hex[41];
    if(kl == 6 && memcmp(k, "parent", 6) == 0 && vl == 40) {
        if(m->nparents < HUB_GIT_MAX_PARENTS) {
            memcpy(hex, v, 40);
            hex[40] = '\0';
            if(hub_oid_from_hex(hex, &m->parent[m->nparents])) m->nparents++;
        } else {
            m->nparents++;
        }
    } else if(kl == 9 && memcmp(k, "committer", 9) == 0) {
        HubGitIdent id;
        parse_ident(v, vl, &id);
        m->time = (uint32_t)id.time;
    }
}

int hub_git_commit_meta(HubGit* g, const HubOid* oid, HubCommitMeta* out) {
    uint8_t rec[HUB_GRAPH_REC];
    memset(out, 0, sizeof(*out));
    bool known = graph_find(g, oid, rec);
    if(known) {
        uint32_t fl = be32(rec + 64);
        if(!(fl & 1)) {
            unsigned np = (fl >> 8) & 0xff;
            if(np <= 2) {
                out->nparents = np;
                out->time = be32(rec + 20);
                memcpy(out->parent[0].b, rec + 24, 20);
                memcpy(out->parent[1].b, rec + 44, 20);
                out->cached = true;
                return HUB_GIT_OK;
            }
        }
    }
    if(!known) g->stale = true;
    uint8_t type;
    uint8_t* data;
    size_t size;
    int r = hub_git_read(g, oid, &type, &data, &size);
    if(r) return r;
    out->cached = known; /* cached: the id is in the graph (parents may come from the object) */
    if(type != HUB_OBJ_COMMIT) {
        free(data);
        return HUB_GIT_ERR_CORRUPT;
    }
    const char* end;
    parse_headers(data, size, meta_hdr, out, &end);
    free(data);
    return HUB_GIT_OK;
}

/* ---------- tree walk ---------- */

typedef struct {
    HubTreeCb cb;
    void* ctx;
    uint32_t mode;
    uint8_t st; /* 0 mode digits, 1 name, 2 id */
    uint8_t k;
    uint8_t nl;
    bool bad, stopped;
    char name[256];
    HubOid id;
} TreeWalk;

static bool tw_sink(void* c, const uint8_t* p, size_t n) {
    TreeWalk* w = c;
    for(size_t i = 0; i < n; i++) {
        uint8_t b = p[i];
        if(w->st == 0) {
            if(b >= '0' && b <= '7' && w->k < 8) {
                w->mode = w->mode * 8 + (b - '0');
                w->k++;
            } else if(b == ' ' && w->k > 0) {
                w->st = 1;
                w->nl = 0;
            } else {
                w->bad = true;
                return false;
            }
        } else if(w->st == 1) {
            if(b == 0 && w->nl > 0) {
                w->name[w->nl] = '\0';
                w->st = 2;
                w->k = 0;
            } else if(b != 0 && w->nl < 255) {
                w->name[w->nl++] = (char)b;
            } else {
                w->bad = true;
                return false;
            }
        } else {
            w->id.b[w->k++] = b;
            if(w->k == 20) {
                if(!w->cb(w->ctx, w->mode, w->name, w->nl, &w->id)) {
                    w->stopped = true;
                    return false;
                }
                w->st = 0;
                w->k = 0;
                w->mode = 0;
            }
        }
    }
    return true;
}

int hub_git_tree_walk(HubGit* g, const HubOid* tree, HubTreeCb cb, void* ctx) {
    TreeWalk* w = calloc(1, sizeof(TreeWalk));
    if(!w) return HUB_GIT_ERR_NOMEM;
    w->cb = cb;
    w->ctx = ctx;
    uint8_t type;
    uint8_t* data;
    size_t size;
    int r = hub_git_read(g, tree, &type, &data, &size);
    if(r == HUB_GIT_OK) {
        if(type == HUB_OBJ_TREE) tw_sink(w, data, size);
        free(data);
        if(type != HUB_OBJ_TREE) r = HUB_GIT_ERR_BADARG;
    } else if(r == HUB_GIT_ERR_TOOLARGE) {
        r = hub_git_stream(g, tree, tw_sink, w, &type, &size);
        if(r == HUB_GIT_ERR_WRITE && !w->stopped) r = HUB_GIT_ERR_CORRUPT;
        if(r == HUB_GIT_OK && type != HUB_OBJ_TREE) r = HUB_GIT_ERR_BADARG;
    }
    if(r == HUB_GIT_OK) {
        if(w->stopped) r = HUB_GIT_ERR_WRITE;
        else if(w->bad || w->st != 0) r = HUB_GIT_ERR_CORRUPT;
    }
    free(w);
    return r;
}
