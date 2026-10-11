#include "hub_gitpack.h"

#include "hub_deflate.h"
#include "hub_gitdocutil.h"
#include "hub_gitio.h"
#include "hub_gitobj.h"
#include "hub_sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define PATHLEN (HUB_GIT_PATH_MAX + 50)
#define IDX_BUF 512

typedef struct {
    HubGitFile* f;
    uint32_t crc;
    uint32_t total;
    bool err;
} PackOut;

typedef struct {
    PackOut po;
    uint8_t type;
    size_t size;
    HubDeflate* df; /* compressor of a big object */
    PackOut so;     /* scratch file of a big object (tmp_pack_o) */
    uint64_t limit; /* the scratch file may not grow past this: the object is left loose */
    bool nomem, over;
    char pa[PATHLEN], pb[PATHLEN], pc[PATHLEN];
    uint8_t buf[512];
} Run;

/* State of the open pack between slices. The objects stay loose (and readable)
 * until the pack is finished, so dropping this (a restart) loses only time:
 * the next slice deletes the tmp_pack_* files and starts again. */
#define JOB_MAX 384 /* finish the pack when it holds this many objects */
#define TAB_REC 28  /* tmp_pack_t record: id[20], offset u32, crc u32 */
typedef struct {
    uint8_t phase; /* 1 append, 2 hash, 3 index, 4 delete loose */
    bool patched;
    int16_t last_dir;
    uint32_t count, total, hpos, dpos;
    HubSha1 sha;
    uint8_t sum[20];
    char name[46];
} PackJob;

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
            if(base == 0) *big = true;
            else n = base;
            break; /* stay on this folder */
        }
    }
    *n_out = n;
    *next = d;
}

__attribute__((noinline)) static void clean_stale(void* io, const char* repo, char* path, char* full) {
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
    if(e == HUB_GIT_ERR_WRITE) e = r->over ? PACK_SKIP : (r->nomem ? HUB_GIT_ERR_NOMEM : HUB_GIT_ERR_IO);
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
            if(hub_gitio_read(f, r->buf, k) != k || !pack_out(&r->po, r->buf, k)) e = HUB_GIT_ERR_IO;
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
    for(unsigned i = 0; i < n; i++) o[i] = (uint16_t)i;
    if(n < 2) return;
    for(unsigned s = n / 2; s > 0; s--) sift(ids, o, s - 1, n);
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
        while(k < n && ids[20 * (size_t)order[k]] <= b) k++;
        idx_be32(o, k);
    }
    for(unsigned i = 0; i < n; i++) idx_put(o, ids + 20 * (size_t)order[i], 20);
    for(unsigned i = 0; i < n; i++) idx_be32(o, crcs[order[i]]);
    for(unsigned i = 0; i < n; i++) idx_be32(o, offs[order[i]]);
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

/* ---------- main ---------- */

static void rm_loose(void* io, const char* repo, char* p, const uint8_t* id) {
    char hex[41];
    HubOid o;
    memcpy(o.b, id, 20);
    hub_oid_to_hex(&o, hex);
    snprintf(p, PATHLEN, "%s/objects/%.2s/%s", repo, hex, hex + 2);
    hub_gitio_remove(io, p);
}

static void tmp_path(Run* r, const char* repo, char* out, const char* leaf) {
    (void)r;
    snprintf(out, PATHLEN, "%s/objects/pack/%s", repo, leaf);
}

static void job_drop(void* io, const char* repo, Run* run, HubGitPackCursor* cur) {
    static const char* const leaf[4] = {"tmp_pack_p", "tmp_pack_i", "tmp_pack_t", "tmp_pack_o"};
    for(int i = 0; i < 4; i++) {
        tmp_path(run, repo, run->pc, leaf[i]);
        hub_gitio_remove(io, run->pc);
    }
    if(cur) {
        free(cur->job);
        cur->job = NULL;
        cur->dir = 0;
    }
}

/* ---- append: pack whole folders of loose objects onto the open pack ---- */

