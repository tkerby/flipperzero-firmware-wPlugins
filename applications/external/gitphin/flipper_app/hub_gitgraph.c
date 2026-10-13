#include "hub_gitgraph.h"

#include "hub_gitio.h"
#include "hub_gitobj.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define GB_TODO  128 /* ids waiting (pending + ref tips) */
#define GB_DEPTH 8 /* folder depth below refs/ */
#define GB_PATH  (HUB_GIT_PATH_MAX + HUB_GIT_REF_MAX + 16)
#define GB_CHUNK 16 /* old records per read while merging */

typedef struct {
    HubGit* g;
    void* io;
    const char* repo;
    unsigned nnews, ntodo;
    uint32_t old_count;
    uint32_t fan[256];
    bool old_ok;
    HubOid todo[GB_TODO];
    char path[GB_PATH];
    char tmp[GB_PATH];
    uint8_t buf[GB_CHUNK * HUB_GRAPH_REC];
    uint8_t news[HUB_GB_CAP][HUB_GRAPH_REC];
} GB;

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* ---------- id list ---------- */

static bool todo_has(const GB* c, const HubOid* id) {
    for(unsigned i = 0; i < c->ntodo; i++) {
        if(memcmp(c->todo[i].b, id->b, 20) == 0) return true;
    }
    return false;
}

static void todo_add(GB* c, const HubOid* id) {
    if(c->ntodo < GB_TODO && !todo_has(c, id)) c->todo[c->ntodo++] = *id;
}

