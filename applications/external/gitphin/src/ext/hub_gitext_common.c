/* Plugin git routes: JSON text and the helpers both plugins use.
 * Part of hub_gitext.c: included there, in this order, not built alone. */
#include "hub_gitext_int.h"

/* ---------- JSON text ---------- */

#define ext_jbytes hub_json_bytes

static __attribute__((unused)) void ext_jstr(FuriString* s, const char* v) {
    ext_jbytes(s, v, strlen(v));
}

/* ---------- common ---------- */

static __attribute__((unused)) void ext_err(Ext* e, int rc) {
    HubReq* r = e->r;
    switch(rc) {
    case HUB_GIT_ERR_NOTFOUND:
        hub_send_error(r, 404, "not found");
        break;
    case HUB_GIT_ERR_TOOMANY:
        hub_send_error(r, 503, "repository is being reorganised, try again in a few minutes");
        break;
    case HUB_GIT_ERR_TOOLARGE:
        hub_send_error(r, 413, "object too large");
        break;
    case HUB_GIT_ERR_BADARG:
    case HUB_GC_ERR_BADPATH:
        hub_send_error(r, 400, "bad path or argument");
        break;
    case HUB_GC_ERR_CONFLICT:
        hub_send_error(r, 409, "path conflicts with an existing file or folder");
        break;
    case HUB_GC_ERR_UNCHANGED:
        hub_send_error(r, 409, "nothing changed");
        break;
    case HUB_GIT_ERR_NOMEM:
        hub_send_error(r, 503, "low memory, try again");
        break;
    case HUB_GIT_ERR_CORRUPT:
        hub_send_error(r, 500, "corrupt repository");
        break;
    default:
        hub_send_error(r, 500, "storage error");
        break;
    }
}

