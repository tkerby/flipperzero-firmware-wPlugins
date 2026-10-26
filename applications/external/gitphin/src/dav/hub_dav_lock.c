/* WebDAV: LOCK, UNLOCK.
 * Part of hub_dav.c: included there, in this order, not built alone. */
#include "hub_dav_int.h"

/* ---------- LOCK, UNLOCK ---------- */

static DavLock* dav_lock_find(uint32_t key) {
    uint32_t now = dav_now();
    for(size_t i = 0; i < DAV_LOCKS; i++) {
        if(dav_locks[i].used && (int32_t)(dav_locks[i].expires - now) <= 0)
            dav_locks[i].used = false;
        if(dav_locks[i].used && dav_locks[i].key == key) return &dav_locks[i];
    }
    return NULL;
}

static void dav_lock(HubReq* r, Dav* d) {
    uint32_t key = dav_hash(d->name, d->rest);
    char token[64];

    /* A refresh carries the old token in "If:" and no body. */
    DavLock* l = dav_lock_find(key);
    if(l) {
        dav_token(l, token, sizeof(token));
    }
    if(!(l && r->h_if && strstr(r->h_if, token))) {
        if(!l) {
            for(size_t i = 0; i < DAV_LOCKS && !l; i++) {
                if(!dav_locks[i].used) l = &dav_locks[i];
            }
            if(!l) l = &dav_locks[0]; /* table full: reuse a slot */
        }
        for(size_t i = 0; i < 4; i++)
            l->tok[i] = furi_hal_random_get();
        l->key = key;
        l->repo = dav_name_hash(d->name);
        l->used = true;
        dav_token(l, token, sizeof(token));
    }
    l->expires = dav_now() + DAV_LOCK_SECS;

    FuriString* extra = furi_string_alloc_printf("Lock-Token: <%s>\r\n", token);
    FuriString* body = furi_string_alloc_printf(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<D:prop xmlns:D=\"DAV:\"><D:lockdiscovery><D:activelock>"
        "<D:locktype><D:write/></D:locktype><D:lockscope><D:exclusive/></D:lockscope>"
        "<D:depth>0</D:depth><D:owner>flipper</D:owner>"
        "<D:timeout>Second-%d</D:timeout>"
        "<D:locktoken><D:href>%s</D:href></D:locktoken>"
        "</D:activelock></D:lockdiscovery></D:prop>\n",
        DAV_LOCK_SECS,
        token);
    dav_reply(
        r, 200, "text/xml; charset=utf-8", furi_string_get_cstr(body), furi_string_get_cstr(extra));
    furi_string_free(body);
    furi_string_free(extra);
}

static void dav_unlock(HubReq* r, Dav* d) {
    DavLock* l = dav_lock_find(dav_hash(d->name, d->rest));
    if(l && r->h_lock_token) {
        char token[64];
        dav_token(l, token, sizeof(token));
        if(strstr(r->h_lock_token, token)) l->used = false;
    }
    /* the push is over when info/refs is unlocked: leave nothing stale */
    if(strcmp(d->rest, "info/refs") == 0) hub_git_flush_r(r);
    dav_reply(r, 204, NULL, NULL, NULL);
}
