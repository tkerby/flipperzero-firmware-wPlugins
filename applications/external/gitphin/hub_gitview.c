#include "hub_git.h"
#include "hub_gitobj.h"
#include "hub_plugin.h"
#include "hub_mem.h"

/* Phase 2: browse a repository. Routes tree, blob, log and commit.
 * The object reading is in hub_gitobj.c; here is only HTTP and JSON.
 * Objects held in RAM are limited to VIEW_MAX_OBJ; blobs are streamed. */

#define VIEW_MAX_OBJ (16 * 1024)

typedef struct {
    HubReq* r;
    HubGit* g;
    char name[80];
    char base[112];
    char val[210];
    char path[170];
} View;

/* ---------- JSON text ---------- */

#define view_jbytes hub_json_bytes

/* ---------- common ---------- */

static void view_err(View* v, int rc) {
    switch(rc) {
    case HUB_GIT_ERR_NOTFOUND:
        hub_send_error(v->r, 404, "not found");
        break;
    case HUB_GIT_ERR_TOOMANY:
        hub_send_error(v->r, 503, "repository is being reorganised, try again in a few minutes");
        break;
    case HUB_GIT_ERR_TOOLARGE:
        hub_send_error(v->r, 413, "object too large");
        break;
    case HUB_GIT_ERR_BADARG:
        hub_send_error(v->r, 400, "bad argument");
        break;
    case HUB_GIT_ERR_NOMEM:
        hub_send_error(v->r, 503, "low memory, try again");
        break;
    case HUB_GIT_ERR_CORRUPT:
        hub_send_error(v->r, 500, "corrupt object");
        break;
    default:
        hub_send_error(v->r, 500, "read error");
        break;
    }
}

/* Open the repo named in the query. Sends the error itself. */
static bool view_open(View* v, size_t need) {
    if(!hub_heap_ok(v->r, need)) return false;
    if(!hub_query_get(v->r->query, "name", v->name, sizeof(v->name))) {
        hub_send_error(v->r, 400, "missing name");
        return false;
    }
    if(!hub_git_name_ok(v->name)) {
        hub_send_error(v->r, 400, "bad repo name");
        return false;
    }
    snprintf(v->base, sizeof(v->base), HUB_GIT_ROOT "/%s.git", v->name);
    v->g = hub_git_open(v->r->storage, v->base);
    if(!v->g) {
        hub_send_error(v->r, 404, "repository not found");
        return false;
    }
    hub_git_set_limit(v->g, VIEW_MAX_OBJ);
    return true;
}

/* Resolve "key" (ref or sha) to a commit. Without it: HEAD. */
static int view_resolve(View* v, const char* key1, const char* key2, HubOid* oid) {
    if(!hub_query_get(v->r->query, key1, v->val, sizeof(v->val)) &&
       !(key2 && hub_query_get(v->r->query, key2, v->val, sizeof(v->val)))) {
        strcpy(v->val, "HEAD");
    }
    return hub_git_resolve(v->g, v->val, oid);
}

/* ---------- tree ---------- */

/* Folders first, then the rest, each in git order: two walks, nothing is kept. */
typedef struct {
    HubChunk* ck;
    FuriString* t;
    HubReq* r;
    const char* commit_hex;
    const char* cache_key;
    const char* path;
    bool want_tree, first, begun;
    bool good;
} TreeOut;

static void tree_head(TreeOut* o) {
    o->good = hub_chunk_begin(o->ck, o->r, "application/json", hub_cache_hdr(o->cache_key));
    if(!o->good) return;
    furi_string_printf(o->t, "{\"ok\":true,\"commit\":\"%s\",\"path\":", o->commit_hex);
    hub_chunk_fs(o->ck, o->t);
    hub_chunk_json(o->ck, o->path, strlen(o->path));
    hub_chunk_lit(o->ck, ",\"entries\":[");
    o->begun = true;
}

static bool tree_cb(void* ctx, uint32_t mode, const char* name, size_t nl, const HubOid* oid) {
    TreeOut* o = ctx;
    const char* type = hub_tree_mode_type(mode);
    if((type[0] == 't') != o->want_tree) return true;
    if(!o->begun) tree_head(o);
    if(!o->good) return false;
    char hex[41];
    hub_oid_to_hex(oid, hex);
    if(!o->first) hub_chunk_lit(o->ck, ",");
    hub_chunk_lit(o->ck, "{\"name\":");
    o->first = false;
    hub_chunk_json(o->ck, name, nl);
    furi_string_printf(
        o->t,
        ",\"type\":\"%s\",\"mode\":\"%06lo\",\"sha\":\"%s\"}",
        type,
        (unsigned long)mode,
        hex);
    hub_chunk_fs(o->ck, o->t);
    hub_beat();
    return o->ck->ok;
}