static int job_append(
    void* io,
    const char* repo,
    Run* run,
    HubGitPackCursor* cur,
    unsigned max_objects,
    size_t ram_limit,
    HubGitPackProgress progress,
    HubGitPackStop stop,
    void* ctx,
    HubGitPackResult* res,
    bool oneshot,
    bool* finish) {
    PackJob* job = cur->job;
    *finish = false;
    if(job && job->count >= JOB_MAX) { /* full: close it before taking more */
        *finish = true;
        job->phase = 2;
        res->more = true;
        return HUB_GIT_OK;
    }
    uint8_t* ids = malloc(max_objects * 20u);
    if(!ids) return HUB_GIT_ERR_NOMEM;
    unsigned n = 0, next = 0;
    bool big = false;
    list_folders(io, repo, run->pa, ids, max_objects, cur->dir, &n, &next, &big);
    res->more = next < 256;
    cur->dir = (uint16_t)next;
    if(n == 0 || (big && job && job->count > 0)) {
        /* nothing left to scan, or a big folder that needs its own pack: close the open pack first */
        free(ids);
        if(job && job->count > 0) {
            *finish = true;
            res->more = true;
        } else if(job) {
            job_drop(io, repo, run, cur);
        }
        return HUB_GIT_OK;
    }
    uint32_t* offs = malloc(n * 8u);
    uint32_t* crcs = offs ? offs + n : NULL;
    HubGit* g = hub_git_open(io, repo);
    int r = HUB_GIT_OK;
    bool fresh = false;
    if(!offs || !g) {
        r = HUB_GIT_ERR_NOMEM;
        goto out;
    }
    hub_git_set_limit(g, ram_limit);
    tmp_path(run, repo, run->pa, "tmp_pack_p");
    if(!job) {
        job = calloc(1, sizeof(PackJob));
        if(!job) {
            r = HUB_GIT_ERR_NOMEM;
            goto out;
        }
        cur->job = job;
        fresh = true;
        job->phase = 1;
        job->last_dir = -1;
        run->po.f = hub_gitio_create(io, run->pa);
        tmp_path(run, repo, run->pb, "tmp_pack_t");
        HubGitFile* t = hub_gitio_create(io, run->pb);
        if(!run->po.f || !t) {
            if(t) hub_gitio_close(t);
            r = HUB_GIT_ERR_IO;
            goto out;
        }
        hub_gitio_close(t);
        static const uint8_t hdr[12] = {'P', 'A', 'C', 'K', 0, 0, 0, 2, 0, 0, 0, 0};
        pack_out(&run->po, hdr, 12);
    } else {
        run->po.f = hub_gitio_open_rw(io, run->pa);
        if(!run->po.f || hub_gitio_size(run->po.f) != job->total || !hub_gitio_seek(run->po.f, job->total)) {
            r = HUB_GIT_ERR_IO; /* the temp file changed: start over */
            cur->job = job; /* dropped below with the files */
            goto out;
        }
        run->po.total = job->total;
    }

    unsigned done = 0, kept = 0;
    while(done < n && !run->po.err) {
        unsigned end = done;
        while(end < n && ids[20 * (size_t)end] == ids[20 * (size_t)done]) end++;
        if(stop && stop(ctx)) {
            res->stopped = true;
            break;
        }
        for(; done < end; done++) {
            HubOid oid;
            memcpy(oid.b, ids + 20 * (size_t)done, 20);
            uint32_t off = run->po.total;
            r = pack_small(g, run, &oid);
            if(r == HUB_GIT_ERR_TOOLARGE) r = pack_big(io, repo, g, run, &oid);
            if(r == PACK_SKIP) { /* stays loose; the pack has no part of it */
                r = HUB_GIT_OK;
                continue;
            }
            if(r) goto out;
            if(run->po.total > 0x7fffffffu) {
                r = HUB_GIT_ERR_TOOLARGE;
                goto out;
            }
            memmove(ids + 20 * (size_t)kept, ids + 20 * (size_t)done, 20);
            offs[kept] = off;
            crcs[kept] = run->po.crc;
            kept++;
            if(progress) progress(ctx, kept, n);
        }
    }
    if(run->po.err) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    hub_gitio_close(run->po.f);
    run->po.f = NULL;
    uint16_t newdir = done == n ? (uint16_t)(big && kept == 0 ? next + 1 : next) : (uint16_t)ids[20 * (size_t)done];
    if(kept > 0) {
        /* the table of this slice goes to the end of tmp_pack_t */
        tmp_path(run, repo, run->pb, "tmp_pack_t");
        HubGitFile* t = hub_gitio_open_rw(io, run->pb);
        if(!t || hub_gitio_size(t) != (uint64_t)job->count * TAB_REC || !hub_gitio_seek(t, (uint64_t)job->count * TAB_REC)) {
            if(t) hub_gitio_close(t);
            r = HUB_GIT_ERR_IO;
            goto out;
        }
        for(unsigned i = 0; i < kept && r == HUB_GIT_OK; i++) {
            uint8_t rec[TAB_REC];
            memcpy(rec, ids + 20 * (size_t)i, 20);
            memcpy(rec + 20, &offs[i], 4);
            memcpy(rec + 24, &crcs[i], 4);
            if(hub_gitio_write(t, rec, TAB_REC) != TAB_REC) r = HUB_GIT_ERR_IO;
        }
        hub_gitio_close(t);
        if(r) goto out;
        job->count += kept;
        job->total = run->po.total;
        res->packed = kept;
    } else if(fresh) {
        job_drop(io, repo, run, cur); /* stopped, or every object stayed loose: nothing to keep */
        cur->dir = newdir;
        res->more = done < n || newdir < 256;
        goto out;
    }
    cur->dir = newdir;
    res->remaining = n - done;
    res->more = done < n || next < 256 || big;
    *finish = job->count > 0 && done == n && (next >= 256 || big || oneshot || job->count >= JOB_MAX);
    if(*finish) job->phase = 2;
out:
    if(r) {
        if(run->po.f) hub_gitio_close(run->po.f);
        run->po.f = NULL;
        job_drop(io, repo, run, cur);
        res->packed = 0;
        res->more = false;
    }
    if(g) hub_git_close(g);
    free(offs);
    free(ids);
    return r;
}

