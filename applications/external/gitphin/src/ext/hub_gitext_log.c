/* Query plugin: log, commit, storage (streamed, the browse routes that live in this plugin).
 * Part of hub_gitext.c: included there, in this order, not built alone. */
#include "hub_gitext_int.h"

static void route_log(Ext* e) {
    if(!ext_open(e, EXT_LOG_OBJ, 20 * 1024))
        return; /* streamed: 8 KB object + 4.5 KB inflate + chunk + scratch */
    HubReq* r = e->r;
    HubOid oid;
    int rc;
    if(!ext_ref_param(e, "ref", "sha", &oid)) return;

    char num[12];
    unsigned skip = 0, n = 30;
    if(hub_query_get(r->query, "skip", num, sizeof(num))) skip = strtoul(num, NULL, 10);
    if(hub_query_get(r->query, "n", num, sizeof(num))) n = strtoul(num, NULL, 10);
    if(n < 1) n = 1;
    if(n > EXT_LOG_MAX) n = EXT_LOG_MAX;

    /* the commits that are skipped need only their first parent: the graph has it */
    HubCommitMeta* m = malloc(sizeof(HubCommitMeta));
    HubChunk* ck = malloc(sizeof(HubChunk));
    uint8_t* data = NULL;
    HubGitCommit* cm = malloc(sizeof(HubGitCommit));
    FuriString* t = furi_string_alloc();
    if(!m || !ck || !cm) {
        hub_send_error(r, 503, "low memory, try again");
        goto out;
    }
    bool end = false;
    for(unsigned i = 0; i < skip && !end; i++) {
        if(hub_git_commit_meta(e->g, &oid, m) != HUB_GIT_OK || m->nparents == 0) {
            end = true;
        } else {
            oid = m->parent[0];
        }
    }
    unsigned done = 0;
    bool more = false, started = false, good = true;
    while(!end && done < n) {
        rc = ext_read_commit(e, &oid, &data, cm);
        if(rc != HUB_GIT_OK) {
            if(!started && skip == 0) {
                ext_err(e, rc);
                goto out;
            }
            break; /* the page asks again */
        }
        if(!started) {
            if(!hub_chunk_begin(ck, r, "application/json", hub_cache_hdr(e->p1))) goto out;
            hub_chunk_lit(ck, "{\"ok\":true,\"commits\":[");
            started = true;
        }
        if(done) hub_chunk_lit(ck, ",");
        ext_chunk_commit(ck, t, &oid, cm, EXT_LOG_MSG, true);
        done++;
        bool has_parent = cm->nparents > 0;
        if(has_parent) oid = cm->parent[0];
        hub_git_free(data);
        data = NULL;
        more = has_parent;
        if(!has_parent) end = true;
        hub_beat();
    }
    if(!started) { /* skip went past the end of the history */
        if(!hub_chunk_begin(ck, r, "application/json", hub_cache_hdr(e->p1))) goto out;
        hub_chunk_lit(ck, "{\"ok\":true,\"commits\":[");
    }
    furi_string_printf(t, "],\"more\":%s,\"next_skip\":%u}", more ? "true" : "false", skip + done);
    hub_chunk_fs(ck, t);
    good = ck->ok;
    hub_chunk_end(ck, good);
out:
    furi_string_free(t);
    free(cm);
    free(ck);
    free(m);
}

static void route_commit(Ext* e) {
    if(!ext_open(e, 16 * 1024, 28 * 1024))
        return; /* 16 KB object + 4.5 KB inflate + chunk + scratch */
    HubOid oid;
    int rc;
    if(!ext_ref_param(e, "sha", "ref", &oid)) return;
    uint8_t* data;
    HubGitCommit* cm = malloc(sizeof(HubGitCommit));
    HubChunk* ck = malloc(sizeof(HubChunk));
    FuriString* t = furi_string_alloc();
    if(!cm || !ck) {
        hub_send_error(e->r, 503, "low memory, try again");
    } else if((rc = ext_read_commit(e, &oid, &data, cm)) != HUB_GIT_OK) {
        ext_err(e, rc);
    } else {
        if(hub_chunk_begin(ck, e->r, "application/json", hub_cache_hdr(e->p1))) {
            hub_chunk_lit(ck, "{\"ok\":true,");
            ext_chunk_commit(ck, t, &oid, cm, EXT_COMMIT_MSG, false);
            hub_chunk_end(ck, ck->ok);
        }
        hub_git_free(data);
    }
    furi_string_free(t);
    free(ck);
    free(cm);
}

/* GET /api/git/storage?name= : pack and loose object counts. Opens the 256
 * object folders, so it takes a few seconds on a big repo. */
static void route_storage(Ext* e) {
    HubReq* r = e->r;
    if(!hub_heap_ok(r, 8 * 1024)) return;
    char dir[128];
    char* name = e->name;
    if(!hub_query_get(r->query, "name", name, sizeof(e->name)) || !hub_git_name_ok(name)) {
        hub_send_error(r, 400, "bad repo name");
        return;
    }
    snprintf(dir, sizeof(dir), EXT_ROOT "/%s.git/objects", name);
    FileInfo fi;
    if(storage_common_stat(r->storage, dir, &fi) != FSE_OK) {
        hub_send_error(r, 404, "repository not found");
        return;
    }
    File* d = storage_file_alloc(r->storage);
    unsigned packs = 0, loose = 0;
    uint64_t pack_bytes = 0;
    char fname[64];
    char sub[160];

    snprintf(sub, sizeof(sub), "%s/pack", dir);
    if(storage_dir_open(d, sub)) {
        while(storage_dir_read(d, &fi, fname, sizeof(fname))) {
            size_t n = strlen(fname);
            if(file_info_is_dir(&fi)) continue;
            if(n > 5 && strcmp(fname + n - 5, ".pack") == 0) packs++;
            if((n > 5 && strcmp(fname + n - 5, ".pack") == 0) ||
               (n > 4 && strcmp(fname + n - 4, ".idx") == 0))
                pack_bytes += fi.size;
        }
    }
    storage_dir_close(d);

    for(unsigned i = 0; i < 256; i++) {
        hub_beat();
        snprintf(sub, sizeof(sub), "%s/%02x", dir, i);
        if(!storage_dir_open(d, sub)) {
            storage_file_free(d); /* never reuse a File after a failed open */
            d = storage_file_alloc(r->storage);
            continue;
        }
        while(storage_dir_read(d, &fi, fname, sizeof(fname))) {
            if(!file_info_is_dir(&fi)) loose++;
        }
        storage_dir_close(d);
    }
    storage_file_free(d);

    char out[96];
    int n = snprintf(
        out,
        sizeof(out),
        "{\"ok\":true,\"packs\":%u,\"pack_kib\":%lu,\"loose\":%u}",
        packs,
        (unsigned long)(pack_bytes / 1024),
        loose);
    hub_send_body(r, 200, "application/json", out, (size_t)n);
}
