#define HUB_PLUGIN_BUILD
#include "../git/hub_git.h"
#include "../http/hub_plugin.h"

#include <furi_hal.h>
#include "../http/hub_mem.h"

/* The WebDAV subset that `git http-push` uses, under /git/<name>.git/.
 * The PC git does all the object work; here we only store files.
 * One user: locks are bookkeeping only (not enforced), kept in RAM, they expire. */

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_dav_int.h"
#include "hub_dav_util.c"
#include "hub_dav_read.c"
#include "hub_dav_write.c"
#include "hub_dav_lock.c"

/* ---------- dispatcher ---------- */

bool hub_dav_route(HubReq* r) {
    const char* m = r->method;
    bool known = !strcmp(m, "OPTIONS") || !strcmp(m, "HEAD") || !strcmp(m, "PROPFIND") ||
                 !strcmp(m, "MKCOL") || !strcmp(m, "PUT") || !strcmp(m, "MOVE") ||
                 !strcmp(m, "DELETE") || !strcmp(m, "LOCK") || !strcmp(m, "UNLOCK");
    if(!known) return false;

    Dav* d = malloc(sizeof(Dav));
    if(!d) {
        dav_reply(r, 503, NULL, "low memory, try again", NULL);
        return true;
    }
    memset(d, 0, sizeof(Dav));
    if(!dav_parse(r->path, d)) {
        dav_reply(r, 400, NULL, "bad path", NULL);
    } else if(!strcmp(m, "OPTIONS")) {
        dav_options(r);
    } else if(!strcmp(m, "HEAD")) {
        dav_head(r, d);
    } else if(!strcmp(m, "PROPFIND")) {
        dav_propfind(r, d);
    } else if(!strcmp(m, "MKCOL")) {
        dav_mkcol(r, d);
    } else if(!strcmp(m, "PUT")) {
        dav_put(r, d);
    } else if(!strcmp(m, "MOVE")) {
        dav_move(r, d);
    } else if(!strcmp(m, "DELETE")) {
        dav_delete(r, d);
    } else if(!strcmp(m, "LOCK")) {
        dav_lock(r, d);
    } else {
        dav_unlock(r, d);
    }
    free(d);
    return true;
}

static const HubPluginRoute dav_ops = {.route = hub_dav_route};
HUB_PLUGIN_ENTRY(hub_pl_dav_ep, &dav_ops)
