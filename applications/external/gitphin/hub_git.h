#pragma once

#include "hub_req.h"

/* Repos live in the app's own data folder: HUB_GIT_ROOT "/<name>.git". */
#define HUB_GIT_ROOT "/ext/apps_data/gitphin/git"

/* Make HUB_GIT_ROOT and its parents (storage_simply_mkdir is not recursive). */
static inline bool hub_git_mkroot(Storage* storage) {
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/gitphin"));
    return storage_simply_mkdir(storage, HUB_GIT_ROOT);
}

/* Git hosting routes: /git/<name>.git/... and /api/git/...
 * Returns true when the request was handled (the answer is sent). */
bool hub_git_route(HubReq* r);

/* Used by hub_dav.c (WebDAV for git http-push). */
bool hub_dav_route(HubReq* r);
bool hub_git_name_ok(const char* name);
bool hub_git_changed(HubReq* r, const char* name);
bool hub_git_drop_packed(HubReq* r, const char* name, const char* ref);
void hub_git_mark_dirty(HubReq* r, const char* name);
void hub_git_mark_clean(const char* name);
/* Rebuild a pending (stale) info/refs and objects/info/packs now. */
void hub_git_flush(Storage* storage);
void hub_git_flush_r(HubReq* r);
bool hub_gitview_route(HubReq* r);

/* Loose object packing (hub_gitpack.c). Repos with loose objects are marked;
 * the worker packs one batch of a marked repo when the app is idle. */
void hub_git_mark_loose(const char* name);
void hub_git_mark_base(const char* base); /* same, from HUB_GIT_ROOT "/<name>.git" */
void hub_git_touch(const char* name); /* a write happened: delay the packer */
void hub_git_mark_all(Storage* storage);
void hub_git_idle_tick(Storage* storage);
void hub_git_free_state(void); /* at app exit: free the idle packer state */
bool hub_dav_repo_locked(const char* name);
void hub_git_cleanup_temp(Storage* storage); /* *.part, tmp_pack_*, hub/tmp */
void hub_git_migrate(Storage* storage); /* move the repos of Gitphin 1.0 (/ext/git) here */
