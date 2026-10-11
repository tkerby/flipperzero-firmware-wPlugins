/* Host stand-ins for the firmware and for the parts of hub_http.c / hub_plugin.c that the
 * route code calls. Used by route_test only. Storage maps "/ext/..." to $HUB_ROOT. */
#define _GNU_SOURCE
#include <furi.h>
#include <storage/storage.h>
#include "../../flipper_app/hub_req.h"
#include "../../flipper_app/hub_git.h"
#include "../../flipper_app/hub_plugin.h"
#include "../../flipper_app/hub_gitpack.h"
#include "../../flipper_app/hub_gitgraph.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

const char* host_map(const char* p, char* buf, size_t n);

uint32_t furi_get_tick(void) { static uint32_t t; return t += 1; }
size_t memmgr_get_free_heap(void) { return 1u << 20; }
size_t memmgr_heap_get_max_free_block(void) { return 1u << 20; }

/* ---- storage ---- */
struct Storage { int x; };
struct File { FILE* f; DIR* d; char path[1024]; };
File* storage_file_alloc(Storage* s) { (void)s; return calloc(1, sizeof(File)); }
void storage_file_free(File* f) { free(f); }
bool storage_file_open(File* f, const char* path, FS_AccessMode a, FS_OpenMode m) {
    char mb[1024];
    const char* p = host_map(path, mb, sizeof mb);
    const char* mode = a == FSAM_READ ? "rb" : (m == FSOM_CREATE_ALWAYS ? "wb" : "r+b");
    f->f = fopen(p, mode);
    return f->f != NULL;
}
bool storage_file_close(File* f) { if(f->f) fclose(f->f); f->f = NULL; return true; }
size_t storage_file_read(File* f, void* b, size_t n) { return fread(b, 1, n, f->f); }
size_t storage_file_write(File* f, const void* b, size_t n) { return fwrite(b, 1, n, f->f); }
uint64_t storage_file_size(File* f) { struct stat st; fstat(fileno(f->f), &st); return (uint64_t)st.st_size; }
bool storage_dir_open(File* f, const char* path) {
    char mb[1024];
    const char* p = host_map(path, mb, sizeof mb);
    f->d = opendir(p);
    snprintf(f->path, sizeof f->path, "%s", p);
    return f->d != NULL;
}
bool storage_dir_read(File* f, FileInfo* info, char* name, uint16_t cap) {
    struct dirent* e;
    while((e = readdir(f->d))) {
        if(!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1400];
        snprintf(full, sizeof full, "%s/%s", f->path, e->d_name);
        struct stat st;
        if(stat(full, &st) != 0) continue;
        info->size = (uint64_t)st.st_size;
        info->flags = S_ISDIR(st.st_mode) ? 1 : 0;
        snprintf(name, cap, "%s", e->d_name);
        return true;
    }
    return false;
}
bool storage_dir_close(File* f) { if(f->d) closedir(f->d); f->d = NULL; return true; }
FS_Error storage_common_stat(Storage* s, const char* path, FileInfo* info) {
    (void)s;
    char mb[1024];
    struct stat st;
    if(stat(host_map(path, mb, sizeof mb), &st) != 0) return FSE_NOT_EXIST;
    info->size = (uint64_t)st.st_size;
    info->flags = S_ISDIR(st.st_mode) ? 1 : 0;
    return FSE_OK;
}
FS_Error storage_common_remove(Storage* s, const char* path) {
    (void)s;
    char mb[1024];
    return remove(host_map(path, mb, sizeof mb)) == 0 ? FSE_OK : FSE_NOT_EXIST;
}
FS_Error storage_common_rename(Storage* s, const char* a, const char* b) {
    (void)s;
    char m1[1024], m2[1024];
    return rename(host_map(a, m1, sizeof m1), host_map(b, m2, sizeof m2)) == 0 ? FSE_OK : FSE_INTERNAL;
}
bool storage_simply_mkdir(Storage* s, const char* path) {
    (void)s;
    char mb[1024];
    const char* p = host_map(path, mb, sizeof mb);
    return mkdir(p, 0755) == 0 || access(p, F_OK) == 0;
}
bool storage_simply_remove_recursive(Storage* s, const char* path) {
    (void)s;
    char mb[1024], cmd[1100];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", host_map(path, mb, sizeof mb));
    return system(cmd) == 0;
}
const char* storage_error_get_desc(FS_Error e) { (void)e; return "storage error"; }
FS_Error storage_common_fs_info(Storage* s, const char* p, uint64_t* total, uint64_t* fr) {
    (void)s; (void)p;
    *total = 1ull << 34;
    *fr = 1ull << 33;
    return FSE_OK;
}

/* ---- transport / http: everything the route writes lands in out_buf ---- */
uint8_t* out_buf;
size_t out_len, out_cap;
bool hub_conn_write(HubTransport* t, const void* d, size_t n) {
    (void)t;
    if(out_len + n > out_cap) {
        out_cap = (out_len + n) * 2 + 4096;
        out_buf = (realloc)(out_buf, out_cap); /* the capture is not part of the measured heap */
    }
    memcpy(out_buf + out_len, d, n);
    out_len += n;
    return true;
}
size_t hub_conn_read(HubTransport* t, uint8_t* b, size_t n, uint32_t to) { (void)t; (void)b; (void)n; (void)to; return 0; }
void hub_conn_close(HubTransport* t) { (void)t; }