/* ---- finish: hash, index, rename, delete the loose files; every step resumes ---- */

static int job_hash(void* io, const char* repo, Run* run, PackJob* job, HubGitPackStop stop, void* ctx, bool* stopped) {
    tmp_path(run, repo, run->pa, "tmp_pack_p");
    HubGitFile* f = hub_gitio_open_rw(io, run->pa);
    if(!f || hub_gitio_size(f) != job->total) {
        if(f) hub_gitio_close(f);
        return HUB_GIT_ERR_IO;
    }
    if(!job->patched) {
        uint32_t d = job->count;
        uint8_t cnt[4] = {(uint8_t)(d >> 24), (uint8_t)(d >> 16), (uint8_t)(d >> 8), (uint8_t)d};
        if(!hub_gitio_seek(f, 8) || hub_gitio_write(f, cnt, 4) != 4) {
            hub_gitio_close(f);
            return HUB_GIT_ERR_IO;
        }
        hub_sha1_init(&job->sha);
        job->hpos = 0;
        job->patched = true;
    }
    int r = HUB_GIT_OK;
    while(job->hpos < job->total) {
        if((job->hpos & 2047) == 0 && stop && stop(ctx)) {
            *stopped = true;
            break;
        }
        uint32_t k = job->total - job->hpos;
        if(k > sizeof run->buf) k = sizeof run->buf;
        if(!hub_gitio_seek(f, job->hpos) || hub_gitio_read(f, run->buf, k) != k) {
            r = HUB_GIT_ERR_IO;
            break;
        }
        hub_sha1_update(&job->sha, run->buf, k);
        job->hpos += k;
    }
    if(r == HUB_GIT_OK && !*stopped) {
        hub_sha1_final(&job->sha, job->sum);
        if(!hub_gitio_seek(f, job->total) || hub_gitio_write(f, job->sum, 20) != 20) {
            r = HUB_GIT_ERR_IO;
        } else {
            job->total += 20;
            job->phase = 3;
        }
    }
    hub_gitio_close(f);
    return r;
}

static int job_index(void* io, const char* repo, Run* run, PackJob* job, HubGitPackResult* res) {
    unsigned n = job->count;
    uint8_t* ids = malloc(n * 20u);
    uint32_t* offs = malloc(n * 8u + n * 2u);
    if(!ids || !offs) {
        free(ids);
        free(offs);
        return HUB_GIT_ERR_NOMEM;
    }
    uint32_t* crcs = offs + n;
    uint16_t* order = (uint16_t*)(offs + 2 * (size_t)n);
    tmp_path(run, repo, run->pb, "tmp_pack_t");
    HubGitFile* t = hub_gitio_open(io, run->pb);
    int r = t && hub_gitio_size(t) == (uint64_t)n * TAB_REC ? HUB_GIT_OK : HUB_GIT_ERR_IO;
    for(unsigned i = 0; i < n && r == HUB_GIT_OK; i++) {
        uint8_t rec[TAB_REC];
        if(hub_gitio_read(t, rec, TAB_REC) != TAB_REC) {
            r = HUB_GIT_ERR_IO;
            break;
        }
        memcpy(ids + 20 * (size_t)i, rec, 20);
        memcpy(&offs[i], rec + 20, 4);
        memcpy(&crcs[i], rec + 24, 4);
    }
    if(t) hub_gitio_close(t);
    if(r == HUB_GIT_OK) {
        sort_order(ids, order, n);
        tmp_path(run, repo, run->pb, "tmp_pack_i");
        r = write_idx(io, run->pb, ids, order, offs, crcs, n, job->sum);
    }
    free(ids);
    free(offs);
    if(r) return r;
    char hex[41];
    HubOid so;
    memcpy(so.b, job->sum, 20);
    hub_oid_to_hex(&so, hex);
    snprintf(job->name, sizeof job->name, "pack-%s", hex);
    tmp_path(run, repo, run->pa, "tmp_pack_p");
    snprintf(run->pc, PATHLEN, "%s/objects/pack/%s.pack", repo, job->name);
    if(!hub_gitio_rename(io, run->pa, run->pc)) return HUB_GIT_ERR_IO;
    tmp_path(run, repo, run->pb, "tmp_pack_i");
    snprintf(run->pc, PATHLEN, "%s/objects/pack/%s.idx", repo, job->name);
    if(!hub_gitio_rename(io, run->pb, run->pc)) return HUB_GIT_ERR_IO;
    job->phase = 4;
    job->dpos = 0;
    res->pack_bytes = job->total;
    return HUB_GIT_OK;
}

