/* Object reader: hex ids, the handle, .idx lookup, pack entries, delta chains.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

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
    while(len > 1 && g->base[len - 1] == '/')
        g->base[--len] = '\0';

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
