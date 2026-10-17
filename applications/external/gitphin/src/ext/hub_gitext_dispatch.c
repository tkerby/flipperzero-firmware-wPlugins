/* Plugin git routes: the dispatcher and the plugin entry.
 * Part of hub_gitext.c: included there, in this order, not built alone. */
#include "hub_gitext_int.h"

/* ---------- dispatcher ---------- */

bool hub_gitext_route(HubReq* r) {
    if(strncmp(r->path, "/api/git/", 9) != 0) return false;
    const char* name = r->path + 9;
    bool get = strcmp(r->method, "GET") == 0;
    bool post = strcmp(r->method, "POST") == 0;
    UNUSED(get);
    UNUSED(post);

    void (*fn)(Ext*) = NULL;
    bool allowed = false;
#ifdef HUB_EXT_QUERY
    if(strcmp(name, "diff") == 0) {
        fn = route_diff;
        allowed = get;
    } else if(strcmp(name, "compare") == 0) {
        fn = route_compare;
        allowed = get;
    } else if(strcmp(name, "search") == 0) {
        fn = route_search;
        allowed = get;
    } else if(strcmp(name, "log") == 0) {
        fn = route_log;
        allowed = get;
    } else if(strcmp(name, "commit") == 0) {
        fn = route_commit;
        allowed = get;
    } else if(strcmp(name, "storage") == 0) {
        fn = route_storage;
        allowed = get;
    }
#endif
#ifdef HUB_EXT_WRITE
    if(strcmp(name, "commit-file") == 0) {
        fn = route_commit_file;
        allowed = post;
    } else if(strcmp(name, "merge") == 0) {
        fn = route_merge;
        allowed = post;
    }
#endif
    if(!fn) return false;
    if(!allowed) {
        hub_send_error(r, 405, "method not allowed");
        return true;
    }
    Ext* e = malloc(sizeof(Ext));
    if(!e) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(e, 0, sizeof(Ext));
    e->r = r;
#ifdef HUB_EXT_QUERY
    bool timed = fn != route_search && fn != route_storage;
#else
    bool timed = fn == route_merge;
#endif
    if(timed) hub_deadline_start(EXT_DEADLINE_MS);
    fn(e);
    if(timed) hub_deadline_clear();
    if(e->g) hub_git_close(e->g);
#ifdef HUB_EXT_WRITE
    if(e->name[0]) hub_git_mark_loose(e->name);
#endif
    free(e);
    return true;
}

static const HubPluginRoute ext_ops = {.route = hub_gitext_route};
#ifdef HUB_EXT_QUERY
HUB_PLUGIN_ENTRY(hub_pl_query_ep, &ext_ops)
#else
HUB_PLUGIN_ENTRY(hub_pl_write_ep, &ext_ops)
#endif
