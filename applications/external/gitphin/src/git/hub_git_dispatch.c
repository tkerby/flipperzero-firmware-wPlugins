/* Git routes: temp cleanup, the dispatcher, the calls for other modules.
 * Part of hub_git.c: included there, in this order, not built alone. */
#include "hub_git_int.h"

/* ---------- temp file cleanup at start ---------- */

/* Delete "*.part" and "tmp_pack_*" files directly inside one folder. The names
 * are collected first: a folder is not changed while it is being read. */
static void clean_dir(Storage* storage, const char* path) {
    FuriString* names = furi_string_alloc();
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, path)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(file_info_is_dir(&info)) continue;
            if((len > 5 && strcmp(name + len - 5, ".part") == 0) ||
               strncmp(name, "tmp_pack_", 9) == 0) {
                furi_string_cat_printf(names, "%s\n", name);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    char full[GIT_PATH_MAX + 140];
    const char* p = furi_string_get_cstr(names);
    while(*p) {
        size_t n = strcspn(p, "\n");
        if(n > 0 && n < 128) {
            snprintf(full, sizeof(full), "%s/%.*s", path, (int)n, p);
            storage_common_remove(storage, full);
        }
        p += n + (p[n] ? 1 : 0);
    }
    furi_string_free(names);
}

void hub_git_cleanup_temp(Storage* storage) {
    static const char* const sub[] = {
        "", "/info", "/objects/info", "/objects/pack", "/refs/heads", "/refs/tags", "/hub"};
    File* dir = storage_file_alloc(storage);
    FuriString* repos = furi_string_alloc();
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(file_info_is_dir(&info) && len >= 5 && strcmp(name + len - 4, ".git") == 0) {
                furi_string_cat_printf(repos, "%s\n", name);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    char path[GIT_PATH_MAX];
    const char* p = furi_string_get_cstr(repos);
    while(*p) {
        size_t n = strcspn(p, "\n");
        if(n > 0 && n < 100) {
            hub_beat();
            snprintf(path, sizeof(path), GIT_ROOT "/%.*s/hub/tmp", (int)n, p);
            storage_simply_remove_recursive(storage, path);
            for(size_t i = 0; i < COUNT_OF(sub); i++) {
                snprintf(path, sizeof(path), GIT_ROOT "/%.*s%s", (int)n, p, sub[i]);
                clean_dir(storage, path);
            }
        }
        p += n + (p[n] ? 1 : 0);
    }
    furi_string_free(repos);
}

/* ---------- dispatcher ---------- */

bool hub_git_route(HubReq* r) {
    bool file = strncmp(r->path, "/git/", 5) == 0;
    bool api = strncmp(r->path, "/api/git/", 9) == 0;
    if(!file && !api) return false;

    bool get = strcmp(r->method, "GET") == 0;
    bool post = strcmp(r->method, "POST") == 0;
    if(file && !get) {
        static const char* const dav_methods[] = {
            "OPTIONS", "HEAD", "PROPFIND", "MKCOL", "PUT", "MOVE", "DELETE", "LOCK", "UNLOCK"};
        for(size_t i = 0; i < sizeof(dav_methods) / sizeof(dav_methods[0]); i++) {
            if(strcmp(r->method, dav_methods[i]) == 0) return hub_plugin_dispatch(r, HUB_PL_DAV);
        }
        return false;
    }

    if(api) {
        HubPluginId pl = hub_plugin_for(r);
        if(pl != HUB_PL_COUNT) return hub_plugin_dispatch(r, pl);
        if(hub_gitview_route(r)) return true;
    }

    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(g, 0, sizeof(GitCtx));
    g->r = r;

    if(file) {
        git_route_file(r, g);
    } else {
        const char* name = r->path + 9;
        hub_git_mkroot(r->storage);
        if(strcmp(name, "repos") == 0 && get) {
            git_route_repos(g);
        } else if(strcmp(name, "refs") == 0 && get) {
            git_route_refs(g);
        } else if(post && strcmp(name, "create") == 0) {
            git_route_create(g);
        } else if(post && strcmp(name, "delete") == 0) {
            git_route_delete(g);
        } else if(post && strcmp(name, "rename") == 0) {
            git_route_rename(g);
        } else if(post && strcmp(name, "describe") == 0) {
            git_route_describe(g);
        } else if(post && strcmp(name, "ref") == 0) {
            git_route_ref(g);
        } else if(post && strcmp(name, "head") == 0) {
            git_route_head(g);
        } else if(post && strcmp(name, "pack") == 0) {
            git_route_pack(g);
        } else if(post && strcmp(name, "server-info") == 0) {
            git_route_server_info(g);
        } else if(
            strcmp(name, "repos") == 0 || strcmp(name, "refs") == 0 ||
            strcmp(name, "create") == 0 || strcmp(name, "delete") == 0 ||
            strcmp(name, "rename") == 0 || strcmp(name, "describe") == 0 ||
            strcmp(name, "ref") == 0 || strcmp(name, "head") == 0 ||
            strcmp(name, "server-info") == 0 || strcmp(name, "pack") == 0) {
            hub_send_error(r, 405, "method not allowed");
        } else {
            hub_send_error(r, 404, "not found");
        }
    }
    free(g);
    return true;
}

/* ---------- for hub_dav.c ---------- */

void hub_git_mark_clean(const char* name) {
    if(strcmp(git_dirty, name) == 0) git_dirty[0] = '\0';
}

void hub_git_mark_dirty(HubReq* r, const char* name) {
    if(git_dirty[0] && strcmp(git_dirty, name) != 0) {
        char old[GIT_NAME_MAX + 1];
        strcpy(old, git_dirty);
        git_dirty[0] = '\0';
        hub_git_changed(r, old);
    }
    snprintf(git_dirty, sizeof(git_dirty), "%s", name);
}

bool hub_git_name_ok(const char* name) {
    return git_name_ok(name);
}

/* Regenerate info/refs and objects/info/packs of a repo. */
bool hub_git_changed(HubReq* r, const char* name) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return false;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    bool ok = git_write_info_refs(g) && git_write_info_packs(g);
    free(g);
    return ok;
}

/* A ref was deleted by file: take it out of packed-refs too. */
bool hub_git_drop_packed(HubReq* r, const char* name, const char* ref) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return false;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    bool ok = git_packed_drop(g, ref);
    free(g);
    return ok;
}

void hub_git_flush_r(HubReq* r) {
    if(!git_dirty[0]) return;
    char name[GIT_NAME_MAX + 1];
    strcpy(name, git_dirty);
    git_dirty[0] = '\0';
    hub_git_changed(r, name);
}

/* For the worker shutdown: there is no request, so make a bare one. */
void hub_git_flush(Storage* storage) {
    if(!git_dirty[0]) return;
    HubReq* r = malloc(sizeof(HubReq));
    if(!r) return; /* the next start regenerates it */
    memset(r, 0, sizeof(HubReq));
    r->storage = storage;
    hub_git_flush_r(r);
    free(r);
}

/* Gitphin 1.0 kept the repos in /ext/git. Move that folder into the app's data folder
 * once, at start, while the new folder does not exist yet. */
void hub_git_migrate(Storage* storage) {
    if(storage_dir_exists(storage, GIT_ROOT) || !storage_dir_exists(storage, EXT_PATH("git")))
        return;
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/gitphin"));
    storage_common_rename(storage, EXT_PATH("git"), GIT_ROOT);
}