static void view_tree(View* v) {
    HubReq* r = v->r;
    HubOid commit, tree, oid;
    uint32_t mode;
    int rc = view_resolve(v, "ref", "sha", &commit);
    char ref[48];
    snprintf(ref, sizeof(ref), "%.47s", v->val); /* v->val is reused below */
    if(rc == HUB_GIT_OK) rc = hub_git_tree_of(v->g, &commit, &tree);
    if(!hub_query_get(r->query, "path", v->path, sizeof(v->path))) v->path[0] = '\0';
    size_t pl = strlen(v->path);
    while(pl > 0 && v->path[pl - 1] == '/')
        v->path[--pl] = '\0';
    if(rc == HUB_GIT_OK) rc = hub_git_tree_find(v->g, &tree, v->path, &oid, &mode);
    if(rc != HUB_GIT_OK) return view_err(v, rc);
    if(hub_tree_mode_type(mode)[0] != 't') {
        hub_send_error(r, 400, "not a folder");
        return;
    }

    char hex[41];
    hub_oid_to_hex(&commit, hex);
    HubChunk* ck = malloc(sizeof(HubChunk));
    TreeOut* o = calloc(1, sizeof(TreeOut));
    FuriString* t = furi_string_alloc();
    if(!ck || !o) {
        hub_send_error(r, 503, "low memory, try again");
    } else {
        o->r = r;
        o->ck = ck;
        o->t = t;
        o->commit_hex = hex;
        o->cache_key = ref;
        o->path = v->path;
        o->first = true;
        o->good = true;
        o->want_tree = true;
        rc = hub_git_tree_walk(v->g, &oid, tree_cb, o);
        if(rc == HUB_GIT_OK) {
            o->want_tree = false;
            rc = hub_git_tree_walk(v->g, &oid, tree_cb, o);
        }
        if(!o->begun && rc == HUB_GIT_OK) tree_head(o); /* empty folder */
        if(!o->begun) {
            view_err(v, rc);
        } else {
            if(rc == HUB_GIT_OK) hub_chunk_lit(ck, "]}");
            hub_chunk_end(ck, rc == HUB_GIT_OK && ck->ok);
        }
    }
    furi_string_free(t);
    free(o);
    free(ck);
}

/* ---------- blob ---------- */

