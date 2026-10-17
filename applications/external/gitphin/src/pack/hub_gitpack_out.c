/* Packer: pack output, loose object listing, entries, sorting, the .idx writer.
 * Part of hub_gitpack.c: included there, in this order, not built alone. */
#include "hub_gitpack_int.h"

static bool pack_out(void* c, const uint8_t* d, size_t n) {
    PackOut* p = c;
    if(hub_gitio_write(p->f, d, n) != n) {
        p->err = true;
        return false;
    }
    p->crc = hub_crc32(p->crc, d, n);
    p->total += (uint32_t)n;
    return true;
}

static bool is_hex(const char* s, size_t n) {
    for(size_t i = 0; i < n; i++) {
        char c = s[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

/* ---------- listing ---------- */

/* Collect the loose ids of whole folders, from folder `from`, up to cap ids.
 * A folder is taken whole or not at all (the next folder to look at is *next),
 * so a job never lists an object twice. A first folder with more than cap
 * objects is cut at cap: *big is set and *next stays on that folder. */
static void list_folders(
    void* io,
    const char* repo,
    char* path,
    uint8_t* ids,
    unsigned cap,
    unsigned from,
    unsigned* n_out,
    unsigned* next,
    bool* big) {
    char sub[64];
    bool isdir;
    unsigned n = 0, d = from;
    *big = false;
    for(; d < 256; d++) {
        snprintf(path, PATHLEN, "%s/objects/%02x", repo, d);
        HubGitDir* s = hub_gitio_dir_open(io, path);
        if(!s) continue;
        unsigned base = n;
        bool over = false;
        while(hub_gitio_dir_read(s, sub, sizeof sub, &isdir)) {
            if(isdir || strlen(sub) != 38 || !is_hex(sub, 38)) continue;
            if(n >= cap) {
                over = true;
                break;
            }
            char hex[72];
            snprintf(hex, sizeof hex, "%02x%s", d, sub);
            HubOid o;
            if(hub_oid_from_hex(hex, &o)) {
                memcpy(ids + 20 * (size_t)n, o.b, 20);
                n++;
            }
        }
        hub_gitio_dir_close(s);
        if(over) {
            if(base == 0)
                *big = true;
            else
                n = base;
            break; /* stay on this folder */
        }
    }
    *n_out = n;
    *next = d;
}

__attribute__((noinline)) static void
    clean_stale(void* io, const char* repo, char* path, char* full) {
    char name[64];
    snprintf(path, PATHLEN, "%s/objects/pack", repo);
    for(int round = 0; round < 8; round++) {
        HubGitDir* d = hub_gitio_dir_open(io, path);
        if(!d) return;
        bool isdir, found = false;
        while(hub_gitio_dir_read(d, name, sizeof name, &isdir)) {
            if(!isdir && strncmp(name, "tmp_pack_", 9) == 0) {
                found = true;
                break;
            }
        }
        hub_gitio_dir_close(d);
        if(!found) return;
        snprintf(full, PATHLEN, "%s/%s", path, name);
        hub_gitio_remove(io, full);
    }
}

/* ---------- entries ---------- */

static bool put_entry_header(Run* r) {
    uint8_t h[12];
    size_t n = 0;
    size_t size = r->size;
    h[n++] = (uint8_t)((r->type << 4) | (size & 15) | (size >> 4 ? 0x80 : 0));
    size >>= 4;
    while(size) {
        h[n++] = (uint8_t)((size & 0x7f) | (size >> 7 ? 0x80 : 0));
        size >>= 7;
    }
    r->po.crc = 0;
    return pack_out(&r->po, h, n);
}

/* Small objects: read into RAM, then compress. The inflate state is freed
 * before the compressor is allocated. */
static int pack_small(HubGit* g, Run* r, const HubOid* oid) {
    uint8_t t;
    uint8_t* data;
    size_t sz;
    int e = hub_git_read(g, oid, &t, &data, &sz);
    if(e) return e; /* TOOLARGE: nothing was written yet */
    r->type = t;
    r->size = sz;
    if(!put_entry_header(r)) {
        hub_git_free(data);
        return HUB_GIT_ERR_IO;
    }
    HubDeflate* df = hub_deflate_new(pack_out, &r->po);
    if(!df) {
        hub_git_free(data);
        return HUB_GIT_ERR_NOMEM;
    }
    bool ok = hub_deflate_write(df, data, sz) && hub_deflate_finish(df);
    hub_deflate_free(df);
    hub_git_free(data);
    return ok ? HUB_GIT_OK : HUB_GIT_ERR_IO;
}

/* Big objects are compressed while they stream, with the small-window
 * compressor (about 8 KB), into a scratch file. The result goes into the pack
 * only when it is at most 1.25 times as big as the loose file it comes from.
 * Otherwise (data with far repeats, which a 2 KB window cannot find) the object
 * stays loose: nothing is lost, and later reads use the SD-backed window. */
#define BIG_DEFLATE_MAX (1024 * 1024)
#define PACK_SKIP       1 /* internal: object left loose */

static bool scratch_out(void* c, const uint8_t* d, size_t n) {
    Run* r = c;
    if(r->so.total + n > r->limit) {
        r->over = true;
        return false;
    }
    if(hub_gitio_write(r->so.f, d, n) != n) {
        r->so.err = true;
        return false;
    }
    r->so.total += (uint32_t)n;
    return true;
}

static bool big_sink(void* c, const uint8_t* d, size_t n) {
    Run* r = c;
    if(!r->df) {
        if(r->size > BIG_DEFLATE_MAX) {
            r->over = true;
            return false;
        }
        r->df = hub_deflate_new(scratch_out, r);
        if(!r->df) {
            r->nomem = true;
            return false;
        }
    }
    return hub_deflate_write(r->df, d, n);
}

static int pack_big(void* io, const char* repo, HubGit* g, Run* r, const HubOid* oid) {
    char hex[41];
    hub_oid_to_hex(oid, hex);
    snprintf(r->pc, PATHLEN, "%s/objects/%.2s/%s", repo, hex, hex + 2);
    HubGitFile* lf = hub_gitio_open(io, r->pc);
    if(!lf) return HUB_GIT_ERR_NOTFOUND;
    uint64_t lsz = hub_gitio_size(lf);
    hub_gitio_close(lf);
    r->limit = lsz + lsz / 4;
    r->over = r->nomem = false;
    r->so.total = 0;
    r->so.err = false;
    snprintf(r->pc, PATHLEN, "%s/objects/pack/tmp_pack_o", repo);
    r->so.f = hub_gitio_create(io, r->pc);
    if(!r->so.f) return HUB_GIT_ERR_IO;
    int e = hub_git_stream(g, oid, big_sink, r, &r->type, &r->size);
    if(e == HUB_GIT_ERR_WRITE)
        e = r->over ? PACK_SKIP : (r->nomem ? HUB_GIT_ERR_NOMEM : HUB_GIT_ERR_IO);
    if(!e && !(r->df && hub_deflate_finish(r->df))) e = r->over ? PACK_SKIP : HUB_GIT_ERR_IO;
    if(r->df) {
        hub_deflate_free(r->df);
        r->df = NULL;
    }
    hub_gitio_close(r->so.f);
    r->so.f = NULL;
    if(!e && r->so.err) e = HUB_GIT_ERR_IO;
    if(!e) { /* accepted: entry header, then the scratch bytes */
        HubGitFile* f = hub_gitio_open(io, r->pc);
        if(!f || !put_entry_header(r)) e = HUB_GIT_ERR_IO;
        for(uint32_t left = r->so.total; !e && left;) {
            uint32_t k = left < sizeof r->buf ? left : (uint32_t)sizeof r->buf;
            if(hub_gitio_read(f, r->buf, k) != k || !pack_out(&r->po, r->buf, k))
                e = HUB_GIT_ERR_IO;
            left -= k;
        }
        if(f) hub_gitio_close(f);
    }
    hub_gitio_remove(io, r->pc);
    return e;
}

/* ---------- sorting (heapsort of indexes, no qsort) ---------- */

static bool id_less(const uint8_t* ids, uint16_t a, uint16_t b) {
    return memcmp(ids + 20 * (size_t)a, ids + 20 * (size_t)b, 20) < 0;
}

static void sift(const uint8_t* ids, uint16_t* o, unsigned start, unsigned end) {
    unsigned root = start;
    while(2 * root + 1 < end) {
        unsigned child = 2 * root + 1;
        if(child + 1 < end && id_less(ids, o[child], o[child + 1])) child++;
        if(id_less(ids, o[root], o[child])) {
            uint16_t t = o[root];
            o[root] = o[child];
            o[child] = t;
            root = child;
        } else {
            return;
        }
    }
}

static void sort_order(const uint8_t* ids, uint16_t* o, unsigned n) {
    for(unsigned i = 0; i < n; i++)
        o[i] = (uint16_t)i;
    if(n < 2) return;
    for(unsigned s = n / 2; s > 0; s--)
        sift(ids, o, s - 1, n);
    for(unsigned e = n - 1; e > 0; e--) {
        uint16_t t = o[0];
        o[0] = o[e];
        o[e] = t;
        sift(ids, o, 0, e);
    }
}

/* ---------- idx ---------- */

typedef struct {
    HubGitFile* f;
    HubSha1 sha;
    uint8_t buf[IDX_BUF];
    size_t n;
    bool err;
} IdxOut;

static void idx_flush(IdxOut* o) {
    if(o->n && !o->err) {
        if(hub_gitio_write(o->f, o->buf, o->n) != o->n) o->err = true;
        hub_sha1_update(&o->sha, o->buf, o->n);
    }
    o->n = 0;
}

static void idx_put(IdxOut* o, const void* d, size_t n) {
    const uint8_t* p = d;
    while(n) {
        size_t k = IDX_BUF - o->n;
        if(k > n) k = n;
        memcpy(o->buf + o->n, p, k);
        o->n += k;
        p += k;
        n -= k;
        if(o->n == IDX_BUF) idx_flush(o);
    }
}

static void idx_be32(IdxOut* o, uint32_t v) {
    uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    idx_put(o, b, 4);
}

static int write_idx(
    void* io,
    const char* path,
    const uint8_t* ids,
    const uint16_t* order,
    const uint32_t* offs,
    const uint32_t* crcs,
    unsigned n,
    const uint8_t pack_sha[20]) {
    IdxOut* o = malloc(sizeof(IdxOut));
    if(!o) return HUB_GIT_ERR_NOMEM;
    o->f = hub_gitio_create(io, path);
    if(!o->f) {
        free(o);
        return HUB_GIT_ERR_IO;
    }
    hub_sha1_init(&o->sha);
    o->n = 0;
    o->err = false;
    static const uint8_t magic[8] = {0xff, 't', 'O', 'c', 0, 0, 0, 2};
    idx_put(o, magic, 8);
    unsigned k = 0;
    for(unsigned b = 0; b < 256; b++) {
        while(k < n && ids[20 * (size_t)order[k]] <= b)
            k++;
        idx_be32(o, k);
    }
    for(unsigned i = 0; i < n; i++)
        idx_put(o, ids + 20 * (size_t)order[i], 20);
    for(unsigned i = 0; i < n; i++)
        idx_be32(o, crcs[order[i]]);
    for(unsigned i = 0; i < n; i++)
        idx_be32(o, offs[order[i]]);
    idx_put(o, pack_sha, 20);
    idx_flush(o);
    uint8_t sum[20];
    hub_sha1_final(&o->sha, sum);
    if(!o->err && hub_gitio_write(o->f, sum, 20) != 20) o->err = true;
    hub_gitio_close(o->f);
    int r = o->err ? HUB_GIT_ERR_IO : HUB_GIT_OK;
    free(o);
    return r;
}
