/* Git routes: the dumb HTTP files and the /api/git repo routes.
 * Part of hub_git.c: included there, in this order, not built alone. */
#include "hub_git_int.h"

/* ---------- routes: dumb HTTP files ---------- */

static void git_route_file(HubReq* r, GitCtx* g) {
    /* /git/<name>.git/<rest> */
    const char* p = r->path + 5;
    const char* slash = strchr(p, '/');
    size_t seg = slash ? (size_t)(slash - p) : strlen(p);
    if(!slash || seg < 5 || seg > GIT_NAME_MAX + 4 || strncmp(p + seg - 4, ".git", 4) != 0) {
        hub_send_error(r, 404, "not found");
        return;
    }
    memcpy(g->name, p, seg - 4);
    g->name[seg - 4] = '\0';
    if(!git_name_ok(g->name)) {
        hub_send_error(r, 404, "not found");
        return;
    }

    /* no empty segments, no "." or ".." */
    const char* rest = slash + 1;
    size_t rest_len = strlen(rest);
    if(rest_len == 0 || rest_len > 150) {
        hub_send_error(r, 404, "not found");
        return;
    }
    for(const char* s = rest; *s;) {
        size_t n = strcspn(s, "/");
        if(n == 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) {
            hub_send_error(r, 400, "bad path");
            return;
        }
        s += n + (s[n] == '/' ? 1 : 0);
    }
    if(strchr(rest, '\\') || rest[rest_len - 1] == '/') {
        hub_send_error(r, 400, "bad path");
        return;
    }

    git_set_base(g, g->name);
    /* push to create: the receive-pack discovery of a missing repo makes it */
    if(strcmp(rest, "info/refs") == 0 && r->query &&
       strstr(r->query, "service=git-receive-pack") && !git_is_dir(g, g->base)) {
        hub_git_mkroot(r->storage);
        strcpy(g->ref, "refs/heads/main");
        strcpy(g->text, "Unnamed repository");
        if(!git_create(g)) {
            hub_send_error(r, 500, "could not make the repository");
            return;
        }
    }
    /* stale after a WebDAV push: rebuild what the reader is about to get */
    if(strcmp(g->name, git_dirty) == 0 &&
       (strcmp(rest, "info/refs") == 0 || strcmp(rest, "objects/info/packs") == 0)) {
        git_dirty[0] = '\0';
        git_write_info_refs(g);
        git_write_info_packs(g);
    }
    snprintf(g->path, sizeof(g->path), "%s/%s", g->base, rest);
    bool text = strcmp(rest, "info/refs") == 0 || strcmp(rest, "HEAD") == 0 ||
                strcmp(rest, "objects/info/packs") == 0;
    hub_serve_file_as(r, g->path, text ? "text/plain" : "application/octet-stream", false);
}

/* ---------- routes: API ---------- */

static void git_route_repos(GitCtx* g) {
    HubReq* r = g->r;
    char* names = malloc(GIT_REPOS_MAX * (GIT_NAME_MAX + 1));
    if(!names) {
        hub_send_error(r, 503, "low memory, try again");
        return;
    }
    size_t count = 0;

    File* dir = storage_file_alloc(r->storage);
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(count < GIT_REPOS_MAX && storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(!file_info_is_dir(&info) || len < 5 || strcmp(name + len - 4, ".git") != 0)
                continue;
            name[len - 4] = '\0';
            if(!git_name_ok(name)) continue;
            strcpy(names + count * (GIT_NAME_MAX + 1), name);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    char tmp_name[GIT_NAME_MAX + 1];
    for(size_t i = 1; i < count; i++) {
        size_t w = GIT_NAME_MAX + 1;
        memcpy(tmp_name, names + i * w, w);
        size_t j = i;
        while(j > 0 && strcmp(names + (j - 1) * w, tmp_name) > 0) {
            memcpy(names + j * w, names + (j - 1) * w, w);
            j--;
        }
        memcpy(names + j * w, tmp_name, w);
    }

    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"repos\":[");
    char head[GIT_REF_MAX];
    for(size_t i = 0; i < count; i++) {
        const char* name = names + i * (GIT_NAME_MAX + 1);
        git_set_base(g, name);

        snprintf(g->tmp, sizeof(g->tmp), "%s/description", g->base);
        FuriString* d = git_slurp(g, g->tmp, 1024);
        char desc[GIT_TEXT_MAX + 1] = {0};
        if(d) {
            size_t n = strcspn(furi_string_get_cstr(d), "\r\n");
            if(n > GIT_TEXT_MAX) n = GIT_TEXT_MAX;
            memcpy(desc, furi_string_get_cstr(d), n);
            furi_string_free(d);
        }

        git_head_ref(g, head, sizeof(head));
        const char* branch = strncmp(head, "refs/heads/", 11) == 0 ? head + 11 : "";

        if(i) furi_string_push_back(s, ',');
        furi_string_cat_str(s, "{\"name\":");
        hub_json_str(s, name);
        furi_string_cat_str(s, ",\"description\":");
        hub_json_str(s, desc);
        furi_string_cat_str(s, ",\"head\":");
        hub_json_str(s, branch);
        furi_string_cat_printf(s, ",\"packs\":%d}", git_packs(g, NULL));
    }
    furi_string_cat_str(s, "]}");
    free(names);
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

/* Make the bare skeleton. g->base, g->ref (full HEAD branch) and g->text
 * (description) are set. Removes the half made repo on failure. */
static bool git_create(GitCtx* g) {
    static const char* const dirs[] = {
        "",
        "/refs",
        "/refs/heads",
        "/refs/tags",
        "/objects",
        "/objects/pack",
        "/objects/info",
        "/info"};
    bool ok = hub_git_mkroot(g->r->storage);
    for(size_t i = 0; ok && i < COUNT_OF(dirs); i++) {
        snprintf(g->path, sizeof(g->path), "%s%s", g->base, dirs[i]);
        ok = storage_simply_mkdir(g->r->storage, g->path);
    }

    FuriString* s = furi_string_alloc();
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/HEAD", g->base);
        furi_string_printf(s, "ref: %s\n", g->ref);
        ok = git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    }
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/config", g->base);
        ok = git_write(g, g->path, GIT_SKELETON_CONFIG, sizeof(GIT_SKELETON_CONFIG) - 1);
    }
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/description", g->base);
        furi_string_printf(s, "%s\n", g->text);
        ok = git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    }
    furi_string_free(s);
    if(ok) ok = git_write_info_refs(g);
    if(ok) ok = git_write_info_packs(g);

    if(!ok) storage_simply_remove_recursive(g->r->storage, g->base);
    return ok;
}