/* Both files are in place: delete the loose objects of the pack. Done is true at the end. */
static void job_delete(void* io, const char* repo, Run* run, PackJob* job, HubGitPackStop stop, void* ctx, bool* done) {
    tmp_path(run, repo, run->pb, "tmp_pack_t");
    HubGitFile* t = hub_gitio_open(io, run->pb);
    if(!t) {
        *done = true; /* nothing to delete from: leftover loose objects are harmless duplicates */
        return;
    }
    while(job->dpos < job->count) {
        if(stop && stop(ctx)) {
            hub_gitio_close(t);
            return;
        }
        unsigned k = job->count - job->dpos;
        if(k > 8) k = 8;
        if(!hub_gitio_seek(t, (uint64_t)job->dpos * TAB_REC) || hub_gitio_read(t, run->buf, k * TAB_REC) != k * TAB_REC) {
            job->dpos = job->count;
            break;
        }
        for(unsigned i = 0; i < k; i++) {
            const uint8_t* id = run->buf + TAB_REC * i;
            if(job->last_dir >= 0 && job->last_dir != id[0]) {
                snprintf(run->pc, PATHLEN, "%s/objects/%02x", repo, job->last_dir);
                hub_gitio_remove(io, run->pc); /* only succeeds when empty */
            }
            job->last_dir = id[0];
            rm_loose(io, repo, run->pc, id);
        }
        job->dpos += k;
    }
    hub_gitio_close(t);
    if(job->last_dir >= 0) {
        snprintf(run->pc, PATHLEN, "%s/objects/%02x", repo, job->last_dir);
        hub_gitio_remove(io, run->pc);
    }
    *done = true;
}

int hub_git_pack_loose(
    void* io,
    const char* repo,
    unsigned max_objects,
    size_t ram_limit,
    HubGitPackCursor* cursor,
    HubGitPackProgress progress,
    HubGitPackStop stop,
    void* ctx,
    HubGitPackResult* res) {
    memset(res, 0, sizeof *res);
    if(strlen(repo) > HUB_GIT_PATH_MAX - 40) return HUB_GIT_ERR_BADARG;
    if(max_objects == 0 || max_objects > HUB_GITPACK_MAX_OBJS) max_objects = HUB_GITPACK_MAX_OBJS;
    if(ram_limit == 0) ram_limit = HUB_GIT_MAX_OBJ;

    Run* run = calloc(1, sizeof(Run));
    if(!run) return HUB_GIT_ERR_NOMEM;
    HubGitPackCursor local = {0};
    bool oneshot = cursor == NULL; /* no cursor: this call must finish its pack */
    HubGitPackCursor* cur = cursor ? cursor : &local;
    snprintf(run->pa, PATHLEN, "%s/objects/pack", repo);
    hub_gitio_mkdir(io, run->pa);
    if(!cur->job) clean_stale(io, repo, run->pa, run->pc);

    int r = HUB_GIT_OK;
    bool finish = false;
    PackJob* job = cur->job;
    if(job && job->phase >= 2) {
        finish = true; /* a finish that was interrupted */
        res->more = true;
    } else {
        r = job_append(io, repo, run, cur, max_objects, ram_limit, progress, stop, ctx, res, oneshot, &finish);
        job = cur->job;
    }
    while(r == HUB_GIT_OK && finish && job) {
        bool stopped = false, done = false;
        if(job->phase == 2) {
            r = job_hash(io, repo, run, job, stop, ctx, &stopped);
        } else if(job->phase == 3) {
            r = job_index(io, repo, run, job, res);
        } else {
            job_delete(io, repo, run, job, stop, ctx, &done);
        }
        if(r) break;
        if(done) {
            snprintf(res->name, sizeof res->name, "%s", job->name);
            res->pack_bytes = job->total;
            res->finished = true;
            res->more = cur->dir < 256;
            free(cur->job);
            cur->job = NULL;
            tmp_path(run, repo, run->pc, "tmp_pack_t");
            hub_gitio_remove(io, run->pc);
            break;
        }
        if(stopped || (job->phase == 4 && stop && stop(ctx))) {
            res->stopped = true;
            res->more = true;
            break;
        }
    }
    if(r) { /* a real error: the loose objects are untouched, the job is dropped */
        job_drop(io, repo, run, cur);
        res->name[0] = '\0';
        res->packed = 0;
    }
    if(oneshot && local.job) { /* stopped in a one-shot call: nothing is kept */
        job_drop(io, repo, run, &local);
        res->name[0] = '\0';
    }
    free(run);
    return r;
}

/* ======================= merging small packs ======================= */

#define M_IDS 8  /* ids cached per source */
#define M_VALS 16 /* u32 values cached per source */
#define M_PATH (HUB_GIT_PATH_MAX + 80)
#define M_SMALL_DEFAULT (1024u * 1024u)
#define IDX_TABLE_OFF 1032u /* 8 header + 1024 fanout */

typedef struct {
    HubGitFile* f;
    uint32_t nobj, next;
    uint32_t ibase, icnt, vbase, vcnt;
    uint8_t ib[20 * M_IDS];
    uint8_t vb[4 * M_VALS];
} MSrc;

typedef struct {
    MSrc s[HUB_MERGE_MAX_SRC];
    unsigned k;
    bool err;
} Merge;

