/* Packer: merging small packs (hub_git_pack_merge).
 * Part of hub_gitpack.c: included there, in this order, not built alone. */
#include "hub_gitpack_int.h"

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
static int merge_select(
    void* io,
    const char* repo,
    MCtx* c,
    HubGitMergeCursor* cur,
    uint32_t small_limit,
    unsigned max_srcs) {
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
        if(isdir || n != 50 || strncmp(name, "pack-", 5) != 0 || strcmp(name + 45, ".pack") != 0)
            continue;
        if(sz >= small_limit || sz < 32) continue;
        /* insert into the sorted list of the smallest max_srcs */
        unsigned pos = k;
        while(pos > 0 && sizes[pos - 1] > sz)
            pos--;
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
    void* io,
    const char* repo,
    MCtx* c,
    HubGitMergeCursor* cur,
    const uint8_t pack_sha[20]) {
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
        for(; b < id[0]; b++)
            idx_be32(o, total);
        total++;
    }
    mg_close(m);
    if(got < 0) {
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    for(; b < 256; b++)
        idx_be32(o, total);
    if(total != cur->count) {
        r = HUB_GIT_ERR_CORRUPT;
        goto out;
    }

    /* pass 1: ids */
    r = mg_open(c, io, repo, cur->names, cur->nsrc);
    if(r) goto out;
    while((got = mg_next(m, id, &src, &ix)) > 0)
        idx_put(o, id, 20);
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
        uint8_t hdr[12] = {
            'P',
            'A',
            'C',
            'K',
            0,
            0,
            0,
            2,
            (uint8_t)(n >> 24),
            (uint8_t)(n >> 16),
            (uint8_t)(n >> 8),
            (uint8_t)n};
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
                if(!hub_gitio_seek(sf, 12ull + cur->cur_off) ||
                   hub_gitio_read(sf, c->buf, k) != k || hub_gitio_write(f, c->buf, k) != k) {
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
