#pragma once

/* Plugins (.fal) that the resident core loads on demand, one at a time.
 * The core exports its functions to them through hub_plugin.c (api table). */

#include "hub_req.h"
#include "hub_gitpack.h"
#include "hub_gitgraph.h"

#include <flipper_application/flipper_application.h>

typedef enum {
    HUB_PL_DAV, /* WebDAV push */
    HUB_PL_PACK, /* pack loose objects, merge packs */
    HUB_PL_WRITE, /* commit-file, merge */
    HUB_PL_QUERY, /* diff, compare, search */
    HUB_PL_DOC, /* issues, wiki ... documents */
    HUB_PL_COUNT
} HubPluginId;

#define HUB_PLUGIN_APPID "gitphin_plugin"
#define HUB_PLUGIN_API   1

/* What a plugin offers (entry_point of its descriptor). */
typedef struct {
    bool (*route)(HubReq* r);
} HubPluginRoute;

typedef struct {
    __typeof__(hub_git_pack_loose)* pack_loose;
    __typeof__(hub_git_pack_merge)* pack_merge;
    __typeof__(hub_git_pack_stats)* pack_stats;
    __typeof__(hub_git_graph_update)* graph_update;
} HubPluginPack;

#define HUB_PLUGIN_ENTRY(epname, ops)                                                       \
    static const FlipperAppPluginDescriptor epname##_desc = {                               \
        .appid = HUB_PLUGIN_APPID, .ep_api_version = HUB_PLUGIN_API, .entry_point = (ops)}; \
    const FlipperAppPluginDescriptor* epname(void) {                                        \
        return &epname##_desc;                                                              \
    }

/* A WebDAV lock is a key hash and four random words (the token is rebuilt from
 * them). The table lives in the core, so locks survive an unload of the plugin. */
#define DAV_LOCKS 4
typedef struct {
    bool used;
    uint32_t expires; /* seconds */
    uint32_t key;
    uint32_t repo; /* hash of the repo name */
    uint32_t tok[4];
} DavLock;
extern DavLock hub_dav_locks[DAV_LOCKS];

#ifndef HUB_PLUGIN_BUILD /* core only */
/* Load the plugin (unloads another one first). The heap guard runs before the
 * load; on failure NULL (and, when r is set, 503 "low memory, try again"). */
const void* hub_plugin_use(HubPluginId id, HubReq* r);
/* Would the plugin and the job it does fit now? */
bool hub_plugin_fits(HubPluginId id);
void hub_plugin_unload(void);
void hub_plugin_tick(void); /* worker: unload a plugin that was not used for a while */
/* Which plugin serves this request? HUB_PL_COUNT = none (a core route). */
HubPluginId hub_plugin_for(const HubReq* r);
/* Load the plugin and run its route. Always true (the answer is sent). */
bool hub_plugin_dispatch(HubReq* r, HubPluginId id);
#endif