typedef struct {
    Merge m;
    char p1[M_PATH], p2[M_PATH];
    uint8_t buf[1024];
} MCtx;

static bool m_read_at(HubGitFile* f, uint64_t off, void* b, size_t n) {
    return hub_gitio_seek(f, off) && hub_gitio_read(f, b, n) == n;
}

static const uint8_t* ms_id(Merge* m, MSrc* s, uint32_t ix) {
    if(!(s->icnt && ix >= s->ibase && ix < s->ibase + s->icnt)) {
        uint32_t cnt = s->nobj - ix < M_IDS ? s->nobj - ix : M_IDS;
        if(!m_read_at(s->f, IDX_TABLE_OFF + 20ull * ix, s->ib, 20u * cnt)) {
            m->err = true;
            return NULL;
        }
        s->ibase = ix;
        s->icnt = cnt;
    }
    return s->ib + 20u * (ix - s->ibase);
}

/* value ix of a 4-byte table that starts at byte offset tbl */
static bool ms_u32(Merge* m, MSrc* s, uint64_t tbl, uint32_t ix, uint32_t* out) {
    if(!(s->vcnt && ix >= s->vbase && ix < s->vbase + s->vcnt)) {
        uint32_t cnt = s->nobj - ix < M_VALS ? s->nobj - ix : M_VALS;
        if(!m_read_at(s->f, tbl + 4ull * ix, s->vb, 4u * cnt)) {
            m->err = true;
            return false;
        }
        s->vbase = ix;
        s->vcnt = cnt;
    }
    const uint8_t* p = s->vb + 4u * (ix - s->vbase);
    *out = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    return true;
}

static void mg_close(Merge* m) {
    for(unsigned i = 0; i < m->k; i++) {
        if(m->s[i].f) hub_gitio_close(m->s[i].f);
        m->s[i].f = NULL;
    }
}

/* Open the idx file of every source. Only one table (ids) is cached per pass
 * start; vcnt/icnt are reset. */
static int mg_open(MCtx* c, void* io, const char* repo, char names[][46], unsigned k) {
    Merge* m = &c->m;
    memset(m, 0, sizeof *m);
    m->k = k;
    for(unsigned i = 0; i < k; i++) {
        snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.idx", repo, names[i]);
        MSrc* s = &m->s[i];
        s->f = hub_gitio_open(io, c->p1);
        uint8_t h[8], f4[4];
        if(!s->f || !m_read_at(s->f, 0, h, 8) || memcmp(h, "\377tOc\0\0\0\2", 8) != 0 ||
           !m_read_at(s->f, 8 + 4 * 255, f4, 4)) {
            mg_close(m);
            return HUB_GIT_ERR_CORRUPT;
        }
        s->nobj = (uint32_t)f4[0] << 24 | (uint32_t)f4[1] << 16 | (uint32_t)f4[2] << 8 | f4[3];
        if(hub_gitio_size(s->f) < IDX_TABLE_OFF + 28ull * s->nobj + 40) {
            mg_close(m);
            return HUB_GIT_ERR_CORRUPT;
        }
    }
    return HUB_GIT_OK;
}

/* Next id in sorted order across all sources. 1 = got one, 0 = end, -1 = error. */
static int mg_next(Merge* m, uint8_t id[20], unsigned* src, uint32_t* ix) {
    int best = -1;
    const uint8_t* bid = NULL;
    for(unsigned i = 0; i < m->k; i++) {
        MSrc* s = &m->s[i];
        if(s->next >= s->nobj) continue;
        const uint8_t* p = ms_id(m, s, s->next);
        if(!p) return -1;
        if(best < 0 || memcmp(p, bid, 20) < 0) {
            best = (int)i;
            bid = p;
        }
    }
    if(best < 0) return 0;
    memcpy(id, bid, 20);
    *src = (unsigned)best;
    *ix = m->s[best].next++;
    return 1;
}

/* ---------- stats ---------- */

int hub_git_pack_stats(void* io, const char* repo, uint32_t small_limit, unsigned* total, unsigned* nsmall) {
    if(!small_limit) small_limit = M_SMALL_DEFAULT;
    *total = *nsmall = 0;
    char* path = malloc(M_PATH);
    if(!path) return HUB_GIT_ERR_NOMEM;
    snprintf(path, M_PATH, "%s/objects/pack", repo);
    HubGitDir* d = hub_gitio_dir_open(io, path);
    free(path);
    if(!d) return HUB_GIT_OK;
    char name[64];
    bool isdir;
    uint64_t sz;
    while(hub_gitio_dir_read_ex(d, name, sizeof name, &isdir, &sz)) {
        size_t n = strlen(name);
        if(isdir || n != 50 || strncmp(name, "pack-", 5) != 0 || strcmp(name + 45, ".pack") != 0) continue;
        (*total)++;
        if(sz < small_limit) (*nsmall)++;
    }
    hub_gitio_dir_close(d);
    return HUB_GIT_OK;
}

/* ---------- merge ---------- */

#define TMP_P "tmp_merge_p"
#define TMP_I "tmp_merge_i"

