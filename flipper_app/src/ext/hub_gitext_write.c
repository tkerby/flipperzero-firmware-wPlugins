/* Write plugin: commit-file and merge.
 * Part of hub_gitext.c: included there, in this order, not built alone. */
#include "hub_gitext_int.h"

/* ---------- commit-file ---------- */

/* Read the whole body into a malloc'd buffer (size + 1). NULL on error (sent). */
static uint8_t* ext_body(Ext* e, size_t* len) {
    HubReq* r = e->r;
    if(r->content_length > EXT_BODY_MAX) {
        hub_send_error(r, 413, "file too large (max 64 KB)");
        return NULL;
    }
    uint8_t* buf = malloc(r->content_length + 1);
    if(!buf) {
        hub_send_error(r, 503, "low memory, try again");
        return NULL;
    }
    size_t got = 0;
    while(got < r->content_length) {
        size_t n = hub_body_read(r, buf + got, r->content_length - got);
        if(n == 0) break;
        got += n;
    }
    if(got != r->content_length) {
        free(buf);
        hub_send_error(r, 400, "upload was cut off");
        return NULL;
    }
    buf[got] = '\0';
    *len = got;
    return buf;
}

/* author / email / message from the query, with defaults */
static bool ext_ident_params(Ext* e, const char* default_msg) {
    int q = ext_q(e, "author", e->author, sizeof(e->author));
    if(q < 0) return false;
    if(q == 0 || e->author[0] == '\0') strcpy(e->author, "me");
    q = ext_q(e, "email", e->email, sizeof(e->email));
    if(q < 0) return false;
    if(q == 0 || e->email[0] == '\0') strcpy(e->email, "me@flipper.local");
    q = ext_q(e, "message", e->msg, sizeof(e->msg));
    if(q < 0) return false;
    if(q == 0 || e->msg[0] == '\0') snprintf(e->msg, sizeof(e->msg), "%s", default_msg);
    return true;
}

static void route_commit_file(Ext* e) {
    HubReq* r = e->r;
    if(!ext_open(e, EXT_CF_OBJ, 42 * 1024)) return;
    if(!hub_sd_ok(r, (uint64_t)r->content_length + 256 * 1024)) return;
    /* p1 = branch, p2 = old, p3 = path; the new branch name goes to e->tmp (not used by the ref writer until later) */
    if(!ext_need(e, "branch", e->p1, sizeof(e->p1)) || !ext_need(e, "old", e->p2, sizeof(e->p2)) ||
       !ext_need(e, "path", e->p3, sizeof(e->p3)))
        return;
    int q = ext_q(e, "action", e->action, sizeof(e->action));
    if(q < 0) return;
    if(q == 0 || e->action[0] == '\0') strcpy(e->action, "put");
    bool del = strcmp(e->action, "delete") == 0;
    if(!del && strcmp(e->action, "put") != 0)
        return hub_send_error(r, 400, "action must be put or delete");
    q = ext_q(e, "new_branch", e->nbranch, sizeof(e->nbranch));
    if(q < 0) return;
    bool create = q == 1 && e->nbranch[0];
    if(!ext_sha_ok(e->p2)) return hub_send_error(r, 400, "bad sha in old");
    snprintf(e->dm, sizeof(e->dm), "%s %.200s", del ? "Delete" : "Update", e->p3);
    if(!ext_ident_params(e, e->dm)) return;

    char* ref = e->ref;
    char* nref = e->nref;
    snprintf(ref, sizeof(e->ref), "refs/heads/%s", e->p1);
    if(!ext_ref_ok(ref)) return hub_send_error(r, 400, "bad branch name");
    if(create) {
        snprintf(nref, sizeof(e->nref), "refs/heads/%s", e->nbranch);
        if(!ext_ref_ok(nref)) return hub_send_error(r, 400, "bad new branch name");
    }

    size_t blen = 0;
    uint8_t* body = ext_body(e, &blen);
    if(!body) return;

    /* the branch must still be at "old" (also the base of a new branch: the page
     * edited that version) */
    bool root = strcmp(e->p2, EXT_ZERO) == 0;
    HubOid cur, tip;
    char* hex = e->hexa;
    int rc = ext_ref_value(e, ref, &cur);
    if(rc == HUB_GIT_OK) {
        hub_oid_to_hex(&cur, hex);
    } else if(rc == HUB_GIT_ERR_NOTFOUND) {
        strcpy(hex, EXT_ZERO);
    } else {
        free(body);
        return ext_err(e, rc);
    }
    if(!create && strcmp(hex, e->p2) != 0) {
        free(body);
        return hub_send_error(r, 409, "branch changed");
    }
    if(!root && !hub_oid_from_hex(e->p2, &tip)) {
        free(body);
        return hub_send_error(r, 400, "bad sha in old");
    }

    HubGitWriteIo io;
    hub_gitwrite_io_furi(&io, r->storage);
    HubGwIdent who = {e->author, e->email, (int64_t)furi_hal_rtc_get_timestamp(), "+0000"};
    HubOid commit;
    rc = hub_git_commit_file(
        e->g,
        &io,
        e->base,
        root ? NULL : &tip,
        e->p3,
        del ? NULL : body,
        blen,
        e->msg,
        &who,
        &commit,
        NULL);
    free(body);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);

    char* chex = e->hexb;
    hub_oid_to_hex(&commit, chex);
    if(!ext_ref_cas(e, create ? nref : ref, create ? EXT_ZERO : e->p2, chex)) return;
    hub_git_changed(r, e->name);

    FuriString* s = furi_string_alloc_printf("{\"ok\":true,\"commit\":\"%s\",\"branch\":", chex);
    ext_jstr(s, create ? e->nbranch : e->p1);
    furi_string_push_back(s, '}');
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

