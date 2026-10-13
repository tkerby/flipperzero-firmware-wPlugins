#define HUB_PLUGIN_BUILD
#include "hub_git.h"
#include "hub_plugin.h"

#include <furi_hal.h>
#include "hub_mem.h"

/* The WebDAV subset that `git http-push` uses, under /git/<name>.git/.
 * The PC git does all the object work; here we only store files.
 * One user: locks are bookkeeping only (not enforced), kept in RAM, they expire. */

#define DAV_NAME_MAX  64
#define DAV_REST_MAX  150
#define DAV_PATH_MAX  320
#define DAV_LOCK_SECS 600
#define DAV_FLUSH_AT  1024

typedef struct {
    char name[DAV_NAME_MAX + 1];
    char rest[DAV_REST_MAX + 1]; /* no leading or trailing "/", may be empty */
    bool trailing; /* the URL ended with "/" */
    char base[DAV_NAME_MAX + 40];
    char path[DAV_PATH_MAX]; /* file system path of rest */
    char path2[DAV_PATH_MAX + 8]; /* MOVE target, or scratch */
} Dav;

#define dav_locks hub_dav_locks /* the table lives in the core */

static void dav_token(const DavLock* l, char* out, size_t size) {
    snprintf(
        out,
        size,
        "opaquelocktoken:%08lx-%04lx-%04lx-%04lx-%08lx%04lx",
        (unsigned long)l->tok[0],
        (unsigned long)l->tok[1] & 0xffff,
        (unsigned long)l->tok[1] >> 16,
        (unsigned long)l->tok[2] & 0xffff,
        (unsigned long)l->tok[3],
        (unsigned long)l->tok[2] >> 16);
}

static uint32_t dav_now(void) {
    return furi_get_tick() / furi_kernel_get_tick_frequency();
}

static uint32_t dav_name_hash(const char* name) {
    uint32_t h = 2166136261u;
    for(const char* p = name; *p; p++)
        h = (h ^ (uint8_t)*p) * 16777619u;
    return h;
}

static uint32_t dav_hash(const char* name, const char* rest) {
    uint32_t h = 2166136261u;
    for(const char* p = name; *p; p++)
        h = (h ^ (uint8_t)*p) * 16777619u;
    h = (h ^ '/') * 16777619u;
    for(const char* p = rest; *p; p++)
        h = (h ^ (uint8_t)*p) * 16777619u;
    return h;
}

/* ---------- url ---------- */

