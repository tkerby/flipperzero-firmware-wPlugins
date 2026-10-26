/* Git routes: the idle packer of loose objects.
 * Part of hub_git.c: included there, in this order, not built alone. */
#include "hub_git_int.h"

/* ---------- packing loose objects ---------- */

#define PACK_BATCH    128
#define PACK_RAM      (16 * 1024)
#define PACK_MARKS    16
#define PACK_QUIET_MS 30000 /* no auto-pack this long after a write to the repo */

typedef struct {
    char name[GIT_NAME_MAX + 1];
    uint32_t last_write; /* tick */
    HubGitPackCursor cursor; /* where the last idle slice stopped */
    HubGitMergeCursor* merge; /* allocated only while a pack merge job runs */
    bool graph; /* the commit graph may be out of date */
    bool rescan; /* loose objects came in while a pack was open: scan again when it is finished */
} PackMark;

static PackMark* pack_marks; /* malloc'd on first use */

static PackMark* pack_find(const char* name) {
    if(!pack_marks) return NULL;
    for(size_t i = 0; i < PACK_MARKS; i++) {
        if(strcmp(pack_marks[i].name, name) == 0) return &pack_marks[i];
    }
    return NULL;
}

void hub_git_touch(const char* name) {
    PackMark* m = pack_find(name);
    if(m) m->last_write = furi_get_tick();
}

static PackMark* pack_get(const char* name) {
    if(!pack_marks) {
        pack_marks = malloc(sizeof(PackMark) * PACK_MARKS);
        if(!pack_marks) return NULL;
        memset(pack_marks, 0, sizeof(PackMark) * PACK_MARKS);
    }
    PackMark* m = pack_find(name);
    if(!m) {
        for(size_t i = 0; i < PACK_MARKS && !m; i++) {
            if(pack_marks[i].name[0] == '\0') m = &pack_marks[i];
        }
        if(!m) return NULL; /* full: the next start marks it again */
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->cursor.dir = 0;
    }
    return m;
}

void hub_git_mark_loose(const char* name) {
    PackMark* m = pack_get(name);
    if(!m) return;
    m->last_write = furi_get_tick();
    m->graph = true;
    if(m->cursor.job) {
        m->rescan = true; /* the open pack must not list a folder twice */
    } else {
        m->cursor.dir = 0; /* new objects may be in folders already passed */
    }
}

/* name of GIT_ROOT "/<name>.git" */
void hub_git_mark_base(const char* base) {
    size_t root = sizeof(GIT_ROOT);
    size_t len = strlen(base);
    if(len <= root + 4) return;
    char name[GIT_NAME_MAX + 1];
    len -= root + 4;
    if(len > GIT_NAME_MAX) return;
    memcpy(name, base + root, len);
    name[len] = '\0';
    hub_git_mark_loose(name);
}

/* A read had to open commit objects the graph did not know: only the graph needs work. */
static void graph_stale(const char* base) {
    size_t root = sizeof(GIT_ROOT);
    size_t len = strlen(base);
    if(len <= root + 4 || len - root - 4 > GIT_NAME_MAX) return;
    char name[GIT_NAME_MAX + 1];
    memcpy(name, base + root, len - root - 4);
    name[len - root - 4] = '\0';
    PackMark* m = pack_get(name);
    if(!m) return;
    m->graph = true;
    m->last_write = furi_get_tick();
}

static void pack_unmark(const char* name) {
    PackMark* m = pack_find(name);
    if(m) {
        m->name[0] = '\0';
        free(m->merge);
        free(m->cursor.job);
        m->cursor.job = NULL;
        m->merge = NULL;
        m->graph = m->rescan = false;
    }
}

void hub_git_free_state(void) {
    if(!pack_marks) return;
    for(size_t i = 0; i < PACK_MARKS; i++) {
        free(pack_marks[i].merge);
        free(pack_marks[i].cursor.job);
    }
    free(pack_marks);
    pack_marks = NULL;
}

/* At start every repo may have loose objects: mark them all once. */
void hub_git_mark_all(Storage* storage) {
    hub_gitobj_stale = graph_stale;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(!file_info_is_dir(&info) || len < 5 || strcmp(name + len - 4, ".git") != 0)
                continue;
            name[len - 4] = '\0';
            if(git_name_ok(name)) hub_git_mark_loose(name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    /* the start is not a write: the packer may begin as soon as it is idle */
    if(pack_marks) {
        for(size_t i = 0; i < PACK_MARKS; i++)
            pack_marks[i].last_write = 0;
    }
}

/* objects/info/packs lists the new pack (needed after every batch). */
static void pack_regen_packs(HubReq* r, const char* name) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    git_write_info_packs(g);
    free(g);
}

