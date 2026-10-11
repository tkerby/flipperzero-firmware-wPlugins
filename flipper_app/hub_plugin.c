#include "hub_plugin.h"
#include "hub_git.h"
#include "hub_gitobj.h"
#include "hub_gitio.h"
#include "hub_transport.h"
#include "hub_gitwrite.h"

#include <furi.h>
#include <loader/firmware_api/firmware_api.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <flipper_application/plugins/composite_resolver.h>
#include <flipper_application/api_hashtable/api_hashtable.h>
#include <string.h>
#include <stdio.h>
#include "hub_mem.h"

#define PL_DIR     "/ext/apps_assets/gitphin/plugins/"
#define PL_IDLE_MS 5000

/* mem: size of the loaded plugin (text + data + bss + loader work), bytes.
 * need: most the plugin's routes take from the heap while they run. */
static const struct {
    const char* file;
    uint32_t mem;
    uint32_t need;
} pl_info[HUB_PL_COUNT] = {
    [HUB_PL_DAV] = {"hub_pl_dav.fal", 7 * 1024, 12 * 1024},
    [HUB_PL_PACK] = {"hub_pl_pack.fal", 16 * 1024, 36 * 1024},
    [HUB_PL_WRITE] = {"hub_pl_write.fal", 17 * 1024, 42 * 1024},
    [HUB_PL_QUERY] = {"hub_pl_query.fal", 14 * 1024, 32 * 1024},
    [HUB_PL_DOC] = {"hub_pl_doc.fal", 5 * 1024, 16 * 1024},
};

DavLock hub_dav_locks[DAV_LOCKS];

/* Is a WebDAV lock held on this repo (a push is going on)? */
bool hub_dav_repo_locked(const char* name) {
    uint32_t repo = 2166136261u;
    for(const char* p = name; *p; p++)
        repo = (repo ^ (uint8_t)*p) * 16777619u;
    uint32_t now = furi_get_tick() / furi_kernel_get_tick_frequency();
    for(size_t i = 0; i < DAV_LOCKS; i++) {
        if(hub_dav_locks[i].used && hub_dav_locks[i].repo == repo &&
           (int32_t)(hub_dav_locks[i].expires - now) > 0)
            return true;
    }
    return false;
}

/* Same as in hub_gitwrite.c (that file lives in a plugin, the query plugin needs this). */
int hub_gitwrite_ent_cmp(
    const char* an,
    size_t al,
    bool a_tree,
    const char* bn,
    size_t bl,
    bool b_tree) {
    size_t m = al < bl ? al : bl;
    int c = memcmp(an, bn, m);
    if(c != 0) return c < 0 ? -1 : 1;
    unsigned c1 = al > m ? (unsigned char)an[m] : (a_tree ? '/' : 0);
    unsigned c2 = bl > m ? (unsigned char)bn[m] : (b_tree ? '/' : 0);
    if(c1 == c2) return 0;
    return c1 < c2 ? -1 : 1;
}

/* ---------- api table: the core functions a plugin may call ---------- */

typedef struct {
    const char* name;
    const void* addr;
} ApiEntry;