static void m_paths(MCtx* c, const char* repo, const char* leaf, char* out) {
    (void)c;
    snprintf(out, M_PATH, "%s/objects/pack/%s", repo, leaf);
}

static void rm_tmp_merge(void* io, MCtx* c, const char* repo) {
    m_paths(c, repo, TMP_P, c->p1);
    hub_gitio_remove(io, c->p1);
    m_paths(c, repo, TMP_I, c->p1);
    hub_gitio_remove(io, c->p1);
}

/* Pick the sources (smallest first), check them, drop duplicate sharers. */
static int merge_select(void* io, const char* repo, MCtx* c, HubGitMergeCursor* cur, uint32_t small_limit, unsigned max_srcs) {
    char names[HUB_MERGE_MAX_SRC][46];
    uint32_t sizes[HUB_MERGE_MAX_SRC];
    unsigned k = 0;
    snprintf(c->p1, M_PATH, "%s/objects/pack", repo);
    HubGitDir* d = hub_gitio_dir_open(io, c->p1);
    if(!d) return HUB_GIT_OK;
    char name[64];
    bool isdir;
    uint64_t sz;
    while(hub_gitio_dir_read_ex(d, name, sizeof name, &isdir, &sz)) {
        size_t n = strlen(name);
        if(isdir || n != 50 || strncmp(name, "pack-", 5) != 0 || strcmp(name + 45, ".pack") != 0) continue;
        if(sz >= small_limit || sz < 32) continue;
        /* insert into the sorted list of the smallest max_srcs */
        unsigned pos = k;
        while(pos > 0 && sizes[pos - 1] > sz) pos--;
        if(pos >= max_srcs) continue;
        if(k < max_srcs) k++;
        for(unsigned j = k - 1; j > pos; j--) {
            memcpy(names[j], names[j - 1], 46);
            sizes[j] = sizes[j - 1];
        }
        memcpy(names[pos], name, 45);
        names[pos][45] = '\0';
        sizes[pos] = (uint32_t)sz;
    }
    hub_gitio_dir_close(d);

    /* header of each pack must say PACK v2 with the idx's object count */
    for(unsigned i = 0; i < k;) {
        bool ok = false;
        uint8_t h[12], f4[4];
        snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.pack", repo, names[i]);
        HubGitFile* f = hub_gitio_open(io, c->p1);
        snprintf(c->p2, M_PATH, "%s/objects/pack/%.45s.idx", repo, names[i]);
        HubGitFile* x = hub_gitio_open(io, c->p2);
        if(f && x && m_read_at(f, 0, h, 12) && memcmp(h, "PACK\0\0\0\2", 8) == 0 &&
           m_read_at(x, 8 + 4 * 255, f4, 4) && memcmp(h + 8, f4, 4) == 0)
            ok = true;
        if(f) hub_gitio_close(f);
        if(x) hub_gitio_close(x);
        if(ok) {
            i++;
        } else {
            for(unsigned j = i; j + 1 < k; j++) {
                memcpy(names[j], names[j + 1], 46);
                sizes[j] = sizes[j + 1];
            }
            k--;
        }
    }

    /* drop sources that share an id with an earlier one */
    while(k >= 2) {
        int r = mg_open(c, io, repo, names, k);
        if(r) return r;
        uint8_t id[20], prev[20];
        unsigned src = 0;
        uint32_t ix;
        int dup = -1, got;
        bool have = false;
        while((got = mg_next(&c->m, id, &src, &ix)) > 0) {
            if(have && memcmp(id, prev, 20) == 0) {
                dup = (int)src;
                break;
            }
            memcpy(prev, id, 20);
            have = true;
        }
        mg_close(&c->m);
        if(got < 0) return HUB_GIT_ERR_IO;
        if(dup < 0) break;
        for(unsigned j = (unsigned)dup; j + 1 < k; j++) {
            memcpy(names[j], names[j + 1], 46);
            sizes[j] = sizes[j + 1];
        }
        k--;
    }
    if(k < 2) return HUB_GIT_OK; /* nothing to merge */

    memset(cur, 0, sizeof *cur);
    cur->nsrc = (uint8_t)k;
    uint32_t pos = 12, count = 0;
    for(unsigned i = 0; i < k; i++) {
        memcpy(cur->names[i], names[i], 46);
        cur->elen[i] = sizes[i] - 32;
        cur->dest[i] = pos;
        pos += cur->elen[i];
        snprintf(c->p2, M_PATH, "%s/objects/pack/%.45s.idx", repo, names[i]);
        HubGitFile* x = hub_gitio_open(io, c->p2);
        uint8_t f4[4];
        if(!x || !m_read_at(x, 8 + 4 * 255, f4, 4)) {
            if(x) hub_gitio_close(x);
            return HUB_GIT_ERR_IO;
        }
        hub_gitio_close(x);
        count += (uint32_t)f4[0] << 24 | (uint32_t)f4[1] << 16 | (uint32_t)f4[2] << 8 | f4[3];
        if(pos > 0x7fffffffu) return HUB_GIT_ERR_TOOLARGE;
    }
    cur->count = count;
    return HUB_GIT_OK;
}