/* The packer keeps what it packed when it is stopped, so a slice is just a
 * call with a deadline. Idle slices are 3 s, the API route loops up to 25 s. */
/* Idle packer switch: false keeps the marks but never packs by itself
 * (POST /api/git/pack still works). */
#define PACK_AUTO     true
/* Object writing per slice. The finish (SHA-1 over the pack, idx, renames) and the
 * deletion of the packed loose files come on top: measured 1.0-2.2 s, more for big objects. */
#define PACK_SLICE_MS 1000
#define PACK_API_MS   25000

typedef struct {
    uint32_t budget; /* ms of object writing, counted from the first object */
    uint32_t deadline; /* tick */
    uint32_t first; /* tick of the first stop callback */
} PackClock;

static bool pack_stop(void* ctx) {
    hub_beat();
    PackClock* k = ctx;
    if(!k->first) {
        k->first = furi_get_tick();
        k->deadline = k->first + k->budget; /* the scan for the first object does not count */
    }
    return (int32_t)(furi_get_tick() - k->deadline) >= 0;
}

/* One slice for the repo. Returns true while more work is likely. */
static bool pack_slice(
    HubReq* r,
    const char* name,
    HubGitPackCursor* cursor,
    uint32_t budget,
    HubGitPackResult* res,
    bool* nomem) {
    char dir[GIT_NAME_MAX + 40];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    PackClock k = {.budget = budget};
    memset(res, 0, sizeof(*res));
    uint16_t dir_before = cursor ? cursor->dir : 0;
    int rc = hub_git_pack_loose(
        r->storage, dir, PACK_BATCH, PACK_RAM, cursor, NULL, pack_stop, &k, res);
    if(rc == HUB_GIT_ERR_NOMEM) *nomem = true;
    if(res->finished) pack_regen_packs(r, name);
    /* a slice that only scanned folders is progress too, if the cursor moved */
    bool progress = res->packed > 0 || res->finished || res->stopped ||
                    (cursor && (cursor->dir != dir_before || cursor->job));
    return rc == 0 && res->more && progress;
}

/* One slice of the commit graph (in the pack plugin). Returns true while more is pending. */
static bool graph_slice(HubReq* r, const char* name, PackMark* m, bool* blocked) {
    if(!hub_plugin_fits(HUB_PL_PACK)) {
        *blocked = true;
        return false;
    }
    char dir[GIT_NAME_MAX + 40];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    PackClock k = {.budget = PACK_SLICE_MS};
    HubGitGraphResult gr;
    int rc = hub_git_graph_update(r->storage, dir, pack_stop, &k, &gr);
    if(rc == HUB_GIT_ERR_NOMEM) *blocked = true;
    if(rc != 0 || !gr.more) {
        if(m) m->graph = false; /* done, or failed: the next mark tries again */
        return false;
    }
    return true;
}

/* One slice of work for a repo: merge small packs first (when 4 or more, or a
 * merge job is running), else pack loose objects, else update the commit graph.
 * Returns true while more work is likely. m is the repo's idle state (NULL:
 * nothing is kept between calls). */
#define PACK_HARD_CAP 60 /* no new packs above this many; merging goes on */
#define PACK_MERGE_AT 4

