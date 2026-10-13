/* HTTP server: transfer progress, the request body, responses.
 * Part of hub_http.c: included there, in this order, not built alone. */

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