/* "/git/<name>.git/<rest>" -> name, rest. Rejects "..", "." and empty segments. */
static bool dav_parse(const char* url, Dav* d) {
    if(strncmp(url, "/git/", 5) != 0) return false;
    const char* p = url + 5;
    size_t seg = strcspn(p, "/");
    if(seg < 5 || seg > DAV_NAME_MAX + 4 || strncmp(p + seg - 4, ".git", 4) != 0) return false;
    memcpy(d->name, p, seg - 4);
    d->name[seg - 4] = '\0';
    if(!hub_git_name_ok(d->name)) return false;

    const char* rest = p + seg;
    if(*rest == '/') rest++;
    size_t len = strlen(rest);
    d->trailing = (seg == strlen(p)) || (len > 0 && rest[len - 1] == '/') || len == 0;
    if(len > 0 && rest[len - 1] == '/') len--;
    if(len > DAV_REST_MAX || memchr(rest, '\\', len) || memchr(rest, '?', len)) return false;
    memcpy(d->rest, rest, len);
    d->rest[len] = '\0';
    for(const char* s = d->rest; *s;) {
        size_t n = strcspn(s, "/");
        if(n == 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.'))
            return false;
        s += n + (s[n] == '/' ? 1 : 0);
    }
    snprintf(d->base, sizeof(d->base), HUB_GIT_ROOT "/%s.git", d->name);
    if(len) {
        snprintf(d->path, sizeof(d->path), "%s/%s", d->base, d->rest);
    } else {
        snprintf(d->path, sizeof(d->path), "%s", d->base);
    }
    return true;
}

static bool dav_in(const Dav* d, const char* dir) {
    return strncmp(d->rest, dir, strlen(dir)) == 0;
}

/* ---------- small helpers ---------- */

/* Answer with no body (or a short one). */
static void
    dav_reply(HubReq* r, int code, const char* ctype, const char* body, const char* extra) {
    size_t len = body ? strlen(body) : 0;
    if(hub_send_head(r, code, ctype ? ctype : "text/plain", len, extra) && len) {
        hub_conn_write(r->t, body, len);
    }
}

static bool dav_exists(HubReq* r, const char* path, bool* is_dir) {
    FileInfo info;
    if(storage_common_stat(r->storage, path, &info) != FSE_OK) return false;
    if(is_dir) *is_dir = file_info_is_dir(&info);
    return true;
}

/* Make every folder above the last "/" in path, below the repo folder. */
static bool dav_mkdirs(HubReq* r, const Dav* d, const char* path) {
    char tmp[DAV_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char* p = tmp + strlen(d->base) + 1; *p; p++) {
        if(*p != '/') continue;
        *p = '\0';
        bool ok = storage_simply_mkdir(r->storage, tmp);
        *p = '/';
        if(!ok) return false;
    }
    return true;
}

/* Refs and packs changed: info/refs and objects/info/packs are rebuilt when
 * somebody next reads them (saves two SD writes per ref). */
static void dav_changed(HubReq* r, const Dav* d, const char* rest) {
    if(strncmp(rest, "refs/", 5) == 0 || strncmp(rest, "objects/pack/", 13) == 0) {
        hub_git_mark_dirty(r, d->name);
    }
    /* loose objects wait for the idle packer; any write delays it */
    if(strncmp(rest, "objects/", 8) == 0 && strncmp(rest, "objects/pack/", 13) != 0) {
        hub_git_mark_loose(d->name);
    } else {
        hub_git_touch(d->name);
    }
}

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

/* ---------- MKCOL, PUT, DELETE, MOVE ---------- */

/* Every SD operation costs 5-50 ms and a new folder about 200 ms, so each
 * handler tries the direct operation first and only checks on failure. */

static void dav_mkcol(HubReq* r, Dav* d) {
    if(d->rest[0] == '\0') {
        dav_reply(r, 405, NULL, "exists", NULL);
        return;
    }
    FS_Error err = storage_common_mkdir(r->storage, d->path);
    if(err == FSE_NOT_EXIST && dav_mkdirs(r, d, d->path)) {
        err = storage_common_mkdir(r->storage, d->path);
    }
    if(err == FSE_OK) {
        dav_reply(r, 201, NULL, NULL, NULL);
    } else if(err == FSE_EXIST) {
        dav_reply(r, 405, NULL, "exists", NULL);
    } else {
        dav_reply(r, 409, NULL, "cannot make folder", NULL);
    }
}

/* http-push sends objects as ".../objects/xx/<38 hex>_<40 hex>" and then MOVEs
 * them to ".../objects/xx/<38 hex>". We store the temp name's file under the
 * final name at once; the MOVE then has nothing left to do. Returns true and
 * the final rest if rest has that form. */
static bool dav_temp_object(const char* rest, char* final_rest) {
    size_t len = strlen(rest);
    if(len != 8 + 2 + 1 + 38 + 1 + 40 || strncmp(rest, "objects/", 8) != 0 || rest[10] != '/' ||
       rest[49] != '_')
        return false;
    for(size_t i = 8; i < len; i++) {
        char c = rest[i];
        if(i == 10 || i == 49) continue;
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    memcpy(final_rest, rest, 49);
    final_rest[49] = '\0';
    return true;
}

/* A File that failed to open is not reused: every try gets a new one. */
static File* dav_open_new(HubReq* r, const Dav* d, const char* path) {
    File* file = storage_file_alloc(r->storage);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW)) return file;
    storage_file_free(file);
    /* a missing parent folder is the usual reason; otherwise the file exists */
    if(!dav_mkdirs(r, d, path)) return NULL;
    file = storage_file_alloc(r->storage);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_NEW)) return file;
    storage_file_free(file);
    return NULL;
}