static int merge_build_idx(
    void* io, const char* repo, MCtx* c, HubGitMergeCursor* cur, const uint8_t pack_sha[20]) {
    IdxOut* o = malloc(sizeof(IdxOut));
    if(!o) return HUB_GIT_ERR_NOMEM;
    m_paths(c, repo, TMP_I, c->p1);
    o->f = hub_gitio_create(io, c->p1);
    if(!o->f) {
        free(o);
        return HUB_GIT_ERR_IO;
    }
    hub_sha1_init(&o->sha);
    o->n = 0;
    o->err = false;
    static const uint8_t magic[8] = {0xff, 't', 'O', 'c', 0, 0, 0, 2};
    idx_put(o, magic, 8);

    int r = HUB_GIT_OK;
    uint8_t id[20];
    unsigned src;
    uint32_t ix, total = 0, v;
    int got;
    Merge* m = &c->m;

    /* pass 0: fanout. When an id with first byte f shows up, every entry
     * below f is complete and equals the count so far. */
    r = mg_open(c, io, repo, cur->names, cur->nsrc);
    if(r) goto out;
    unsigned b = 0;
    while((got = mg_next(m, id, &src, &ix)) > 0) {
        for(; b < id[0]; b++) idx_be32(o, total);
        total++;
    }
    mg_close(m);
    if(got < 0) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    for(; b < 256; b++) idx_be32(o, total);
    if(total != cur->count) {
        r = HUB_GIT_ERR_CORRUPT;
        goto out;
    }

    /* pass 1: ids */
    r = mg_open(c, io, repo, cur->names, cur->nsrc);
    if(r) goto out;
    while((got = mg_next(m, id, &src, &ix)) > 0) idx_put(o, id, 20);
    mg_close(m);
    if(got < 0) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }

    /* pass 2: CRC32 values, copied from the sources */
    r = mg_open(c, io, repo, cur->names, cur->nsrc);
    if(r) goto out;
    while((got = mg_next(m, id, &src, &ix)) > 0) {
        MSrc* s = &m->s[src];
        if(!ms_u32(m, s, IDX_TABLE_OFF + 20ull * s->nobj, ix, &v)) {
            got = -1;
            break;
        }
        idx_be32(o, v);
    }
    mg_close(m);
    if(got < 0) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }

    /* pass 3: offsets, moved to the place of the source in the new pack */
    r = mg_open(c, io, repo, cur->names, cur->nsrc);
    if(r) goto out;
    while((got = mg_next(m, id, &src, &ix)) > 0) {
        MSrc* s = &m->s[src];
        if(!ms_u32(m, s, IDX_TABLE_OFF + 24ull * s->nobj, ix, &v)) {
            got = -1;
            break;
        }
        if((v & 0x80000000u) || v < 12 || v - 12 >= cur->elen[src]) {
            mg_close(m);
            r = HUB_GIT_ERR_CORRUPT;
            goto out;
        }
        idx_be32(o, v - 12 + cur->dest[src]);
    }
    mg_close(m);
    if(got < 0) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }

    idx_put(o, pack_sha, 20);
    idx_flush(o);
    {
        uint8_t sum[20];
        hub_sha1_final(&o->sha, sum);
        if(!o->err && hub_gitio_write(o->f, sum, 20) != 20) o->err = true;
    }
    if(o->err) r = HUB_GIT_ERR_IO;
out:
    mg_close(m);
    hub_gitio_close(o->f);
    free(o);
    return r;
}

