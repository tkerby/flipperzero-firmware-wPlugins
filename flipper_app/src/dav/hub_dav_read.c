/* WebDAV: OPTIONS, HEAD, PROPFIND.
 * Part of hub_dav.c: included there, in this order, not built alone. */
#include "hub_dav_int.h"

/* ---------- OPTIONS ---------- */

static void dav_options(HubReq* r) {
    dav_reply(
        r,
        200,
        NULL,
        NULL,
        "DAV: 1, 2\r\n"
        "Allow: OPTIONS, GET, PROPFIND, MKCOL, PUT, MOVE, DELETE, LOCK, UNLOCK\r\n");
}

/* ---------- HEAD ---------- */

static void dav_head(HubReq* r, Dav* d) {
    FileInfo info;
    if(storage_common_stat(r->storage, d->path, &info) != FSE_OK || file_info_is_dir(&info)) {
        dav_reply(r, 404, NULL, NULL, NULL);
        return;
    }
    bool text = strcmp(d->rest, "info/refs") == 0 || strcmp(d->rest, "HEAD") == 0 ||
                strcmp(d->rest, "objects/info/packs") == 0;
    hub_send_head(r, 200, text ? "text/plain" : "application/octet-stream", info.size, NULL);
}

/* ---------- PROPFIND ---------- */

/* The answer can be big (objects/ has up to 256 folders) and the heap is small,
 * so it is made twice: once to count the bytes, once to send them. */
typedef struct {
    HubReq* r;
    FuriString* buf;
    size_t total;
    bool count_only;
} Sink;

static void sink_flush(Sink* k) {
    hub_beat();
    if(!k->count_only && furi_string_size(k->buf)) {
        hub_conn_write(k->r->t, furi_string_get_cstr(k->buf), furi_string_size(k->buf));
    }
    furi_string_reset(k->buf);
}

static void sink_end_chunk(Sink* k, FuriString* s) {
    k->total += furi_string_size(s);
    if(!k->count_only) {
        furi_string_cat(k->buf, s);
        if(furi_string_size(k->buf) >= DAV_FLUSH_AT) sink_flush(k);
    }
    furi_string_reset(s);
}

static void dav_entry(
    Sink* k,
    FuriString* s,
    const Dav* d,
    const char* child,
    bool is_dir,
    uint64_t size,
    bool full) {
    furi_string_cat_printf(s, "<D:response><D:href>/git/%s.git/", d->name);
    if(d->rest[0]) furi_string_cat_printf(s, "%s/", d->rest);
    if(child) furi_string_cat_str(s, child);
    if(is_dir && child) furi_string_push_back(s, '/');
    furi_string_cat_str(s, "</D:href><D:propstat><D:prop><D:resourcetype>");
    if(is_dir) furi_string_cat_str(s, "<D:collection/>");
    furi_string_cat_str(s, "</D:resourcetype>");
    if(!is_dir)
        furi_string_cat_printf(
            s, "<D:getcontentlength>%lu</D:getcontentlength>", (unsigned long)size);
    if(full) {
        furi_string_cat_str(
            s,
            "<D:supportedlock><D:lockentry><D:lockscope><D:exclusive/></D:lockscope>"
            "<D:locktype><D:write/></D:locktype></D:lockentry></D:supportedlock>"
            "<D:lockdiscovery/>");
    }
    furi_string_cat_str(
        s, "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>\n");
    sink_end_chunk(k, s);
}

static void
    dav_propfind_body(HubReq* r, const Dav* d, bool is_dir, uint64_t size, bool depth1, Sink* k) {
    FuriString* s = furi_string_alloc_set("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                          "<D:multistatus xmlns:D=\"DAV:\">\n");
    sink_end_chunk(k, s);
    dav_entry(k, s, d, NULL, is_dir, size, true);

    if(is_dir && depth1) {
        File* dir = storage_file_alloc(r->storage);
        if(storage_dir_open(dir, d->path)) {
            FileInfo info;
            char name[128];
            while(storage_dir_read(dir, &info, name, sizeof(name))) {
                dav_entry(k, s, d, name, file_info_is_dir(&info), info.size, false);
            }
        }
        storage_dir_close(dir);
        storage_file_free(dir);
    }
    furi_string_set(s, "</D:multistatus>\n");
    sink_end_chunk(k, s);
    furi_string_free(s);
}

static void dav_propfind(HubReq* r, Dav* d) {
    FileInfo info;
    if(storage_common_stat(r->storage, d->path, &info) != FSE_OK) {
        dav_reply(r, 404, NULL, "not found", NULL);
        return;
    }
    bool is_dir = file_info_is_dir(&info);
    bool depth1 = !(r->h_depth && r->h_depth[0] == '0');

    Sink k = {.r = r, .buf = furi_string_alloc(), .total = 0, .count_only = true};
    dav_propfind_body(r, d, is_dir, info.size, depth1, &k);
    if(hub_send_head(r, 207, "text/xml; charset=utf-8", k.total, NULL)) {
        k.count_only = false;
        dav_propfind_body(r, d, is_dir, info.size, depth1, &k);
        sink_flush(&k);
    }
    furi_string_free(k.buf);
}