static const char* blob_mime(const char* path) {
    const char* dot = strrchr(path, '.');
    if(!dot) return "application/octet-stream";
    static const char* const text[] = {".txt",  ".md",   ".c",   ".h",    ".cpp", ".hpp",  ".py",
                                       ".js",   ".json", ".css", ".html", ".htm", ".xml",  ".yml",
                                       ".yaml", ".toml", ".sh",  ".rs",   ".go",  ".java", ".csv",
                                       ".ini",  ".cfg",  ".log", ".sub",  ".ir",  ".nfc",  ".rfid",
                                       ".fam",  ".ts",   ".lua", ".rb",   ".php", ".sql"};
    for(size_t i = 0; i < COUNT_OF(text); i++) {
        if(strcasecmp(dot, text[i]) == 0) return "text/plain; charset=utf-8";
    }
    if(strcasecmp(dot, ".png") == 0) return "image/png";
    if(strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if(strcasecmp(dot, ".gif") == 0) return "image/gif";
    if(strcasecmp(dot, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

typedef struct {
    View* v;
    const char* mime;
    const char* extra; /* headers for the answer */
    uint8_t type;
    size_t size;
    bool head_sent;
    bool bad_type;
} BlobCtx;

static bool blob_sink(void* ctx, const uint8_t* data, size_t len) {
    BlobCtx* b = ctx;
    hub_beat();
    if(!b->head_sent) {
        if(b->type != HUB_OBJ_BLOB) {
            b->bad_type = true;
            return false;
        }
        b->head_sent = true;
        if(!hub_send_head(b->v->r, 200, b->mime, b->size, b->extra)) return false;
    }
    return hub_conn_write(b->v->r->t, data, len);
}

static void view_blob(View* v) {
    HubReq* r = v->r;
    HubOid oid;
    int rc = HUB_GIT_OK;
    const char* name_for_mime = "";
    char sha[48];
    const char* ckey = sha; /* what addresses the answer: a full id never changes */

    if(hub_query_get(r->query, "sha", sha, sizeof(sha)) &&
       !hub_query_get(r->query, "path", v->path, sizeof(v->path))) {
        if(!hub_oid_from_hex(sha, &oid)) {
            hub_send_error(r, 400, "bad sha");
            return;
        }
    } else {
        HubOid commit, tree;
        uint32_t mode;
        if(!hub_query_get(r->query, "path", v->path, sizeof(v->path)) || v->path[0] == '\0') {
            hub_send_error(r, 400, "missing path or sha");
            return;
        }
        rc = view_resolve(v, "ref", NULL, &commit);
        ckey = v->val;
        if(rc == HUB_GIT_OK) rc = hub_git_tree_of(v->g, &commit, &tree);
        if(rc == HUB_GIT_OK) rc = hub_git_tree_find(v->g, &tree, v->path, &oid, &mode);
        if(rc != HUB_GIT_OK) return view_err(v, rc);
        if(hub_tree_mode_type(mode)[0] == 't') {
            hub_send_error(r, 400, "path is a folder");
            return;
        }
        if(hub_tree_mode_type(mode)[0] == 'c') {
            hub_send_error(r, 400, "path is a submodule");
            return;
        }
        name_for_mime = v->path;
    }

    char hex[41];
    hub_oid_to_hex(&oid, hex);
    FuriString* extra = furi_string_alloc_printf(
        "X-Git-Sha: %s\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: sandbox\r\n%s",
        hex,
        hub_cache_hdr(ckey));
    char dl[4] = {0};
    if(hub_query_get(r->query, "dl", dl, sizeof(dl)) && dl[0] == '1') {
        const char* base = strrchr(name_for_mime, '/');
        base = base ? base + 1 : (name_for_mime[0] ? name_for_mime : hex);
        furi_string_cat_str(extra, "Content-Disposition: attachment; filename=\"");
        for(const char* p = base; *p; p++) {
            char c = *p;
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '.' || c == '_' || c == '-';
            furi_string_push_back(extra, ok ? c : '_');
        }
        furi_string_cat_str(extra, "\"\r\n");
    }

    BlobCtx b = {
        .v = v,
        .mime = blob_mime(name_for_mime),
        .extra = furi_string_get_cstr(extra),
        .type = 0,
        .size = 0,
        .head_sent = false,
        .bad_type = false};
    rc = hub_git_stream(v->g, &oid, blob_sink, &b, &b.type, &b.size);
    if(!b.head_sent) {
        if(b.bad_type) {
            hub_send_error(r, 400, "not a file");
        } else if(rc == HUB_GIT_OK) {
            if(b.type != HUB_OBJ_BLOB) {
                hub_send_error(r, 400, "not a file");
            } else {
                hub_send_head(r, 200, b.mime, 0, b.extra); /* empty file */
            }
        } else {
            view_err(v, rc);
        }
    }
    furi_string_free(extra);
}

/* ---------- dispatcher ---------- */

bool hub_gitview_route(HubReq* r) {
    if(strncmp(r->path, "/api/git/", 9) != 0 || strcmp(r->method, "GET") != 0) return false;
    const char* route = r->path + 9;
    void (*fn)(View*) = NULL;
    size_t need = 0; /* measured peak + 6 KB */
    if(strcmp(route, "tree") == 0) {
        fn = view_tree;
        need = 28 * 1024; /* est.: 16 KB object + 4.5 KB inflate + 6 KB list/out */
    } else if(strcmp(route, "blob") == 0) {
        fn = view_blob;
        need =
            24 *
            1024; /* host peak 16.6 KB (8 KB ring window, cache file for far matches) +25 % +firmware objects */
    } else {
        return false;
    }
    if(memmgr_get_free_heap() < need + HUB_HEAP_RESERVE)
        hub_plugin_unload(); /* give the plugin's memory back */
    View* v = malloc(sizeof(View));
    if(!v) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(v, 0, sizeof(View));
    v->r = r;
    if(view_open(v, need)) {
        fn(v);
        hub_git_close(v->g);
    }
    free(v);
    return true;
}