int hub_git_pack_merge(
    void* io,
    const char* repo,
    HubGitMergeCursor* cur,
    uint32_t small_limit,
    unsigned max_srcs,
    HubGitPackStop stop,
    void* ctx,
    HubGitMergeResult* res) {
    memset(res, 0, sizeof *res);
    if(strlen(repo) > HUB_GIT_PATH_MAX - 40) return HUB_GIT_ERR_BADARG;
    if(!small_limit) small_limit = M_SMALL_DEFAULT;
    if(!max_srcs || max_srcs > HUB_MERGE_MAX_SRC) max_srcs = HUB_MERGE_MAX_SRC;
    MCtx* c = malloc(sizeof(MCtx));
    if(!c) return HUB_GIT_ERR_NOMEM;
    int r = HUB_GIT_OK;
    HubGitFile* f = NULL; /* tmp pack */
    HubGitFile* sf = NULL; /* current source pack */

    if(cur->phase == 0) {
        memset(cur, 0, sizeof *cur);
        rm_tmp_merge(io, c, repo);
        r = merge_select(io, repo, c, cur, small_limit, max_srcs);
        if(r) goto fail;
        if(cur->nsrc < 2) {
            memset(cur, 0, sizeof *cur);
            free(c);
            return HUB_GIT_OK; /* nothing to merge */
        }
        m_paths(c, repo, TMP_P, c->p1);
        f = hub_gitio_create(io, c->p1);
        if(!f) {
            r = HUB_GIT_ERR_IO;
            goto fail;
        }
        uint32_t n = cur->count;
        uint8_t hdr[12] = {'P', 'A', 'C', 'K', 0, 0, 0, 2, (uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n};
        if(hub_gitio_write(f, hdr, 12) != 12) {
            r = HUB_GIT_ERR_IO;
            goto fail;
        }
        hub_sha1_init(&cur->sha);
        hub_sha1_update(&cur->sha, hdr, 12);
        cur->total = 12;
        cur->phase = 1;
    } else {
        m_paths(c, repo, TMP_P, c->p1);
        f = hub_gitio_open_rw(io, c->p1);
        if(!f || hub_gitio_size(f) != cur->total || !hub_gitio_seek(f, cur->total)) {
            r = HUB_GIT_ERR_IO; /* the temp file is gone or changed: start over next time */
            goto fail;
        }
    }

    if(cur->phase == 1) {
        while(cur->cur_src < cur->nsrc) {
            unsigned i = cur->cur_src;
            snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.pack", repo, cur->names[i]);
            sf = hub_gitio_open(io, c->p1);
            if(!sf) {
                r = HUB_GIT_ERR_IO;
                goto fail;
            }
            while(cur->cur_off < cur->elen[i]) {
                if(stop && stop(ctx)) {
                    res->stopped = true;
                    res->more = true;
                    goto suspend;
                }
                uint32_t k = cur->elen[i] - cur->cur_off;
                if(k > sizeof c->buf) k = sizeof c->buf;
                if(!hub_gitio_seek(sf, 12ull + cur->cur_off) || hub_gitio_read(sf, c->buf, k) != k ||
                   hub_gitio_write(f, c->buf, k) != k) {
                    r = HUB_GIT_ERR_IO;
                    goto fail;
                }
                hub_sha1_update(&cur->sha, c->buf, k);
                cur->total += k;
                cur->cur_off += k;
            }
            hub_gitio_close(sf);
            sf = NULL;
            cur->cur_src++;
            cur->cur_off = 0;
        }
        if(cur->total != cur->dest[cur->nsrc - 1] + cur->elen[cur->nsrc - 1]) {
            r = HUB_GIT_ERR_CORRUPT;
            goto fail;
        }
        cur->phase = 2;
        res->more = true; /* the idx and the commit come in the next call */
        goto suspend;
    }

    /* phase 2: trailer, idx, commit */
    {
        uint8_t sum[20];
        hub_sha1_final(&cur->sha, sum);
        if(hub_gitio_write(f, sum, 20) != 20) {
            r = HUB_GIT_ERR_IO;
            goto fail;
        }
        hub_gitio_close(f);
        f = NULL;
        res->pack_bytes = cur->total + 20;

        r = merge_build_idx(io, repo, c, cur, sum);
        if(r) goto fail;

        char hex[41];
        HubOid so;
        memcpy(so.b, sum, 20);
        hub_oid_to_hex(&so, hex);
        snprintf(res->name, sizeof res->name, "pack-%s", hex);
        /* the result may have the name of a source only if they are the same bytes: then keep it */
        m_paths(c, repo, TMP_P, c->p1);
        snprintf(c->p2, M_PATH, "%s/objects/pack/%s.pack", repo, res->name);
        if(!hub_gitio_rename(io, c->p1, c->p2)) {
            r = HUB_GIT_ERR_IO;
            goto fail;
        }
        m_paths(c, repo, TMP_I, c->p1);
        snprintf(c->p2, M_PATH, "%s/objects/pack/%s.idx", repo, res->name);
        if(!hub_gitio_rename(io, c->p1, c->p2)) {
            r = HUB_GIT_ERR_IO;
            goto fail;
        }
        /* new pack and idx are in place: the sources can go, idx first */
        for(unsigned i = 0; i < cur->nsrc; i++) {
            if(strcmp(cur->names[i], res->name) == 0) continue;
            snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.idx", repo, cur->names[i]);
            hub_gitio_remove(io, c->p1);
            snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.pack", repo, cur->names[i]);
            hub_gitio_remove(io, c->p1);
            /* files that newer git writes next to a pack */
            snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.rev", repo, cur->names[i]);
            hub_gitio_remove(io, c->p1);
            snprintf(c->p1, M_PATH, "%s/objects/pack/%.45s.bitmap", repo, cur->names[i]);
            hub_gitio_remove(io, c->p1);
        }
        res->merged_packs = cur->nsrc;
        res->objects = cur->count;
        memset(cur, 0, sizeof *cur);
        free(c);
        return HUB_GIT_OK;
    }
suspend:
    if(sf) hub_gitio_close(sf);
    if(f) hub_gitio_close(f);
    res->objects = cur->count;
    free(c);
    return HUB_GIT_OK;
fail:
    if(sf) hub_gitio_close(sf);
    if(f) hub_gitio_close(f);
    rm_tmp_merge(io, c, repo);
    memset(cur, 0, sizeof *cur);
    free(c);
    return r;
}