#define API(x) \
    { #x, (const void*)&(x) }

static const ApiEntry hub_api_table[] = {
#include "hub_api_table.inc"
    {NULL, NULL}};

static bool hub_api_resolve(const ElfApiInterface* iface, uint32_t hash, Elf32_Addr* address) {
    UNUSED(iface);
    for(const ApiEntry* e = hub_api_table; e->name; e++) {
        if(elf_symbolname_hash(e->name) == hash) {
            *address = (Elf32_Addr)e->addr;
            return true;
        }
    }
    return false;
}

static const ElfApiInterface hub_api_iface = {
    .api_version_major = 0,
    .api_version_minor = 0,
    .resolver_callback = hub_api_resolve,
};

/* ---------- loading ---------- */

static PluginManager* pl_pm;
static CompositeApiResolver* pl_res;
static int pl_cur = -1;
static const void* pl_ops;
static uint32_t pl_last;

void hub_plugin_unload(void) {
    if(pl_pm) plugin_manager_free(pl_pm);
    if(pl_res) composite_api_resolver_free(pl_res);
    pl_pm = NULL;
    pl_res = NULL;
    pl_cur = -1;
    pl_ops = NULL;
}

void hub_plugin_tick(void) {
    if(pl_cur >= 0 && furi_get_tick() - pl_last >= PL_IDLE_MS) hub_plugin_unload();
}

bool hub_plugin_fits(HubPluginId id) {
    size_t need = pl_info[id].need + HUB_HEAP_RESERVE;
    if(pl_cur == (int)id) {
        return memmgr_heap_get_max_free_block() >= pl_info[id].need * 3 / 4 &&
               memmgr_get_free_heap() >= need;
    }
    /* another plugin would be unloaded first: count its memory as free */
    size_t freed = pl_cur >= 0 ? pl_info[pl_cur].mem : 0;
    return memmgr_get_free_heap() + freed >= need + pl_info[id].mem &&
           memmgr_heap_get_max_free_block() + freed >= pl_info[id].mem + 2048;
}

const void* hub_plugin_use(HubPluginId id, HubReq* r) {
    pl_last = furi_get_tick();
    if(pl_cur == (int)id) return pl_ops;
    hub_plugin_unload();
    if(hub_plugin_fits(id)) {
        pl_res = composite_api_resolver_alloc();
        composite_api_resolver_add(pl_res, firmware_api_interface);
        composite_api_resolver_add(pl_res, &hub_api_iface);
        pl_pm = plugin_manager_alloc(
            HUB_PLUGIN_APPID, HUB_PLUGIN_API, composite_api_resolver_get(pl_res));
        char path[80];
        snprintf(path, sizeof(path), PL_DIR "%s", pl_info[id].file);
        if(plugin_manager_load_single(pl_pm, path) == PluginManagerErrorNone &&
           plugin_manager_get_count(pl_pm) == 1) {
            pl_ops = plugin_manager_get_ep(pl_pm, 0);
            pl_cur = (int)id;
            pl_last = furi_get_tick();
            return pl_ops;
        }
        hub_plugin_unload();
        FURI_LOG_E("Gitphin", "cannot load %s", path);
        if(r) hub_send_error(r, 500, "plugin missing or does not fit, install the app again");
        return NULL;
    }
    if(r) hub_send_error(r, 503, "low memory, try again");
    return NULL;
}

bool hub_plugin_dispatch(HubReq* r, HubPluginId id) {
    const HubPluginRoute* ops = hub_plugin_use(id, r);
    if(ops) {
        ops->route(r);
        pl_last = furi_get_tick();
    }
    return true;
}

HubPluginId hub_plugin_for(const HubReq* r) {
    if(strncmp(r->path, "/api/git/", 9) != 0) return HUB_PL_COUNT;
    const char* n = r->path + 9;
    if(!strcmp(n, "doc") || !strcmp(n, "doc-delete") || !strcmp(n, "docs")) return HUB_PL_DOC;
    if(!strcmp(n, "diff") || !strcmp(n, "compare") || !strcmp(n, "search") || !strcmp(n, "log") ||
       !strcmp(n, "commit") || !strcmp(n, "storage"))
        return HUB_PL_QUERY;
    if(!strcmp(n, "commit-file") || !strcmp(n, "merge")) return HUB_PL_WRITE;
    return HUB_PL_COUNT;
}

/* ---------- pack plugin: same names as hub_gitpack.c, for the callers in the core ---------- */

int hub_git_pack_loose(
    void* io,
    const char* repo_dir,
    unsigned max_objects,
    size_t ram_limit,
    HubGitPackCursor* cursor,
    HubGitPackProgress progress,
    HubGitPackStop stop,
    void* ctx,
    HubGitPackResult* res) {
    const HubPluginPack* p = hub_plugin_use(HUB_PL_PACK, NULL);
    if(!p) return HUB_GIT_ERR_NOMEM;
    int rc = p->pack_loose(io, repo_dir, max_objects, ram_limit, cursor, progress, stop, ctx, res);
    pl_last = furi_get_tick();
    return rc;
}

int hub_git_pack_stats(
    void* io,
    const char* repo_dir,
    uint32_t small_limit,
    unsigned* total,
    unsigned* nsmall) {
    const HubPluginPack* p = hub_plugin_use(HUB_PL_PACK, NULL);
    if(!p) return HUB_GIT_ERR_NOMEM;
    return p->pack_stats(io, repo_dir, small_limit, total, nsmall);
}

int hub_git_pack_merge(
    void* io,
    const char* repo_dir,
    HubGitMergeCursor* cur,
    uint32_t small_limit,
    unsigned max_srcs,
    HubGitPackStop stop,
    void* ctx,
    HubGitMergeResult* res) {
    const HubPluginPack* p = hub_plugin_use(HUB_PL_PACK, NULL);
    if(!p) return HUB_GIT_ERR_NOMEM;
    int rc = p->pack_merge(io, repo_dir, cur, small_limit, max_srcs, stop, ctx, res);
    pl_last = furi_get_tick();
    return rc;
}

int hub_git_graph_update(
    void* io,
    const char* repo_dir,
    HubGitPackStop stop,
    void* ctx,
    HubGitGraphResult* res) {
    const HubPluginPack* p = hub_plugin_use(HUB_PL_PACK, NULL);
    if(!p) return HUB_GIT_ERR_NOMEM;
    int rc = p->graph_update(io, repo_dir, stop, ctx, res);
    pl_last = furi_get_tick();
    return rc;
}