static const char* reason(int c) { return c == 200 ? "OK" : "Other"; }
bool hub_send_head(HubReq* r, int code, const char* ctype, size_t len, const char* extra) {
    char cl[48];
    if(len == HUB_CHUNKED) strcpy(cl, "Transfer-Encoding: chunked\r\n");
    else snprintf(cl, sizeof cl, "Content-Length: %lu\r\n", (unsigned long)len);
    FuriString* h = furi_string_alloc_printf(
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\n%s%sConnection: close\r\n%s\r\n", code, reason(code), ctype, cl,
        extra && strstr(extra, "Cache-Control:") ? "" : "Cache-Control: no-store\r\n", extra ? extra : "");
    hub_conn_write(r->t, furi_string_get_cstr(h), furi_string_size(h));
    furi_string_free(h);
    r->head_sent = true;
    return true;
}
void hub_send_body(HubReq* r, int code, const char* ctype, const char* body, size_t len) {
    if(hub_send_head(r, code, ctype, len, NULL)) hub_conn_write(r->t, body, len);
}
void hub_send_json(HubReq* r, int code, FuriString* j) {
    hub_send_body(r, code, "application/json", furi_string_get_cstr(j), furi_string_size(j));
}
void hub_send_error(HubReq* r, int code, const char* msg) {
    FuriString* s = furi_string_alloc_set("{\"ok\":false,\"error\":");
    hub_json_str(s, msg);
    furi_string_push_back(s, '}');
    hub_send_json(r, code, s);
    furi_string_free(s);
}
void hub_send_ok(HubReq* r) { hub_send_body(r, 200, "application/json", "{\"ok\":true}", 11); }
void hub_serve_file_as(HubReq* r, const char* p, const char* c, bool d) { (void)p; (void)c; (void)d; hub_send_error(r, 500, "no files in the test"); }
bool hub_deadline_expired(void) { return false; }
void hub_deadline_start(uint32_t ms) { (void)ms; }
void hub_deadline_clear(void) {}
void hub_beat(void) {}
bool hub_heap_ok(HubReq* r, size_t need) { (void)r; (void)need; return true; }
bool hub_sd_ok(HubReq* r, uint64_t n) { (void)r; (void)n; return true; }
bool hub_sd_free_ok(Storage* s, uint64_t n) { (void)s; (void)n; return true; }
volatile uint8_t hub_activity;
volatile uint32_t hub_beat_tick;

static int hv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
bool hub_query_get(const char* q, const char* key, char* out, size_t cap) {
    size_t kl = strlen(key);
    while(q && *q) {
        const char* e = strchr(q, '&');
        size_t l = e ? (size_t)(e - q) : strlen(q);
        if(l > kl && !strncmp(q, key, kl) && q[kl] == '=') {
            size_t o = 0;
            for(const char* p = q + kl + 1; p < q + l && o + 1 < cap; p++) {
                if(*p == '%' && p + 2 < q + l + 1 && hv(p[1]) >= 0 && hv(p[2]) >= 0) { out[o++] = (char)(hv(p[1]) * 16 + hv(p[2])); p += 2; }
                else out[o++] = *p == '+' ? ' ' : *p;
            }
            out[o] = 0;
            return true;
        }
        q = e ? e + 1 : NULL;
    }
    return false;
}

/* ---- plugin / pack side: not exercised by the routes under test ---- */
bool hub_plugin_fits(HubPluginId id) { (void)id; return true; }
void hub_plugin_unload(void) {}
bool hub_plugin_dispatch(HubReq* r, HubPluginId id) { (void)r; (void)id; return false; }
HubPluginId hub_plugin_for(const HubReq* r) { (void)r; return HUB_PL_COUNT; }
DavLock hub_dav_locks[DAV_LOCKS];
bool hub_dav_repo_locked(const char* n) { (void)n; return false; }
bool hub_dav_route(HubReq* r) { (void)r; return false; }
bool hub_gitdoc_route(HubReq* r) { (void)r; return false; }
int hub_git_pack_loose(void* io, const char* r, unsigned m, size_t l, HubGitPackCursor* c, HubGitPackProgress p, HubGitPackStop s, void* x, HubGitPackResult* res) { (void)io; (void)r; (void)m; (void)l; (void)c; (void)p; (void)s; (void)x; memset(res, 0, sizeof *res); return 0; }
int hub_git_pack_stats(void* io, const char* r, uint32_t l, unsigned* t, unsigned* n) { (void)io; (void)r; (void)l; *t = *n = 0; return 0; }
int hub_git_pack_merge(void* io, const char* r, HubGitMergeCursor* c, uint32_t l, unsigned m, HubGitPackStop s, void* x, HubGitMergeResult* res) { (void)io; (void)r; (void)c; (void)l; (void)m; (void)s; (void)x; memset(res, 0, sizeof *res); return 0; }
int hub_git_graph_update(void* io, const char* r, HubGitPackStop s, void* x, HubGitGraphResult* res) { (void)io; (void)r; (void)s; (void)x; memset(res, 0, sizeof *res); return 0; }

/* same as in hub_plugin.c */
int hub_gitwrite_ent_cmp(const char* an, size_t al, bool a_tree, const char* bn, size_t bl, bool b_tree) {
    size_t m = al < bl ? al : bl;
    int c = memcmp(an, bn, m);
    if(c != 0) return c < 0 ? -1 : 1;
    unsigned c1 = al > m ? (unsigned char)an[m] : (a_tree ? '/' : 0);
    unsigned c2 = bl > m ? (unsigned char)bn[m] : (b_tree ? '/' : 0);
    if(c1 == c2) return 0;
    return c1 < c2 ? -1 : 1;
}