static void dav_put(HubReq* r, Dav* d) {
    if(d->rest[0] == '\0' || d->trailing) {
        dav_reply(r, 409, NULL, "bad target", NULL);
        return;
    }
    if(!hub_sd_ok(r, r->content_length)) return;
    if(r->h_expect) {
        static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
        hub_conn_write(r->t, cont, sizeof(cont) - 1);
    }

    /* temp object name -> final name (see dav_temp_object) */
    char final_rest[DAV_REST_MAX + 1];
    if(dav_temp_object(d->rest, final_rest)) {
        snprintf(d->path, sizeof(d->path), "%s/%s", d->base, final_rest);
    }

    File* file = dav_open_new(r, d, d->path);
    bool replace = false;
    if(!file) {
        /* the file exists (a ref, info/refs): write ".part", then swap */
        bool is_dir = false;
        bool exists = dav_exists(r, d->path, &is_dir);
        if(!exists || is_dir) {
            if(!exists && !dav_exists(r, d->base, NULL)) {
                dav_reply(r, 404, NULL, "repository not found", NULL);
            } else {
                dav_reply(r, 409, NULL, "bad target", NULL);
            }
            return;
        }
        replace = true;
        snprintf(d->path2, sizeof(d->path2), "%s.part", d->path);
        file = storage_file_alloc(r->storage);
        if(!storage_file_open(file, d->path2, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_free(file);
            dav_reply(r, 500, NULL, "cannot write", NULL);
            return;
        }
    }

    bool write_ok = true;
    uint32_t since_sync = 0;
    while(true) {
        size_t n = hub_body_read(r, r->io, sizeof(r->io));
        if(n == 0) break;
        if(write_ok && storage_file_write(file, r->io, n) != n) write_ok = false;
        since_sync += n;
        if(since_sync >= 256 * 1024) {
            storage_file_sync(file);
            since_sync = 0;
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    const char* tmp = replace ? d->path2 : d->path;
    if(!write_ok || r->body_read != r->content_length) {
        storage_common_remove(r->storage, tmp);
        dav_reply(
            r, write_ok ? 400 : 500, NULL, write_ok ? "upload was cut off" : "write failed", NULL);
        return;
    }
    if(replace) {
        storage_common_remove(r->storage, d->path);
        if(storage_common_rename(r->storage, d->path2, d->path) != FSE_OK) {
            storage_common_remove(r->storage, d->path2);
            dav_reply(r, 500, NULL, "rename failed", NULL);
            return;
        }
    }
    if(strcmp(d->rest, "info/refs") == 0) {
        hub_git_mark_clean(d->name); /* git just wrote the current one */
    } else {
        dav_changed(r, d, d->rest);
    }
    dav_reply(r, replace ? 204 : 201, NULL, NULL, NULL);
}

static void dav_delete(HubReq* r, Dav* d) {
    bool is_dir = false;
    if(d->rest[0] == '\0') {
        dav_reply(r, 403, NULL, "cannot delete the repository", NULL);
        return;
    }
    bool existed = dav_exists(r, d->path, &is_dir);
    bool packed = dav_in(d, "refs/heads/") || dav_in(d, "refs/tags/");
    if(!existed && !packed) {
        dav_reply(r, 404, NULL, "not found", NULL);
        return;
    }
    if(existed) {
        bool ok = is_dir ? storage_simply_remove_recursive(r->storage, d->path) :
                           storage_common_remove(r->storage, d->path) == FSE_OK;
        if(!ok) {
            dav_reply(r, 500, NULL, "delete failed", NULL);
            return;
        }
    }
    if(packed) hub_git_drop_packed(r, d->name, d->rest);
    dav_changed(r, d, d->rest);
    dav_reply(r, 204, NULL, NULL, NULL);
}

static void dav_move(HubReq* r, Dav* d) {
    Dav* to = malloc(sizeof(Dav));
    if(!to) {
        dav_reply(r, 503, NULL, "low memory, try again", NULL);
        return;
    }
    memset(to, 0, sizeof(Dav));
    uint16_t code = 0;
    const char* msg = NULL;

    /* Destination is a full URL: skip "scheme://host" */
    const char* url = r->h_destination;
    if(url && strstr(url, "://")) {
        url = strchr(strstr(url, "://") + 3, '/');
    }
    if(!url || !dav_parse(url, to) || strcmp(to->name, d->name) != 0 || to->rest[0] == '\0' ||
       d->rest[0] == '\0') {
        dav_reply(r, 400, NULL, "bad destination", NULL);
        free(to);
        return;
    }

    /* The PUT already stored this object under its final name. */
    char final_rest[DAV_REST_MAX + 1];
    if(dav_temp_object(d->rest, final_rest) && strcmp(final_rest, to->rest) == 0 &&
       dav_exists(r, to->path, NULL)) {
        dav_reply(r, 201, NULL, NULL, NULL);
        free(to);
        return;
    }

    bool overwrite = !(r->h_overwrite && (r->h_overwrite[0] == 'F' || r->h_overwrite[0] == 'f'));
    bool existed = false;
    FS_Error err = storage_common_rename(r->storage, d->path, to->path);
    if(err != FSE_OK) {
        bool src_dir = false, dst_dir = false;
        if(!dav_exists(r, d->path, &src_dir)) {
            code = 404, msg = "not found";
        } else if((existed = dav_exists(r, to->path, &dst_dir)) && !overwrite) {
            code = 412, msg = "target exists";
        } else {
            if(existed) {
                if(dst_dir) {
                    storage_simply_remove_recursive(r->storage, to->path);
                } else {
                    storage_common_remove(r->storage, to->path);
                }
            } else if(!dav_mkdirs(r, to, to->path)) {
                code = 409, msg = "no parent folder";
            }
            if(!code) {
                err = storage_common_rename(r->storage, d->path, to->path);
                if(err != FSE_OK) code = 500, msg = storage_error_get_desc(err);
            }
        }
    }
    if(code) {
        dav_reply(r, code, NULL, msg, NULL);
    } else {
        dav_changed(r, d, to->rest);
        dav_reply(r, existed ? 204 : 201, NULL, NULL, NULL);
    }
    free(to);
}

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
