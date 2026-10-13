#define HUB_PLUGIN_BUILD
#include "hub_plugin.h"
#include "hub_gitpack.h"
#include "hub_gitgraph.h"
#include "hub_mem.h"

/* Entry of the pack plugin (hub_gitpack.c + hub_deflate.c + hub_sha1.c). */
static const HubPluginPack pack_ops = {
    .pack_loose = hub_git_pack_loose,
    .pack_merge = hub_git_pack_merge,
    .pack_stats = hub_git_pack_stats,
    .graph_update = hub_git_graph_update,
};

HUB_PLUGIN_ENTRY(hub_pl_pack_ep, &pack_ops)