static bool work_slice(
    HubReq* r,
    const char* name,
    PackMark* m,
    unsigned* packed,
    unsigned* merged,
    bool* blocked) {
    char dir[GIT_NAME_MAX + 40];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    unsigned total = 0, nsmall = 0;
    hub_git_pack_stats(r->storage, dir, 0, &total, &nsmall);

    if((m && m->merge) || nsmall >= PACK_MERGE_AT) {
        if(!hub_plugin_fits(HUB_PL_PACK)) {
            *blocked = true;
            return false;
        }
        HubGitMergeCursor* mc = m ? m->merge : NULL;
        if(!mc) {
            mc = calloc(1, sizeof(HubGitMergeCursor));
            if(!mc) return false;
            if(m) m->merge = mc;
        }
        PackClock k = {.budget = PACK_SLICE_MS};
        HubGitMergeResult mres;
        memset(&mres, 0, sizeof(mres));
        int rc = hub_git_pack_merge(r->storage, dir, mc, 0, 0, pack_stop, &k, &mres);
        if(rc != 0 || !mres.more) {
            free(mc);
            if(m) m->merge = NULL;
        }
        if(mres.merged_packs > 0) {
            *merged += mres.merged_packs;
            pack_regen_packs(r, name); /* the packs list changed */
        }
        if(rc == HUB_GIT_ERR_NOMEM) *blocked = true;
        if(rc != 0) return false;
        if(mres.more || mres.merged_packs > 0) return true;
        /* nothing could be merged (for example shared objects): go on to loose objects */
    }
    if(total >= PACK_HARD_CAP && !(m && m->cursor.job)) return false;
    if(!hub_plugin_fits(HUB_PL_PACK)) { /* the plugin and a pack slice (36 KB, host peak 24 KB) */
        *blocked = true;
        return false;
    }

    HubGitPackResult res;
    bool nomem = false;
    bool more = pack_slice(r, name, m ? &m->cursor : NULL, PACK_SLICE_MS, &res, &nomem);
    *packed += res.packed;
    if(nomem) {
        *blocked = true; /* the allocator refused a block: try again later */
        return false;
    }
    if(more) return true;
    if(m && m->rescan && !m->cursor.job) { /* objects came in during the job */
        m->rescan = false;
        m->cursor.dir = 0;
        return true;
    }
    if(m && m->cursor.job) return true; /* a pack is still open */
    if(m && m->graph) return graph_slice(r, name, m, blocked);
    return false;
}

/* Called by the worker when nothing has come in for a while. One slice. */
void hub_git_idle_tick(Storage* storage) {
    if(!PACK_AUTO || !pack_marks) return;
    if(furi_get_tick() < 60000) return; /* let the app settle after the start */
    if(!hub_sd_free_ok(storage, 0)) return;
    uint32_t now = furi_get_tick();
    for(size_t i = 0; i < PACK_MARKS; i++) {
        PackMark* m = &pack_marks[i];
        if(!m->name[0]) continue;
        if(m->last_write && now - m->last_write < PACK_QUIET_MS) continue;
        if(hub_dav_repo_locked(m->name)) continue;
        HubReq* r = malloc(sizeof(HubReq));
        if(!r) return;
        memset(r, 0, sizeof(HubReq));
        r->storage = storage;
        char name[GIT_NAME_MAX + 1];
        strcpy(name, m->name);
        unsigned packed = 0, merged = 0;
        bool blocked = false;
        hub_activity = HUB_ACT_PACK;
        bool more = work_slice(r, name, m, &packed, &merged, &blocked);
        hub_activity = HUB_ACT_IDLE;
        if(blocked) {
            m->last_write = furi_get_tick(); /* not enough heap now: look again in 30 s */
        } else if(!more) {
            pack_unmark(name);
        } /* done, nothing to do, or failed: retry at the next start */
        free(r);
        return; /* one slice only */
    }
}

/* POST /api/git/pack?name= : slices until done or the time is up.
 * Answer: {"ok":true,"packed":N,"merged":M,"remaining":0|1,"more":bool} */
static void git_route_pack(GitCtx* g) {
    if(!hub_plugin_fits(HUB_PL_PACK)) {
        hub_send_error(g->r, 503, "low memory, try again");
        return;
    }
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_sd_ok(g->r, 0)) return;
    hub_git_mark_loose(g->name); /* make sure the idle state (cursors) exists */
    PackMark* mk = pack_find(g->name);
    uint32_t end = furi_get_tick() + PACK_API_MS;
    unsigned packed = 0, merged = 0;
    bool more, blocked = false;
    for(;;) {
        char name[GIT_NAME_MAX + 1];
        strcpy(name, g->name);
        more = work_slice(g->r, name, mk, &packed, &merged, &blocked);
        if(!more) break;
        hub_beat();
        if((int32_t)(furi_get_tick() - end) >= 0) break;
    }
    if(blocked && packed == 0 && merged == 0) {
        hub_send_error(g->r, 503, "low memory, try again");
        return;
    }
    if(!more && !blocked) pack_unmark(g->name);
    char out[110];
    int n = snprintf(
        out,
        sizeof(out),
        "{\"ok\":true,\"packed\":%u,\"merged\":%u,\"remaining\":%u,\"more\":%s}",
        packed,
        merged,
        more ? 1u : 0u,
        more ? "true" : "false");
    hub_send_body(g->r, 200, "application/json", out, (size_t)n);
}
