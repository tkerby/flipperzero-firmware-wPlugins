/* WebDAV: MKCOL, PUT, DELETE, MOVE.
 * Part of hub_dav.c: included there, in this order, not built alone. */
#include "hub_dav_int.h"

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