/* ---------- merge ---------- */

static bool merge_path(void* ctx, const char* path) {
    FuriString* s = ctx;
    if(furi_string_size(s) > EXT_JSON_CAP) return false;
    if(furi_string_get_char(s, furi_string_size(s) - 1) != '[') furi_string_push_back(s, ',');
    ext_jstr(s, path);
    return true;
}

static void route_merge(Ext* e) {
    HubReq* r = e->r;
    if(!ext_open(e, EXT_MAX_OBJ, 35 * 1024)) return;
    if(!ext_need(e, "base", e->p1, sizeof(e->p1)) || !ext_need(e, "head", e->p2, sizeof(e->p2)))
        return;
    char* ref = e->ref;
    if(strncmp(e->p1, "refs/heads/", 11) == 0) {
        snprintf(ref, sizeof(e->ref), "%s", e->p1);
    } else {
        snprintf(ref, sizeof(e->ref), "refs/heads/%s", e->p1);
    }
    if(!ext_ref_ok(ref)) return hub_send_error(r, 400, "bad base branch");
    snprintf(e->dm, sizeof(e->dm), "Merge %.100s into %.100s", e->p2, e->p1);
    if(!ext_ident_params(e, e->dm)) return;
    HubOid base, head;
    int rc = ext_ref_value(e, ref, &base);
    if(rc == HUB_GIT_ERR_NOTFOUND) return hub_send_error(r, 404, "base branch not found");
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    rc = hub_git_peel(e->g, &base);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    if(!ext_resolve(e, e->p2, &head)) return;

    HubGitCompare* c = calloc(1, sizeof(HubGitCompare));
    if(!c) return hub_send_error(r, 503, "low memory, try again");
    c->cap = EXT_COMPARE_CAP;
    rc = hub_git_compare(e->g, &base, &head, c);
    bool capped = c->capped, ff = c->ff, has_mb = c->has_merge_base;
    uint32_t ahead = c->ahead_count;
    HubOid mb = c->merge_base;
    free(c);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    if(capped) return hub_send_error(r, 409, "history too long to merge on the device");
    if(ahead == 0) return hub_send_error(r, 409, "already up to date");

    char* bhex = e->hexa;
    char* chex = e->hexb;
    hub_oid_to_hex(&base, bhex);
    HubOid result;
    bool fast = ff;
    if(ff) {
        result = head; /* fast-forward: no merge commit */
    } else {
        FuriString* paths = furi_string_alloc_set("[");
        HubGitWriteIo io;
        hub_gitwrite_io_furi(&io, r->storage);
        HubGwIdent who = {e->author, e->email, (int64_t)furi_hal_rtc_get_timestamp(), "+0000"};
        unsigned nconf = 0;
        hub_git_set_limit(e->g, EXT_MERGE_OBJ);
        rc = hub_git_merge_commits(
            e->g,
            &io,
            e->base,
            &base,
            &head,
            has_mb ? &mb : NULL,
            e->msg,
            &who,
            &result,
            NULL,
            merge_path,
            paths,
            &nconf);
        hub_git_set_limit(e->g, EXT_MAX_OBJ);
        if(rc == HUB_GC_ERR_MERGE) {
            furi_string_cat_str(paths, "]");
            FuriString* s =
                furi_string_alloc_set("{\"ok\":false,\"error\":\"conflict\",\"paths\":");
            furi_string_cat(s, paths);
            furi_string_push_back(s, '}');
            hub_send_json(r, 409, s);
            furi_string_free(s);
            furi_string_free(paths);
            return;
        }
        furi_string_free(paths);
        if(rc != HUB_GIT_OK) return ext_err(e, rc);
    }
    hub_oid_to_hex(&result, chex);
    if(!ext_ref_cas(e, ref, bhex, chex)) return;
    hub_git_changed(r, e->name);
    FuriString* s = furi_string_alloc_printf(
        "{\"ok\":true,\"commit\":\"%s\",\"ff\":%s}", chex, fast ? "true" : "false");
    hub_send_json(r, 200, s);
    furi_string_free(s);
}
