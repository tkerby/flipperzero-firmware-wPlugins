/* WebDAV: lock tokens, the URL parser, small helpers.
 * Part of hub_dav.c: included there, in this order, not built alone. */
#include "hub_dav_int.h"

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
