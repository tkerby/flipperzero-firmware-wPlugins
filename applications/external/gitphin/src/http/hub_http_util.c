/* HTTP server: small helpers, heartbeat and deadline, SD and heap checks, MIME types.
 * Part of hub_http.c: included there, in this order, not built alone. */

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