static void git_route_create(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", false)) return;
    if(storage_common_stat(r->storage, g->base, NULL) == FSE_OK) {
        hub_send_error(r, 409, "repository exists");
        return;
    }

    char branch[GIT_REF_MAX];
    if(!hub_query_get(r->query, "branch", branch, sizeof(branch)) || branch[0] == '\0') {
        strcpy(branch, "main");
    }
    snprintf(g->ref, sizeof(g->ref), "refs/heads/%s", branch);
    if(!git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad branch name");
        return;
    }
    if(!hub_query_get(r->query, "description", g->text, sizeof(g->text)) || g->text[0] == '\0') {
        strcpy(g->text, "Unnamed repository");
    }
    for(char* c = g->text; *c; c++) {
        if(*c == '\r' || *c == '\n') *c = ' ';
    }

    if(git_create(g)) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "could not make the repository");
    }
}

static void git_route_delete(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(storage_simply_remove_recursive(g->r->storage, g->base)) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "could not delete the repository");
    }
}

static void git_route_rename(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    char from[sizeof(g->base)];
    strcpy(from, g->base);
    if(!git_need_repo(g, "to", false)) return;
    if(storage_common_stat(r->storage, g->base, NULL) == FSE_OK) {
        hub_send_error(r, 409, "repository exists");
        return;
    }
    FS_Error err = storage_common_rename(r->storage, from, g->base);
    if(err == FSE_OK) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, storage_error_get_desc(err));
    }
}

static void git_route_describe(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_query_get(g->r->query, "text", g->text, sizeof(g->text))) {
        hub_send_error(g->r, 400, "missing text");
        return;
    }
    for(char* c = g->text; *c; c++) {
        if(*c == '\r' || *c == '\n') *c = ' ';
    }
    /* strcat is not in the release firmware's API; also keeps room for the newline */
    size_t tl = strlen(g->text);
    if(tl > sizeof(g->text) - 2) tl = sizeof(g->text) - 2;
    g->text[tl] = '\n';
    g->text[tl + 1] = '\0';
    snprintf(g->path, sizeof(g->path), "%s/description", g->base);
    if(git_write(g, g->path, g->text, strlen(g->text))) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "write failed");
    }
}

