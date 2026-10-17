/* Packer: merging small packs, the source packs and the stats.
 * Part of hub_gitpack.c: included there, in this order, not built alone. */
#include "hub_gitpack_int.h"

/* ======================= merging small packs ======================= */

#define M_IDS           8 /* ids cached per source */
#define M_VALS          16 /* u32 values cached per source */
#define M_PATH          (HUB_GIT_PATH_MAX + 80)
#define M_SMALL_DEFAULT (1024u * 1024u)
#define IDX_TABLE_OFF   1032u /* 8 header + 1024 fanout */

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

int hub_git_pack_stats(
    void* io,
    const char* repo,
    uint32_t small_limit,
    unsigned* total,
    unsigned* nsmall) {
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
        if(isdir || n != 50 || strncmp(name, "pack-", 5) != 0 || strcmp(name + 45, ".pack") != 0)
            continue;
        (*total)++;
        if(sz < small_limit) (*nsmall)++;
    }
    hub_gitio_dir_close(d);
    return HUB_GIT_OK;
}
