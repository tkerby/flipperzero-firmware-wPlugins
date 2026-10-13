#include "hub_http.h"
#include "hub_req.h"
#include "hub_git.h"
#include "hub_gitobj.h"

#include <furi_hal.h>
#include <toolbox/version.h>
#include "hub_mem.h"

#define HUB_INDEX_PATH EXT_PATH("apps_assets/gitphin/index.html")

/* ---------- small helpers ---------- */

static bool hub_ieq_prefix(const char* s, const char* prefix) {
    for(; *prefix; s++, prefix++) {
        char a = *s, b = *prefix;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(a != b) return false;
    }
    return true;
}

static const char* hub_hdr_value(const char* v) {
    while(*v == ' ' || *v == '\t')
        v++;
    return v;
}

static int hub_hex(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Find key in a query string and percent-decode its value into out. */
bool hub_query_get(const char* query, const char* key, char* out, size_t out_size) {
    if(!query) return false;
    size_t key_len = strlen(key);
    const char* p = query;
    while(*p) {
        if(strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            size_t n = 0;
            while(*p && *p != '&' && n + 1 < out_size) {
                if(*p == '%' && hub_hex(p[1]) >= 0 && hub_hex(p[2]) >= 0) {
                    out[n++] = (char)(hub_hex(p[1]) * 16 + hub_hex(p[2]));
                    p += 3;
                } else {
                    out[n++] = (*p == '+') ? ' ' : *p;
                    p++;
                }
            }
            out[n] = '\0';
            return true;
        }
        p = strchr(p, '&');
        if(!p) break;
        p++;
    }
    return false;
}

/* Only the SD card (not the internal flash), and no way out of it. */
bool hub_path_ok(const char* path) {
    bool root_ok = strncmp(path, "/ext", 4) == 0 && (path[4] == '\0' || path[4] == '/');
    return root_ok && strstr(path, "/..") == NULL;
}

volatile uint32_t hub_beat_tick;
volatile uint8_t hub_activity;
volatile bool hub_long_op;
HubProgress hub_prog;
static HubReq* hub_cur_req; /* the request being served, for hub_beat */
static uint32_t hub_last_service;

void hub_beat(void) {
    uint32_t now = furi_get_tick();
    hub_beat_tick = now;
    HubReq* r = hub_cur_req;
    /* PING frames can be read only when the request body is all in */
    if(r && now - hub_last_service >= 500 && r->body_read >= r->content_length &&
       r->pre_len == 0) {
        hub_last_service = now;
        hub_transport_service(r->t);
    }
}

static volatile uint32_t hub_deadline_tick;

void hub_deadline_start(uint32_t ms) {
    uint32_t t = furi_get_tick() + ms;
    hub_deadline_tick = t ? t : 1;
}

void hub_deadline_clear(void) {
    hub_deadline_tick = 0;
}

bool hub_deadline_expired(void) {
    uint32_t d = hub_deadline_tick;
    return d != 0 && (int32_t)(furi_get_tick() - d) >= 0;
}

bool hub_beat_hook(void) {
    hub_beat();
    return !hub_deadline_expired();
}

bool hub_sd_free_ok(Storage* storage, uint64_t incoming) {
    uint64_t total = 0, free_bytes = 0;
    if(storage_common_fs_info(storage, STORAGE_EXT_PATH_PREFIX, &total, &free_bytes) != FSE_OK)
        return false;
    return free_bytes > incoming && free_bytes - incoming >= 16ULL * 1024 * 1024;
}

bool hub_sd_ok(HubReq* r, uint64_t incoming) {
    if(hub_sd_free_ok(r->storage, incoming)) return true;
    hub_send_error(r, 507, "SD card full");
    return false;
}

/* Enough free memory (total and biggest block) for a route? Else answer 503. */
bool hub_heap_ok(HubReq* r, size_t need) {
    if(memmgr_get_free_heap() >= need + HUB_HEAP_RESERVE &&
       memmgr_heap_get_max_free_block() >= need / 4 * 3) {
        return true;
    }
    hub_send_error(r, 503, "low memory, try again");
    return false;
}

static const char* hub_reason(int code) {
    switch(code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 207:
        return "Multi-Status";
    case 404:
        return "Not Found";
    case 503:
        return "Service Unavailable";
    case 507:
        return "Insufficient Storage";
    case 405:
        return "Method Not Allowed";
    case 409:
        return "Conflict";
    case 412:
        return "Precondition Failed";
    case 413:
        return "Payload Too Large";
    case 423:
        return "Locked";
    case 431:
        return "Request Header Fields Too Large";
    default:
        return "Internal Server Error";
    }
}

static const char* hub_mime(const char* path) {
    const char* dot = strrchr(path, '.');
    if(!dot) return "application/octet-stream";
    if(strcmp(dot, ".html") == 0) return "text/html; charset=utf-8";
    if(strcmp(dot, ".css") == 0) return "text/css";
    if(strcmp(dot, ".js") == 0) return "text/javascript";
    if(strcmp(dot, ".json") == 0) return "application/json";
    if(strcmp(dot, ".png") == 0) return "image/png";
    if(strcmp(dot, ".txt") == 0 || strcmp(dot, ".md") == 0 || strcmp(dot, ".sub") == 0 ||
       strcmp(dot, ".ir") == 0 || strcmp(dot, ".nfc") == 0 || strcmp(dot, ".rfid") == 0 ||
       strcmp(dot, ".log") == 0)
        return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

/* ---------- transfer progress ---------- */

/* A request or an answer with a known length (0 = unknown). */
static void prog_len(uint32_t total) {
    HubProgress* p = &hub_prog;
    p->total = total;
    p->done = 0;
    if(p->kind == HUB_PG_NONE && total > 16384) {
        p->kind = HUB_PG_BIG;
        p->start = furi_get_tick();
        p->bytes = total;
        p->objects = 0;
    }
}

/* Size of all packs of the repo in r->path ("/git/<name>.git/..."): a clone
 * downloads them, so this is the total for the whole pull. 0 when unknown. */
static uint32_t prog_pack_bytes(HubReq* r) {
    const char* name = r->path + 5;
    const char* end = strchr(name, '/');
    if(!end || end - name > 80) return 0;
    char path[128];
    snprintf(path, sizeof(path), HUB_GIT_ROOT "/%.*s/objects/pack", (int)(end - name), name);
    File* dir = storage_file_alloc(r->storage);
    uint32_t sum = 0;
    if(storage_dir_open(dir, path)) {
        FileInfo info;
        char fn[64];
        while(storage_dir_read(dir, &info, fn, sizeof(fn))) {
            size_t n = strlen(fn);
            if(n > 5 && strcmp(fn + n - 5, ".pack") == 0) sum += (uint32_t)info.size;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return sum;
}

static void prog_begin(HubReq* r) {
    HubProgress* p = &hub_prog;
    uint32_t now = furi_get_tick();
    if(strncmp(r->path, "/git/", 5) == 0) {
        uint8_t kind = strcmp(r->method, "GET") ? HUB_PG_PUSH : HUB_PG_PULL;
        uint32_t repo = 0;
        for(const char* c = r->path + 5; *c && *c != '/'; c++)
            repo = repo * 31 + (uint8_t)*c;
        if(kind != p->kind || repo != p->repo || now - p->last >= 3000) {
            p->objects = p->bytes = 0;
            p->start = now;
            p->sess_done = 0;
            p->sess_total = kind == HUB_PG_PULL ? prog_pack_bytes(r) : 0;
        }
        p->kind = kind;
        p->repo = repo;
        p->last = now;
    } else {
        /* an API call (pack, merge, ...) is not a git transfer: no progress
         * screen; a big body or answer of its own sets HUB_PG_BIG below */
        p->kind = HUB_PG_NONE;
    }
    p->done = p->total = 0;
    if(r->content_length) prog_len(r->content_length);
}

static void prog_end(HubReq* r) {
    HubProgress* p = &hub_prog;
    if(!r->path) return; /* the head never parsed */
    if(strncmp(r->path, "/git/", 5) == 0) {
        if(p->kind == HUB_PG_PUSH) {
            if(strcmp(r->method, "PUT") == 0 && strstr(r->path, "/objects/")) p->objects++;
            p->bytes += r->body_read;
        } else {
            p->objects++;
            p->bytes += p->total;
            p->sess_done += p->total;
        }
        p->done = p->total = 0;
    } else if(p->kind == HUB_PG_BIG) {
        p->done = p->total;
    } else {
        return;
    }
    p->last = furi_get_tick();
}

/* ---------- request body ---------- */

size_t hub_body_read(HubReq* r, uint8_t* buf, size_t len) {
    size_t left = r->content_length - r->body_read;
    if(left == 0) return 0;
    len = MIN(len, left);

    size_t n;
    if(r->pre_len > 0) {
        n = MIN(len, r->pre_len);
        memcpy(buf, r->pre, n);
        r->pre += n;
        r->pre_len -= n;
    } else {
        n = hub_conn_read(r->t, buf, len, HUB_READ_TIMEOUT);
    }
    hub_beat_tick = hub_prog.last = furi_get_tick();
    r->body_read += n;
    hub_prog.done = r->body_read;
    return n;
}

/* The bridge sends the whole request first, so read it all before answering. */
static void hub_body_drain(HubReq* r) {
    while(hub_body_read(r, r->io, sizeof(r->io)) > 0) {
    }
}

/* ---------- responses ---------- */

bool hub_send_head(HubReq* r, int code, const char* ctype, size_t len, const char* extra) {
    hub_body_drain(r);
    if(len != HUB_CHUNKED && r->content_length == 0) prog_len(len);
    char cl[40];
    if(len == HUB_CHUNKED) {
        strcpy(cl, "Transfer-Encoding: chunked\r\n");
    } else {
        snprintf(cl, sizeof(cl), "Content-Length: %lu\r\n", (unsigned long)len);
    }
    FuriString* h = furi_string_alloc_printf(
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "%s"
        "%s"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        code,
        hub_reason(code),
        ctype,
        cl,
        extra && strstr(extra, "Cache-Control:") ? "" : "Cache-Control: no-store\r\n",
        extra ? extra : "");

    bool ok = hub_conn_write(r->t, furi_string_get_cstr(h), furi_string_size(h));
    furi_string_free(h);
    r->head_sent = true;
    return ok;
}

void hub_send_body(HubReq* r, int code, const char* ctype, const char* body, size_t len) {
    if(hub_send_head(r, code, ctype, len, NULL)) hub_conn_write(r->t, body, len);
}

void hub_send_json(HubReq* r, int code, FuriString* json) {
    if(code < 400 && hub_deadline_expired()) {
        hub_send_error(r, 503, "too slow on the device");
        return;
    }
    hub_send_body(r, code, "application/json", furi_string_get_cstr(json), furi_string_size(json));
}

void hub_send_error(HubReq* r, int code, const char* msg) {
    /* an aborted read after the deadline shows up as some other error */
    if(code >= 400 && hub_deadline_expired()) {
        code = 503;
        msg = "too slow on the device";
    }
    FuriString* s = furi_string_alloc_set("{\"ok\":false,\"error\":");
    hub_json_str(s, msg);
    furi_string_push_back(s, '}');
    hub_send_json(r, code, s);
    furi_string_free(s);
}

void hub_send_ok(HubReq* r) {
    static const char ok[] = "{\"ok\":true}";
    hub_send_body(r, 200, "application/json", ok, sizeof(ok) - 1);
}

/* Read the "path" query value and check it. Sends the error itself. */
static bool hub_get_path(HubReq* r, char* out) {
    if(!hub_query_get(r->query, "path", out, HUB_PATH_MAX)) {
        hub_send_error(r, 400, "missing path");
        return false;
    }
    if(!hub_path_ok(out)) {
        hub_send_error(r, 400, "path must be inside /ext");
        return false;
    }
    return true;
}

/* ---------- routes ---------- */

static void hub_route_info(HubReq* r) {
    uint64_t total = 0, free_bytes = 0;
    bool sd_ok =
        storage_common_fs_info(r->storage, STORAGE_EXT_PATH_PREFIX, &total, &free_bytes) == FSE_OK;
    const Version* ver = furi_hal_version_get_firmware_version();
    const char* name = furi_hal_version_get_name_ptr();

    FuriString* s = furi_string_alloc_set("{\"name\":");
    hub_json_str(s, name ? name : "Flipper");
    furi_string_cat_str(s, ",\"firmware\":");
    hub_json_str(s, ver ? version_get_version(ver) : "unknown");
    furi_string_cat_printf(
        s,
        ",\"battery\":%u,\"charging\":%s,\"sd_ok\":%s,\"sd_total_kib\":%lu,\"sd_free_kib\":%lu,"
        "\"heap_free\":%lu,\"uptime_s\":%lu}",
        furi_hal_power_get_pct(),
        furi_hal_power_is_charging() ? "true" : "false",
        sd_ok ? "true" : "false",
        (unsigned long)(total / 1024),
        (unsigned long)(free_bytes / 1024),
        (unsigned long)memmgr_get_free_heap(),
        (unsigned long)(furi_get_tick() / furi_kernel_get_tick_frequency()));
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

static void hub_route_list(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;

    File* dir = storage_file_alloc(r->storage);
    if(!storage_dir_open(dir, path)) {
        hub_send_error(r, 404, storage_file_get_error_desc(dir));
        storage_file_free(dir);
        return;
    }

    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"path\":");
    hub_json_str(s, path);
    furi_string_cat_str(s, ",\"entries\":[");
    FileInfo info;
    char name[HUB_PATH_MAX];
    bool first = true;
    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(!first) furi_string_push_back(s, ',');
        first = false;
        furi_string_cat_str(s, "{\"name\":");
        hub_json_str(s, name);
        furi_string_cat_printf(
            s,
            ",\"dir\":%s,\"size\":%lu}",
            file_info_is_dir(&info) ? "true" : "false",
            (unsigned long)info.size);
    }
    furi_string_cat_str(s, "]}");
    storage_dir_close(dir);
    storage_file_free(dir);

    hub_send_json(r, 200, s);
    furi_string_free(s);
}

static const char* hub_xhdr; /* extra header lines for the next hub_serve_file_as */

void hub_serve_file_as(HubReq* r, const char* path, const char* ctype, bool download) {
    FileInfo info;
    if(storage_common_stat(r->storage, path, &info) != FSE_OK) {
        hub_send_error(r, 404, "file not found");
        return;
    }
    if(file_info_is_dir(&info)) {
        hub_send_error(r, 400, "path is a directory");
        return;
    }

    File* file = storage_file_alloc(r->storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        hub_send_error(r, 500, storage_file_get_error_desc(file));
        storage_file_free(file);
        return;
    }

    FuriString* extra = NULL;
    if(download) {
        const char* base = strrchr(path, '/');
        extra = furi_string_alloc_printf(
            "Content-Disposition: attachment; filename=\"%s\"\r\n", base ? base + 1 : path);
    }

    bool ok = hub_send_head(
        r, 200, ctype, (size_t)info.size, extra ? furi_string_get_cstr(extra) : hub_xhdr);
    while(ok) {
        size_t n = storage_file_read(file, r->io, sizeof(r->io));
        if(n == 0) break;
        ok = hub_conn_write(r->t, r->io, n);
        hub_beat_tick = hub_prog.last = furi_get_tick();
        hub_prog.done += n;
    }

    if(extra) furi_string_free(extra);
    storage_file_close(file);
    storage_file_free(file);
}

void hub_serve_file(HubReq* r, const char* path, bool download) {
    hub_serve_file_as(r, path, hub_mime(path), download);
}

static void hub_route_file_get(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    char dl[4] = {0};
    bool download = hub_query_get(r->query, "dl", dl, sizeof(dl)) && dl[0] == '1';
    hub_serve_file(r, path, download);
}

/* Stream the upload to "<path>.part", then rename it to <path> only when the
 * whole body arrived and every write succeeded. A cut-off or failed upload
 * leaves the .part file, which is removed, so the real file is never corrupt
 * and an existing file is replaced only on success. */
static void hub_route_file_put(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    if(!hub_sd_ok(r, r->content_length)) return;

    char temp[HUB_PATH_MAX + 8];
    snprintf(temp, sizeof(temp), "%s.part", path);

    File* file = storage_file_alloc(r->storage);
    if(!storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        hub_send_error(r, 500, storage_file_get_error_desc(file));
        storage_file_free(file);
        return;
    }

    bool write_ok = true;
    uint32_t since_sync = 0;
    while(true) {
        size_t n = hub_body_read(r, r->io, sizeof(r->io));
        if(n == 0) break;
        if(write_ok && storage_file_write(file, r->io, n) != n) write_ok = false;
        /* Flush to the card now and then, so a long transfer does not keep all
         * of its dirty data buffered. Rare enough not to slow the stream much. */
        since_sync += n;
        if(since_sync >= 256 * 1024) {
            storage_file_sync(file);
            since_sync = 0;
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    bool complete = write_ok && r->body_read == r->content_length;
    if(!complete) {
        storage_common_remove(r->storage, temp);
        hub_send_error(r, write_ok ? 400 : 500, write_ok ? "upload was cut off" : "write failed");
        return;
    }

    /* Rename does not overwrite, so clear any existing target first. */
    storage_common_remove(r->storage, path);
    FS_Error err = storage_common_rename(r->storage, temp, path);
    if(err != FSE_OK) {
        storage_common_remove(r->storage, temp);
        hub_send_error(r, 500, storage_error_get_desc(err));
    } else {
        hub_send_ok(r);
    }
}

static void hub_route_mkdir(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    if(storage_simply_mkdir(r->storage, path)) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "could not make folder");
    }
}

static void hub_route_delete(HubReq* r) {
    char path[HUB_PATH_MAX];
    if(!hub_get_path(r, path)) return;
    FS_Error err = storage_common_remove(r->storage, path);
    if(err == FSE_OK) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, storage_error_get_desc(err));
    }
}

static void hub_route_index(HubReq* r) {
    if(r->h_accept_enc && strstr(r->h_accept_enc, "gzip") &&
       storage_common_stat(r->storage, HUB_INDEX_PATH ".gz", NULL) == FSE_OK) {
        hub_xhdr = "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n";
        hub_serve_file_as(r, HUB_INDEX_PATH ".gz", "text/html; charset=utf-8", false);
        hub_xhdr = NULL;
        return;
    }
    if(storage_common_stat(r->storage, HUB_INDEX_PATH, NULL) == FSE_OK) {
        hub_serve_file(r, HUB_INDEX_PATH, false);
        return;
    }
    static const char page[] =
        "<!doctype html><meta charset=utf-8><title>Gitphin</title>"
        "<h1>Gitphin</h1><p>The server runs, but the web page is missing. "
        "Install <code>gitphin.fap</code> again: the app unpacks the page to the SD card on start.</p>"
        "<p>The API works: <a href=/api/info>/api/info</a></p>";
    hub_send_body(r, 200, "text/html; charset=utf-8", page, sizeof(page) - 1);
}

/* ---------- request head ---------- */

static bool hub_read_head(HubReq* r) {
    while(r->head_len < HUB_HEAD_MAX) {
        size_t n = hub_conn_read(
            r->t, r->head + r->head_len, HUB_HEAD_MAX - r->head_len, HUB_READ_TIMEOUT);
        if(n == 0) return false;
        size_t from = r->head_len >= 3 ? r->head_len - 3 : 0;
        r->head_len += n;
        r->head[r->head_len] = '\0';

        for(size_t i = from; i + 3 < r->head_len; i++) {
            if(memcmp(r->head + i, "\r\n\r\n", 4) == 0) {
                r->head[i] = '\0';
                r->pre = r->head + i + 4;
                r->pre_len = r->head_len - (i + 4);
                return true;
            }
        }
    }
    return false;
}

static bool hub_parse_head(HubReq* r) {
    char* line = (char*)r->head;
    char* eol = strstr(line, "\r\n");
    char* headers = NULL;
    if(eol) {
        *eol = '\0';
        headers = eol + 2;
    }

    /* "GET /api/list?path=/ext HTTP/1.1" */
    char* sp1 = strchr(line, ' ');
    if(!sp1) return false;
    *sp1 = '\0';
    char* target = sp1 + 1;
    char* sp2 = strchr(target, ' ');
    if(sp2) *sp2 = '\0';

    r->method = line;
    r->path = target;
    char* q = strchr(target, '?');
    if(q) {
        *q = '\0';
        r->query = q + 1;
    }

    while(headers && *headers) {
        char* next = strstr(headers, "\r\n");
        if(next) *next = '\0';
        if(hub_ieq_prefix(headers, "content-length:")) {
            r->content_length = strtoul(headers + 15, NULL, 10);
        } else if(hub_ieq_prefix(headers, "depth:")) {
            r->h_depth = hub_hdr_value(headers + 6);
        } else if(hub_ieq_prefix(headers, "destination:")) {
            r->h_destination = hub_hdr_value(headers + 12);
        } else if(hub_ieq_prefix(headers, "overwrite:")) {
            r->h_overwrite = hub_hdr_value(headers + 10);
        } else if(hub_ieq_prefix(headers, "lock-token:")) {
            r->h_lock_token = hub_hdr_value(headers + 11);
        } else if(hub_ieq_prefix(headers, "if:")) {
            r->h_if = hub_hdr_value(headers + 3);
        } else if(hub_ieq_prefix(headers, "if-match:")) {
            r->h_if_match = hub_hdr_value(headers + 9);
        } else if(hub_ieq_prefix(headers, "if-none-match:")) {
            r->h_if_none_match = hub_hdr_value(headers + 14);
        } else if(hub_ieq_prefix(headers, "accept-encoding:")) {
            r->h_accept_enc = hub_hdr_value(headers + 16);
        } else if(hub_ieq_prefix(headers, "expect:")) {
            r->h_expect = hub_hdr_value(headers + 7);
        }
        headers = next ? next + 2 : NULL;
    }

    /* Body bytes that came with the head never exceed the declared length. */
    if(r->pre_len > r->content_length) r->pre_len = r->content_length;
    return true;
}

void hub_http_handle(HubTransport* t, Storage* storage, HubStats* stats) {
    HubReq* r = malloc(sizeof(HubReq));
    if(!r) {
        hub_conn_close(t);
        return;
    }
    memset(r, 0, sizeof(HubReq));
    r->t = t;
    r->storage = storage;
    hub_beat_tick = furi_get_tick();
    hub_activity = HUB_ACT_REQUEST;

    if(!hub_read_head(r)) {
        if(r->head_len >= HUB_HEAD_MAX) hub_send_error(r, 431, "request head too large");
    } else if(!hub_parse_head(r)) {
        hub_send_error(r, 400, "bad request line");
    } else {
        if(furi_mutex_acquire(stats->lock, 200) == FuriStatusOk) {
            stats->requests++;
            snprintf(stats->last, sizeof(stats->last), "%s %s", r->method, r->path);
            furi_mutex_release(stats->lock);
        }
        prog_begin(r);

        bool get = strcmp(r->method, "GET") == 0;
        bool post = strcmp(r->method, "POST") == 0;
        bool put = strcmp(r->method, "PUT") == 0;

        /* The head is parsed: from here hub_beat may answer PING frames. */
        hub_cur_req = r;
        /* routes that may run for tens of seconds: the screen allows a longer silence */
        hub_long_op = strncmp(r->path, "/api/git/", 9) == 0 &&
                      (strstr(r->path, "merge") || strstr(r->path, "compare") ||
                       strstr(r->path, "search") || strstr(r->path, "diff") ||
                       strstr(r->path, "pack") || strstr(r->path, "storage") ||
                       strstr(r->path, "commit-file"));

        if(get && (strcmp(r->path, "/") == 0 || strcmp(r->path, "/index.html") == 0)) {
            hub_route_index(r);
        } else if(get && strcmp(r->path, "/api/info") == 0) {
            hub_route_info(r);
        } else if(get && strcmp(r->path, "/api/list") == 0) {
            hub_route_list(r);
        } else if(get && strcmp(r->path, "/api/file") == 0) {
            hub_route_file_get(r);
        } else if(put && strcmp(r->path, "/api/file") == 0) {
            hub_route_file_put(r);
        } else if(post && strcmp(r->path, "/api/mkdir") == 0) {
            hub_route_mkdir(r);
        } else if(post && strcmp(r->path, "/api/delete") == 0) {
            hub_route_delete(r);
        } else if(hub_git_route(r)) {
            /* handled in hub_git.c */
        } else if(!get && !post && !put) {
            hub_send_error(r, 405, "method not allowed");
        } else {
            hub_send_error(r, 404, "not found");
        }
    }

    hub_conn_close(t);
    prog_end(r);
    hub_cur_req = NULL;
    hub_long_op = false;
    hub_activity = HUB_ACT_IDLE;
    hub_beat_tick = furi_get_tick();
    free(r);
}
