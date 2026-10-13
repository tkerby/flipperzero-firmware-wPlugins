/* Object reader: the spill-to-SD path, the object cache, hub_git_stream.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

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
static int
    file_stream(HubGit* g, const char* path, size_t size, uint8_t* cb, HubGitSink sink, void* ctx) {
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
static int delta_via_file(
    HubGit* g,
    Spill* s,
    HubGitFile* pf,
    const ChainEnt* ce,
    DeltaSpill* d,
    size_t* olen) {
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
    uint8_t* cb = r == HUB_GIT_OK ? malloc(SPILL_BUF) :
                                    NULL; /* not s->cb: the parser copies with it */
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
            if(r == HUB_GIT_OK && (olen != ce->dsize || d.st != DS_CMD || d.out != d.dst_size))
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
static int plain_cached(
    HubGit* g,
    const HubOid* oid,
    Loc loc,
    HubGitSink sink,
    void* ctx,
    uint8_t* type,
    size_t* size) {
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