/* position of id in the sorted new records; found says if it is there */
static unsigned news_find(const GB* c, const uint8_t* id, bool* found) {
    unsigned lo = 0, hi = c->nnews;
    while(lo < hi) {
        unsigned mid = (lo + hi) / 2;
        int k = memcmp(c->news[mid], id, 20);
        if(k == 0) {
            *found = true;
            return mid;
        }
        if(k < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *found = false;
    return lo;
}

/* Octopus commit: the parents after the fourth are read from the object. */
static void todo_more_parents(GB* c, const HubOid* id) {
    uint8_t type;
    uint8_t* data;
    size_t size;
    if(hub_git_read(c->g, id, &type, &data, &size) != HUB_GIT_OK) return;
    const char* p = (const char*)data;
    const char* end = p + size;
    unsigned seen = 0;
    char hex[41];
    while(p < end && *p != '\n')
        p++; /* the tree line */
    while(end - p > 48 && memcmp(p + 1, "parent ", 7) == 0) {
        p++;
        if(++seen > HUB_GIT_MAX_PARENTS) {
            memcpy(hex, p + 7, 40);
            hex[40] = '\0';
            HubOid o;
            if(hub_oid_from_hex(hex, &o)) todo_add(c, &o);
        }
        p += 47;
    }
    hub_git_free(data);
}

/* ---------- old file ---------- */

/* Header of the old graph: loads the pending ids into todo. */
static void old_load(GB* c) {
    snprintf(c->path, GB_PATH, "%s/hub/graph.bin", c->repo);
    HubGitFile* f = hub_gitio_open(c->io, c->path);
    if(!f) return;
    uint8_t h[HUB_GRAPH_HDR];
    uint32_t n = 0, np = 0;
    if(hub_gitio_read(f, h, sizeof h) == sizeof h && memcmp(h, "HGRF\0\0\0\1", 8) == 0) {
        n = rd32(h + 8);
        np = rd32(h + 12);
        uint64_t want = HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * n + 20ull * np;
        if(hub_gitio_size(f) != want || np > GB_TODO) n = 0;
    }
    if(n > 0) {
        c->old_ok = true;
        c->old_count = n;
        uint8_t id[20];
        if(np && hub_gitio_seek(f, HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * n)) {
            for(uint32_t i = 0; i < np; i++) {
                if(hub_gitio_read(f, id, 20) != 20) break;
                HubOid o;
                memcpy(o.b, id, 20);
                todo_add(c, &o);
            }
        }
    }
    hub_gitio_close(f);
}

/* ---------- ref tips ---------- */

static void tip_add(GB* c, const char* hex40) {
    char hex[41];
    HubOid o;
    memcpy(hex, hex40, 40);
    hex[40] = '\0';
    if(!hub_oid_from_hex(hex, &o)) return;
    if(hub_git_peel(c->g, &o) != HUB_GIT_OK) return;
    todo_add(c, &o);
}

static void tip_file(GB* c, const char* path) {
    HubGitFile* f = hub_gitio_open(c->io, path);
    if(!f) return;
    char b[48];
    size_t n = hub_gitio_read(f, b, 47);
    hub_gitio_close(f);
    if(n >= 40) tip_add(c, b);
}

static void tips_packed(GB* c) {
    snprintf(c->path, GB_PATH, "%s/packed-refs", c->repo);
    HubGitFile* f = hub_gitio_open(c->io, c->path);
    if(!f) return;
    char line[48];
    size_t ll = 0, n;
    bool skip = false;
    uint8_t b[128];
    while((n = hub_gitio_read(f, b, sizeof b)) > 0) {
        for(size_t i = 0; i < n; i++) {
            if(b[i] == '\n') {
                if(!skip && ll >= 41 && line[0] != '#' && line[0] != '^' && line[40] == ' ')
                    tip_add(c, line);
                ll = 0;
                skip = false;
            } else if(ll < sizeof line) {
                line[ll++] = (char)b[i];
            }
        }
    }
    hub_gitio_close(f);
}

/* All loose refs below refs/ (folder by folder, no recursion), HEAD when it
 * is detached, and packed-refs. */
static void tips_collect(GB* c) {
    HubGitDir* dh[GB_DEPTH];
    size_t plen[GB_DEPTH];
    int lvl = 0;
    size_t base = (size_t)snprintf(c->tmp, GB_PATH, "%s/refs", c->repo);
    dh[0] = hub_gitio_dir_open(c->io, c->tmp);
    plen[0] = base;
    if(!dh[0]) lvl = -1;
    char name[64];
    bool isdir;
    while(lvl >= 0) {
        if(!hub_gitio_dir_read(dh[lvl], name, sizeof name, &isdir)) {
            hub_gitio_dir_close(dh[lvl]);
            lvl--;
            continue;
        }
        size_t nl = strlen(name);
        size_t pl = plen[lvl];
        if(pl + 1 + nl >= GB_PATH) continue;
        c->tmp[pl] = '/';
        memcpy(c->tmp + pl + 1, name, nl + 1);
        if(isdir) {
            if(lvl + 1 < GB_DEPTH) {
                dh[lvl + 1] = hub_gitio_dir_open(c->io, c->tmp);
                if(dh[lvl + 1]) plen[++lvl] = pl + 1 + nl;
            }
            continue; /* the path stays: the new level builds on it */
        }
        tip_file(c, c->tmp);
        c->tmp[pl] = '\0';
    }
    snprintf(c->tmp, GB_PATH, "%s/HEAD", c->repo);
    HubGitFile* f = hub_gitio_open(c->io, c->tmp);
    if(f) {
        char b[48];
        size_t n = hub_gitio_read(f, b, 47);
        hub_gitio_close(f);
        if(n >= 40 && memcmp(b, "ref:", 4) != 0) tip_add(c, b);
    }
    tips_packed(c);
}

/* ---------- writing ---------- */

static int graph_write(GB* c) {
    uint32_t* fan = c->fan;
    memset(fan, 0, sizeof c->fan);
    HubGitFile* of = NULL;
    uint32_t oc = c->old_ok ? c->old_count : 0;
    if(oc) {
        snprintf(c->path, GB_PATH, "%s/hub/graph.bin", c->repo);
        of = hub_gitio_open(c->io, c->path);
        uint8_t* fb = c->buf; /* 1024 bytes of fanout fit in buf (16 * 68 = 1088) */
        if(!of || !hub_gitio_seek(of, HUB_GRAPH_HDR) || hub_gitio_read(of, fb, 1024) != 1024) {
            if(of) hub_gitio_close(of);
            return HUB_GIT_ERR_IO;
        }
        for(unsigned i = 0; i < 256; i++)
            fan[i] = rd32(fb + 4 * i);
    }
    /* new cumulative fanout */
    {
        unsigned k = 0;
        for(unsigned b = 0; b < 256; b++) {
            while(k < c->nnews && c->news[k][0] <= b)
                k++;
            fan[b] += k;
        }
    }
    uint32_t total = oc + c->nnews;
    snprintf(c->tmp, GB_PATH, "%s/hub", c->repo);
    hub_gitio_mkdir(c->io, c->tmp);
    snprintf(c->tmp, GB_PATH, "%s/hub/graph.tmp", c->repo);
    HubGitFile* nf = hub_gitio_create(c->io, c->tmp);
    if(!nf) {
        if(of) hub_gitio_close(of);
        return HUB_GIT_ERR_IO;
    }
    bool ok = true;
    uint8_t h[HUB_GRAPH_HDR];
    memcpy(h, "HGRF\0\0\0\1", 8);
    wr32(h + 8, total);
    wr32(h + 12, c->ntodo);
    ok = hub_gitio_write(nf, h, sizeof h) == sizeof h;
    uint8_t fb[4 * 32];
    for(unsigned b = 0; b < 256 && ok; b += 32) {
        for(unsigned j = 0; j < 32; j++)
            wr32(fb + 4 * j, fan[b + j]);
        ok = hub_gitio_write(nf, fb, sizeof fb) == sizeof fb;
    }
    /* merge the old records (in chunks) with the sorted new ones */
    uint32_t oi = 0, ni = 0, in_buf = 0, buf_at = 0;
    if(of) ok = ok && hub_gitio_seek(of, HUB_GRAPH_RECS);
    while(ok && (oi < oc || ni < c->nnews)) {
        const uint8_t* o = NULL;
        if(oi < oc) {
            if(buf_at == in_buf) {
                uint32_t k = oc - oi < GB_CHUNK ? oc - oi : GB_CHUNK;
                if(hub_gitio_read(of, c->buf, (size_t)k * HUB_GRAPH_REC) !=
                   (size_t)k * HUB_GRAPH_REC) {
                    ok = false;
                    break;
                }
                in_buf = k;
                buf_at = 0;
            }
            o = c->buf + (size_t)buf_at * HUB_GRAPH_REC;
        }
        const uint8_t* n = ni < c->nnews ? c->news[ni] : NULL;
        const uint8_t* take;
        if(o && (!n || memcmp(o, n, 20) < 0)) {
            take = o;
            oi++;
            buf_at++;
        } else {
            take = n;
            ni++;
        }
        ok = hub_gitio_write(nf, take, HUB_GRAPH_REC) == HUB_GRAPH_REC;
    }
    for(unsigned i = 0; i < c->ntodo && ok; i++)
        ok = hub_gitio_write(nf, c->todo[i].b, 20) == 20;
    hub_gitio_close(nf);
    if(of) hub_gitio_close(of);
    if(ok) {
        snprintf(c->path, GB_PATH, "%s/hub/graph.bin", c->repo);
        ok = hub_gitio_rename(c->io, c->tmp, c->path);
    }
    if(!ok) hub_gitio_remove(c->io, c->tmp);
    return ok ? HUB_GIT_OK : HUB_GIT_ERR_IO;
}

/* ---------- the slice ---------- */

int hub_git_graph_update(
    void* io,
    const char* repo,
    HubGitPackStop stop,
    void* ctx,
    HubGitGraphResult* res) {
    memset(res, 0, sizeof *res);
    if(strlen(repo) > HUB_GIT_PATH_MAX - 40) return HUB_GIT_ERR_BADARG;
    GB* c = calloc(1, sizeof(GB));
    if(!c) return HUB_GIT_ERR_NOMEM;
    c->io = io;
    c->repo = repo;
    c->g = hub_git_open(io, repo);
    if(!c->g) {
        free(c);
        return HUB_GIT_ERR_NOMEM;
    }
    hub_git_graph_ignore(c->g);
    hub_git_set_limit(c->g, 4096); /* commits are small; a bigger one is left out of the graph */
    old_load(c);
    unsigned old_pending = c->ntodo;
    tips_collect(c);

    int rc = HUB_GIT_OK;
    HubCommitMeta* m = malloc(sizeof(HubCommitMeta));
    if(!m) rc = HUB_GIT_ERR_NOMEM;
    while(rc == HUB_GIT_OK && c->ntodo > 0 && c->nnews < HUB_GB_CAP) {
        /* known ids are cheap: the clock is looked at only after the first new record */
        if(c->nnews > 0 && stop && stop(ctx)) {
            res->stopped = true;
            break;
        }
        HubOid id = c->todo[--c->ntodo];
        bool found;
        news_find(c, id.b, &found);
        if(found) continue;
        int r = hub_git_commit_meta(c->g, &id, m);
        if(r == HUB_GIT_ERR_CORRUPT || r == HUB_GIT_ERR_NOTFOUND || r == HUB_GIT_ERR_TOOLARGE)
            continue;
        if(r != HUB_GIT_OK) {
            rc = r;
            break;
        }
        if(m->cached) continue; /* already in the graph */
        unsigned np = m->nparents < HUB_GIT_MAX_PARENTS ? m->nparents : HUB_GIT_MAX_PARENTS;
        unsigned at = news_find(c, id.b, &found);
        memmove(c->news[at + 1], c->news[at], (size_t)(c->nnews - at) * HUB_GRAPH_REC);
        uint8_t* rec = c->news[at];
        memset(rec, 0, HUB_GRAPH_REC);
        memcpy(rec, id.b, 20);
        wr32(rec + 20, m->time);
        if(np > 0) memcpy(rec + 24, m->parent[0].b, 20);
        if(np > 1) memcpy(rec + 44, m->parent[1].b, 20);
        wr32(
            rec + 64, (m->nparents > 2 ? 1u : 0u) | (m->nparents > 255 ? 255u : m->nparents) << 8);
        c->nnews++;
        for(unsigned j = 0; j < np; j++) {
            news_find(c, m->parent[j].b, &found);
            if(!found) todo_add(c, &m->parent[j]);
        }
        if(m->nparents > HUB_GIT_MAX_PARENTS) todo_more_parents(c, &id);
    }
    free(m);
    hub_git_close(c->g);
    c->g = NULL;

    if(c->nnews > 0 || old_pending > 0) {
        int w = graph_write(c);
        if(w == HUB_GIT_OK) {
            res->added = c->nnews;
        } else if(rc == HUB_GIT_OK) {
            rc = w;
        }
    }
    res->more = rc == HUB_GIT_OK && c->ntodo > 0;
    free(c);
    return rc;
}