static __attribute__((unused)) bool ext_sha_ok(const char* s) {
    for(size_t i = 0; i < 40; i++) {
        char c = s[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return s[40] == '\0';
}

/* Full ref under refs/heads/ or refs/tags/, with the git rules (same as hub_git.c). */
static __attribute__((unused)) bool ext_ref_ok(const char* ref) {
    size_t len = strlen(ref);
    size_t pre;
    if(strncmp(ref, "refs/heads/", 11) == 0) {
        pre = 11;
    } else if(strncmp(ref, "refs/tags/", 10) == 0) {
        pre = 10;
    } else {
        return false;
    }
    if(len == pre || len > EXT_REF_MAX) return false;
    if(ref[len - 1] == '/' || ref[len - 1] == '.') return false;
    if(ref[pre] == '.') return false;
    for(size_t i = 0; i < len; i++) {
        char c = ref[i];
        if((unsigned char)c <= 0x20 || c == 0x7f || strchr("~^:?*[\\", c)) return false;
        char n = ref[i + 1];
        if(c == '.' && n == '.') return false;
        if(c == '/' && (n == '/' || n == '.')) return false;
        if(c == '@' && n == '{') return false;
        if((c == '/' || n == '\0') && i >= 4) {
            const char* end = (n == '\0') ? ref + i + 1 : ref + i;
            if(strncmp(end - 5, ".lock", 5) == 0) return false;
        }
    }
    return true;
}

/* Query value: 1 = found, 0 = missing, -1 = too long (error sent). */
static __attribute__((unused)) int ext_q(Ext* e, const char* key, char* out, size_t size) {
    if(!hub_query_get(e->r->query, key, out, size)) {
        out[0] = '\0';
        return 0;
    }
    if(strlen(out) + 1 >= size) {
        hub_send_error(e->r, 400, "parameter too long");
        return -1;
    }
    return 1;
}

/* Required, non-empty parameter. Sends the error itself. */
static __attribute__((unused)) bool ext_need(Ext* e, const char* key, char* out, size_t size) {
    int q = ext_q(e, key, out, size);
    if(q < 0) return false;
    if(q == 0 || out[0] == '\0') {
        char m[40];
        snprintf(m, sizeof(m), "missing %s", key);
        hub_send_error(e->r, 400, m);
        return false;
    }
    return true;
}

/* Check "name", open the repository. Sends the error itself. */
static __attribute__((unused)) bool ext_open(Ext* e, size_t limit, size_t need) {
    if(!hub_heap_ok(e->r, need)) return false;
    if(!ext_need(e, "name", e->name, sizeof(e->name))) return false;
    if(!hub_git_name_ok(e->name)) {
        hub_send_error(e->r, 400, "bad repo name");
        return false;
    }
    snprintf(e->base, sizeof(e->base), EXT_ROOT "/%s.git", e->name);
    FileInfo info;
    if(storage_common_stat(e->r->storage, e->base, &info) != FSE_OK || !file_info_is_dir(&info)) {
        hub_send_error(e->r, 404, "repository not found");
        return false;
    }
    e->g = hub_git_open(e->r->storage, e->base);
    if(!e->g) {
        hub_send_error(e->r, 500, "cannot open repository");
        return false;
    }
    hub_git_set_limit(e->g, limit);
    return true;
}

static __attribute__((unused)) bool ext_resolve(Ext* e, const char* ref, HubOid* oid) {
    int rc = hub_git_resolve(e->g, ref, oid);
    if(rc == HUB_GIT_OK) return true;
    if(rc == HUB_GIT_ERR_NOTFOUND) {
        hub_send_error(e->r, 404, "unknown ref or commit");
    } else {
        ext_err(e, rc);
    }
    return false;
}

/* The commit of a ref ("refs/heads/x"), or ZERO-like not found. */
static __attribute__((unused)) int ext_ref_value(Ext* e, const char* ref, HubOid* oid) {
    int rc = hub_git_ref_read(e->g, ref, oid);
    return rc;
}

/* Write the loose ref file via .part. */
static __attribute__((unused)) bool ext_ref_write(Ext* e, const char* ref, const char* hex) {
    Storage* st = e->r->storage;
    snprintf(e->path, sizeof(e->path), "%s/%s", e->base, ref);
    snprintf(e->tmp, sizeof(e->tmp), "%s", e->path);
    for(char* p = e->tmp + strlen(e->base) + 1; *p; p++) {
        if(*p != '/') continue;
        *p = '\0';
        bool ok = storage_simply_mkdir(st, e->tmp);
        *p = '/';
        if(!ok) return false;
    }
    snprintf(e->tmp, sizeof(e->tmp), "%s.part", e->path);
    File* f = storage_file_alloc(st);
    bool ok = storage_file_open(f, e->tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        ok = storage_file_write(f, hex, 40) == 40 && storage_file_write(f, "\n", 1) == 1;
    }
    storage_file_close(f);
    storage_file_free(f);
    if(ok) {
        storage_common_remove(st, e->path);
        ok = storage_common_rename(st, e->tmp, e->path) == FSE_OK;
    }
    if(!ok) storage_common_remove(st, e->tmp);
    return ok;
}

/* Compare-and-swap of a ref: expected is a 40 hex sha or ZERO (must not exist). */
static __attribute__((unused)) bool
    ext_ref_cas(Ext* e, const char* ref, const char* expected, const char* value) {
    HubOid cur;
    char hex[41];
    int rc = ext_ref_value(e, ref, &cur);
    if(rc == HUB_GIT_OK) {
        hub_oid_to_hex(&cur, hex);
    } else if(rc == HUB_GIT_ERR_NOTFOUND) {
        strcpy(hex, EXT_ZERO);
    } else {
        ext_err(e, rc);
        return false;
    }
    if(strcmp(hex, expected) != 0) {
        hub_send_error(e->r, 409, "branch changed");
        return false;
    }
    if(!ext_ref_write(e, ref, value)) {
        hub_send_error(e->r, 409, "ref conflicts with another ref");
        return false;
    }
    return true;
}