static void git_route_refs(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    GitRefs l;
    if(!git_refs_load(g, &l)) {
        git_refs_free(&l);
        hub_send_error(g->r, 503, "low memory or too many refs, try again");
        return;
    }
    git_head_ref(g, g->ref, sizeof(g->ref));

    /* annotated tags get "peeled": the commit they point to */
    HubGit* og = NULL;
    for(size_t i = 0; i < l.n && !og; i++) {
        if(strncmp(l.v[i].name, "refs/tags/", 10) == 0) {
            og = hub_git_open(g->r->storage, g->base);
            if(og) hub_git_set_limit(og, 4 * 1024); /* tags are small */
        }
    }

    HubChunk* ck = malloc(sizeof(HubChunk));
    if(!ck || !hub_chunk_begin(ck, g->r, "application/json", NULL)) {
        if(!ck) hub_send_error(g->r, 503, "low memory, try again");
        free(ck);
        if(og) hub_git_close(og);
        git_refs_free(&l);
        return;
    }
    hub_chunk_lit(ck, "{\"ok\":true,\"head\":");
    hub_chunk_json(ck, g->ref, strlen(g->ref));
    hub_chunk_lit(ck, ",\"refs\":[");
    char line[160];
    for(size_t i = 0; i < l.n && ck->ok; i++) {
        if(i) hub_chunk_lit(ck, ",");
        hub_chunk_lit(ck, "{\"name\":");
        hub_chunk_json(ck, l.v[i].name, strlen(l.v[i].name));
        int n = snprintf(line, sizeof(line), ",\"sha\":\"%s\"", l.v[i].sha);
        HubOid oid;
        uint8_t type;
        size_t size;
        if(og && strncmp(l.v[i].name, "refs/tags/", 10) == 0 &&
           hub_oid_from_hex(l.v[i].sha, &oid) &&
           hub_git_info(og, &oid, &type, &size) == HUB_GIT_OK && type == HUB_OBJ_TAG &&
           hub_git_peel(og, &oid) == HUB_GIT_OK) {
            char hex[41];
            hub_oid_to_hex(&oid, hex);
            n += snprintf(line + n, sizeof(line) - (size_t)n, ",\"peeled\":\"%s\"", hex);
        }
        line[n++] = '}';
        hub_chunk_write(ck, line, (size_t)n);
    }
    hub_chunk_lit(ck, "]}");
    hub_chunk_end(ck, ck->ok);
    free(ck);
    if(og) hub_git_close(og);
    git_refs_free(&l);
}

/* Remove the empty folders a deleted ref leaves behind (refs/heads/a/b -> a). */
static void git_prune_dirs(GitCtx* g, const char* ref) {
    size_t blen = strlen(g->base);
    snprintf(g->tmp, sizeof(g->tmp), "%s/%s", g->base, ref);
    char* slash;
    while((slash = strrchr(g->tmp, '/')) != NULL) {
        *slash = '\0';
        if(strcmp(g->tmp + blen, "/refs/heads") == 0 || strcmp(g->tmp + blen, "/refs/tags") == 0)
            break;
        /* fails while the folder still has files in it, which is what we want */
        if(storage_common_remove(g->r->storage, g->tmp) != FSE_OK) break;
    }
}

static void git_route_ref(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_query_get(r->query, "ref", g->ref, sizeof(g->ref)) || !git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad ref name");
        return;
    }
    if(!hub_query_get(r->query, "old", g->osha, sizeof(g->osha)) || !git_sha_ok(g->osha) ||
       !hub_query_get(r->query, "new", g->nsha, sizeof(g->nsha)) || !git_sha_ok(g->nsha)) {
        hub_send_error(r, 400, "bad sha");
        return;
    }

    char cur[41];
    if(!git_ref_current(g, g->ref, cur)) strcpy(cur, GIT_ZERO);
    if(strcmp(cur, g->osha) != 0) {
        hub_send_error(r, 409, "ref changed");
        return;
    }

    bool ok = true;
    if(strcmp(g->nsha, GIT_ZERO) == 0) {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->ref);
        storage_common_remove(r->storage, g->path);
        ok = git_packed_drop(g, g->ref);
        git_prune_dirs(g, g->ref);
    } else {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->ref);
        if(!git_mkdirs(g, g->path)) {
            hub_send_error(r, 409, "ref conflicts with another ref");
            return;
        }
        g->nsha[40] = '\n';
        if(!git_write(g, g->path, g->nsha, 41)) {
            hub_send_error(r, 409, "ref conflicts with another ref");
            return;
        }
    }
    if(ok) ok = git_write_info_refs(g);
    if(ok) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "write failed");
    }
}

static void git_route_head(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    char branch[GIT_REF_MAX];
    if(!hub_query_get(r->query, "branch", branch, sizeof(branch))) {
        hub_send_error(r, 400, "missing branch");
        return;
    }
    snprintf(g->ref, sizeof(g->ref), "refs/heads/%s", branch);
    if(!git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad branch name");
        return;
    }
    snprintf(g->text, sizeof(g->text), "ref: %s\n", g->ref);
    snprintf(g->path, sizeof(g->path), "%s/HEAD", g->base);
    if(git_write(g, g->path, g->text, strlen(g->text))) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "write failed");
    }
}

static void git_route_server_info(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(git_write_info_refs(g) && git_write_info_packs(g)) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "write failed");
    }
}
