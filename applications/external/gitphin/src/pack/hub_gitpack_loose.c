/* Packer: packing loose objects in resumable jobs (hub_git_pack_loose).
 * Part of hub_gitpack.c: included there, in this order, not built alone. */
#include "hub_gitpack_int.h"

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
        if(!run->po.f || hub_gitio_size(run->po.f) != job->total ||
           !hub_gitio_seek(run->po.f, job->total)) {
            r = HUB_GIT_ERR_IO; /* the temp file changed: start over */
            cur->job = job; /* dropped below with the files */
            goto out;
        }
        run->po.total = job->total;
    }

    unsigned done = 0, kept = 0;
    while(done < n && !run->po.err) {
        unsigned end = done;
        while(end < n && ids[20 * (size_t)end] == ids[20 * (size_t)done])
            end++;
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
    uint16_t newdir = done == n ? (uint16_t)(big && kept == 0 ? next + 1 : next) :
                                  (uint16_t)ids[20 * (size_t)done];
    if(kept > 0) {
        /* the table of this slice goes to the end of tmp_pack_t */
        tmp_path(run, repo, run->pb, "tmp_pack_t");
        HubGitFile* t = hub_gitio_open_rw(io, run->pb);
        if(!t || hub_gitio_size(t) != (uint64_t)job->count * TAB_REC ||
           !hub_gitio_seek(t, (uint64_t)job->count * TAB_REC)) {
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
    *finish = job->count > 0 && done == n &&
              (next >= 256 || big || oneshot || job->count >= JOB_MAX);
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

static int job_hash(
    void* io,
    const char* repo,
    Run* run,
    PackJob* job,
    HubGitPackStop stop,
    void* ctx,
    bool* stopped) {
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
static void job_delete(
    void* io,
    const char* repo,
    Run* run,
    PackJob* job,
    HubGitPackStop stop,
    void* ctx,
    bool* done) {
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
        if(!hub_gitio_seek(t, (uint64_t)job->dpos * TAB_REC) ||
           hub_gitio_read(t, run->buf, k * TAB_REC) != k * TAB_REC) {
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
        r = job_append(
            io, repo, run, cur, max_objects, ram_limit, progress, stop, ctx, res, oneshot, &finish);
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
