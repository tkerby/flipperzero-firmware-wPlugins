#define HUB_PLUGIN_BUILD
#include "hub_gitdoc.h"
#include "hub_plugin.h"
#include "hub_git.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "hub_mem.h"

#define DOC_ROOT     HUB_GIT_ROOT
#define DOC_NAME_MAX 64
#define DOC_LIST_MAX 1000

typedef struct {
    HubReq* r;
    char name[DOC_NAME_MAX + 16];
    char key[HUB_GITDOC_KEY_MAX + 40];
    char base[DOC_NAME_MAX + 48]; /* DOC_ROOT/<name>.git */
    char path[320];
    char tmp[330];
    const char* if_match;
    const char* if_none_match;
} DocCtx;

typedef struct {
    char* key;
    uint32_t size;
} DocItem;

/* ---------- helpers ---------- */

static bool doc_is_dir(DocCtx* d, const char* path) {
    FileInfo info;
    return storage_common_stat(d->r->storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

/* 0 = missing, 1 = file, 2 = folder */
static int doc_kind(DocCtx* d, const char* path) {
    FileInfo info;
    if(storage_common_stat(d->r->storage, path, &info) != FSE_OK) return 0;
    return file_info_is_dir(&info) ? 2 : 1;
}

/* CRC-32 of a whole file. */
static bool doc_file_crc(DocCtx* d, const char* path, uint32_t* crc) {
    File* f = storage_file_alloc(d->r->storage);
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING);
    uint32_t c = 0;
    while(ok) {
        size_t n = storage_file_read(f, d->r->io, HUB_IO_CHUNK);
        if(n == 0) break;
        c = hub_crc32(c, d->r->io, n);
    }
    storage_file_close(f);
    storage_file_free(f);
    *crc = c;
    return ok;
}

/* Call it before the body is read. */
static void doc_headers(DocCtx* d) {
    d->if_match = d->r->h_if_match;
    d->if_none_match = d->r->h_if_none_match;
}

/* Query "name" and "key" (or "prefix"), repo check. Sends the error itself. */
static bool doc_need_repo(DocCtx* d) {
    if(!hub_query_get(d->r->query, "name", d->name, sizeof(d->name))) {
        hub_send_error(d->r, 400, "missing name");
        return false;
    }
    if(!hub_git_name_ok(d->name)) {
        hub_send_error(d->r, 400, "bad repo name");
        return false;
    }
    snprintf(d->base, sizeof(d->base), DOC_ROOT "/%s.git", d->name);
    if(!doc_is_dir(d, d->base)) {
        hub_send_error(d->r, 404, "repository not found");
        return false;
    }
    return true;
}

static bool doc_need_key(DocCtx* d) {
    if(!hub_query_get(d->r->query, "key", d->key, sizeof(d->key))) {
        hub_send_error(d->r, 400, "missing key");
        return false;
    }
    if(!hub_gitdoc_key_ok(d->key)) {
        hub_send_error(d->r, 400, "bad key");
        return false;
    }
    snprintf(d->path, sizeof(d->path), "%s/hub/%s", d->base, d->key);
    return true;
}

/* Make hub/ and every folder of the key below it. */
static bool doc_mkdirs(DocCtx* d) {
    snprintf(d->tmp, sizeof(d->tmp), "%s", d->path);
    char* p = d->tmp + strlen(d->base) + 1; /* after "<base>/" */
    for(; *p; p++) {
        if(*p != '/') continue;
        *p = '\0';
        bool ok = storage_simply_mkdir(d->r->storage, d->tmp);
        *p = '/';
        if(!ok) return false;
    }
    return true;
}

/* {"ok":true,"etag":"\"xxxxxxxx\""} and the ETag header */
static void doc_send_etag_json(DocCtx* d, uint32_t crc) {
    char etag[11];
    char body[48];
    char extra[32];
    hub_gitdoc_etag(crc, etag);
    int n = snprintf(body, sizeof(body), "{\"ok\":true,\"etag\":\"\\\"%.8s\\\"\"}", etag + 1);
    snprintf(extra, sizeof(extra), "ETag: %s\r\n", etag);
    if(hub_send_head(d->r, 200, "application/json", (size_t)n, extra))
        hub_conn_write(d->r->t, body, (size_t)n);
}

/* ---------- routes ---------- */

static void doc_get(DocCtx* d) {
    if(!doc_need_repo(d) || !doc_need_key(d)) return;
    if(doc_kind(d, d->path) != 1) {
        hub_send_error(d->r, 404, "not found");
        return;
    }
    uint32_t crc;
    FileInfo info;
    File* f = storage_file_alloc(d->r->storage);
    if(!doc_file_crc(d, d->path, &crc) ||
       !storage_file_open(f, d->path, FSAM_READ, FSOM_OPEN_EXISTING) ||
       storage_common_stat(d->r->storage, d->path, &info) != FSE_OK) {
        storage_file_free(f);
        hub_send_error(d->r, 500, "read failed");
        return;
    }
    char etag[11];
    char extra[32];
    hub_gitdoc_etag(crc, etag);
    snprintf(extra, sizeof(extra), "ETag: %s\r\n", etag);
    const char* dot = strrchr(d->key, '.');
    const char* ctype = (dot && strcmp(dot, ".json") == 0) ? "application/json" :
                                                             "text/plain; charset=utf-8";
    bool ok = hub_send_head(d->r, 200, ctype, (size_t)info.size, extra);
    while(ok) {
        size_t n = storage_file_read(f, d->r->io, HUB_IO_CHUNK);
        if(n == 0) break;
        ok = hub_conn_write(d->r->t, d->r->io, n);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void doc_put(DocCtx* d) {
    HubReq* r = d->r;
    if(!doc_need_repo(d) || !doc_need_key(d)) return;
    if(!hub_sd_ok(r, r->content_length)) return;
    if(r->content_length > HUB_GITDOC_BODY_MAX) {
        hub_send_error(r, 413, "document too large");
        return;
    }
    int kind = doc_kind(d, d->path);
    if(kind == 2) {
        hub_send_error(r, 409, "key is a folder");
        return;
    }
    uint32_t cur = 0;
    if(kind == 1 && !doc_file_crc(d, d->path, &cur)) {
        hub_send_error(r, 500, "read failed");
        return;
    }
    doc_headers(d);
    int pc = hub_gitdoc_precond(d->if_match, d->if_none_match, false, kind == 1, cur);
    if(pc == 400) {
        hub_send_error(r, 400, "need If-Match or If-None-Match: *");
        return;
    }
    if(pc == 412) {
        hub_send_error(r, 412, "changed");
        return;
    }
    if(!doc_mkdirs(d)) {
        hub_send_error(r, 409, "key conflicts with another key");
        return;
    }

    snprintf(d->tmp, sizeof(d->tmp), "%s.part", d->path);
    File* f = storage_file_alloc(r->storage);
    if(!storage_file_open(f, d->tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        hub_send_error(r, 500, "write failed");
        return;
    }
    bool ok = true;
    uint32_t crc = 0;
    for(;;) {
        size_t n = hub_body_read(r, r->io, HUB_IO_CHUNK);
        if(n == 0) break;
        crc = hub_crc32(crc, r->io, n);
        if(ok && storage_file_write(f, r->io, n) != n) ok = false;
    }
    storage_file_close(f);
    storage_file_free(f);
    if(!ok || r->body_read != r->content_length) {
        storage_common_remove(r->storage, d->tmp);
        hub_send_error(r, ok ? 400 : 500, ok ? "upload was cut off" : "write failed");
        return;
    }
    /* rename does not overwrite */
    storage_common_remove(r->storage, d->path);
    if(storage_common_rename(r->storage, d->tmp, d->path) != FSE_OK) {
        storage_common_remove(r->storage, d->tmp);
        hub_send_error(r, 500, "write failed");
        return;
    }
    doc_send_etag_json(d, crc);
}

static void doc_delete(DocCtx* d) {
    HubReq* r = d->r;
    if(!doc_need_repo(d) || !doc_need_key(d)) return;
    if(doc_kind(d, d->path) != 1) {
        hub_send_error(r, 404, "not found");
        return;
    }
    uint32_t cur = 0;
    if(!doc_file_crc(d, d->path, &cur)) {
        hub_send_error(r, 500, "read failed");
        return;
    }
    doc_headers(d);
    int pc = hub_gitdoc_precond(d->if_match, d->if_none_match, true, true, cur);
    if(pc == 400) {
        hub_send_error(r, 400, "need If-Match");
        return;
    }
    if(pc == 412) {
        hub_send_error(r, 412, "changed");
        return;
    }
    if(storage_common_remove(r->storage, d->path) != FSE_OK) {
        hub_send_error(r, 500, "delete failed");
        return;
    }
    hub_send_ok(r);
}

/* Insert in key order. Takes the malloc'd key. */
static void doc_list_add(DocItem* items, size_t* count, char* key, uint32_t size) {
    size_t n = *count;
    size_t pos = 0;
    while(pos < n && strcmp(items[pos].key, key) < 0)
        pos++;
    memmove(&items[pos + 1], &items[pos], (n - pos) * sizeof(DocItem));
    items[pos].key = key;
    items[pos].size = size;
    *count = n + 1;
}

static void doc_list(DocCtx* d) {
    HubReq* r = d->r;
    if(!doc_need_repo(d)) return;
    /* the key buffer holds the prefix */
    if(!hub_query_get(r->query, "prefix", d->key, sizeof(d->key))) d->key[0] = '\0';
    if(!hub_gitdoc_prefix_ok(d->key)) {
        hub_send_error(r, 400, "bad prefix");
        return;
    }
    size_t plen = strlen(d->key);
    if(plen > 0 && d->key[plen - 1] == '/') d->key[--plen] = '\0';
    if(plen > 0) {
        snprintf(d->path, sizeof(d->path), "%s/hub/%s", d->base, d->key);
    } else {
        snprintf(d->path, sizeof(d->path), "%s/hub", d->base);
    }

    DocItem* items = malloc(DOC_LIST_MAX * sizeof(DocItem));
    if(!items) {
        hub_send_error(r, 503, "low memory, try again");
        return;
    }
    size_t count = 0;
    bool full = true;
    File* dir = storage_file_alloc(r->storage);
    if(storage_dir_open(dir, d->path)) {
        FileInfo info;
        char* name = d->tmp; /* long enough for one entry name */
        while(storage_dir_read(dir, &info, name, sizeof(d->tmp))) {
            size_t nl = strlen(name);
            if(file_info_is_dir(&info)) continue;
            if(count >= DOC_LIST_MAX) break;
            /* full key = prefix + "/" + name */
            size_t off = plen ? plen + 1 : 0;
            char* buf = malloc(off + nl + 1);
            if(!buf) {
                full = false;
                break;
            }
            if(plen) {
                memcpy(buf, d->key, plen);
                buf[plen] = '/';
            }
            memcpy(buf + off, name, nl + 1);
            /* foreign names (and our own .part files) are not documents */
            if(hub_gitdoc_key_ok(buf)) {
                doc_list_add(items, &count, buf, (uint32_t)info.size);
            } else {
                free(buf);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"keys\":[");
    for(size_t i = 0; i < count; i++) {
        if(i) furi_string_push_back(s, ',');
        furi_string_cat_str(s, "{\"key\":");
        hub_json_str(s, items[i].key);
        furi_string_cat_printf(s, ",\"size\":%lu}", (unsigned long)items[i].size);
        free(items[i].key);
    }
    furi_string_cat_str(s, "]}");
    free(items);
    if(full) {
        hub_send_json(r, 200, s);
    } else {
        hub_send_error(r, 503, "low memory, try again");
    }
    furi_string_free(s);
}

/* ---------- dispatcher ---------- */

bool hub_gitdoc_route(HubReq* r) {
    if(strncmp(r->path, "/api/git/doc", 12) != 0) return false;
    const char* name = r->path + 9;
    bool get = strcmp(r->method, "GET") == 0;
    bool put = strcmp(r->method, "PUT") == 0;
    bool post = strcmp(r->method, "POST") == 0;

    void (*fn)(DocCtx*) = NULL;
    bool allowed = false;
    if(strcmp(name, "doc") == 0) {
        allowed = get || put;
        fn = get ? doc_get : doc_put;
    } else if(strcmp(name, "doc-delete") == 0) {
        allowed = post;
        fn = doc_delete;
    } else if(strcmp(name, "docs") == 0) {
        allowed = get;
        fn = doc_list;
    } else {
        return false;
    }
    if(!allowed) {
        hub_send_error(r, 405, "method not allowed");
        return true;
    }
    DocCtx* d = malloc(sizeof(DocCtx));
    if(!d) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(d, 0, sizeof(DocCtx));
    d->r = r;
    hub_git_mkroot(r->storage);
    fn(d);
    free(d);
    return true;
}

static const HubPluginRoute doc_ops = {.route = hub_gitdoc_route};
HUB_PLUGIN_ENTRY(hub_pl_doc_ep, &doc_ops)
