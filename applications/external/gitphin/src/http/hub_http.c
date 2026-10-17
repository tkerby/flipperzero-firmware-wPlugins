#include "hub_http.h"
#include "hub_req.h"
#include "../git/hub_git.h"
#include "../obj/hub_gitobj.h"

#include <furi_hal.h>
#include <toolbox/version.h>
#include "hub_mem.h"

#define HUB_INDEX_PATH EXT_PATH("apps_assets/gitphin/index.html")

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_http_util.c"
#include "hub_http_send.c"
#include "hub_http_files.c"

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
