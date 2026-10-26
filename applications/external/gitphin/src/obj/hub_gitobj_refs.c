/* Object reader: refs, peeling, names to commits and trees.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

/* ---------- refs ---------- */

static bool ref_name_ok(const char* n) {
    size_t len = strlen(n);
    if(len == 0 || len > HUB_GIT_REF_MAX || n[0] == '/' || n[len - 1] == '/') return false;
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)n[i];
        if(c <= 0x20 || c == 0x7f || strchr("~^:?*[\\", c)) return false;
        if(c == '.' && n[i + 1] == '.') return false;
        if(c == '/' && n[i + 1] == '/') return false;
        if(c == '@' && n[i + 1] == '{') return false;
    }
    return true;
}

/* Find name in packed-refs. */
static int packed_ref(HubGit* g, const char* name, HubOid* oid) {
    snprintf(g->path, sizeof(g->path), "%s/packed-refs", g->base);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return HUB_GIT_ERR_NOTFOUND;
    size_t nlen = strlen(name);
    size_t ll = 0;
    bool skip = false, found = false;
    size_t n;
    while(!found && (n = hub_gitio_read(f, g->buf, sizeof(g->buf))) > 0) {
        for(size_t i = 0; i < n && !found; i++) {
            char c = (char)g->buf[i];
            if(c != '\n') {
                if(ll < sizeof(g->line) - 1)
                    g->line[ll++] = c;
                else
                    skip = true;
                continue;
            }
            g->line[ll] = '\0';
            if(!skip && ll >= 42 && g->line[0] != '#' && g->line[0] != '^' && g->line[40] == ' ' &&
               ll - 41 == nlen && memcmp(g->line + 41, name, nlen) == 0) {
                g->line[40] = '\0';
                found = hub_oid_from_hex(g->line, oid);
                if(!found) {
                    hub_gitio_close(f);
                    return HUB_GIT_ERR_CORRUPT;
                }
            }
            ll = 0;
            skip = false;
        }
    }
    hub_gitio_close(f);
    return found ? HUB_GIT_OK : HUB_GIT_ERR_NOTFOUND;
}

int hub_git_ref_read(HubGit* g, const char* name, HubOid* oid) {
    if(!ref_name_ok(name)) return HUB_GIT_ERR_BADARG;
    strcpy(g->refname, name);
    for(int depth = 0; depth < SYMREF_MAX; depth++) {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->refname);
        HubGitFile* f = hub_gitio_open(g->io, g->path);
        if(!f) return packed_ref(g, g->refname, oid);
        size_t n = hub_gitio_read(f, g->buf, 255);
        hub_gitio_close(f);
        g->buf[n] = '\0';
        while(n > 0 && (g->buf[n - 1] == '\n' || g->buf[n - 1] == '\r' || g->buf[n - 1] == ' '))
            g->buf[--n] = '\0';
        if(n > 5 && memcmp(g->buf, "ref: ", 5) == 0) {
            const char* t = (const char*)g->buf + 5;
            if(!ref_name_ok(t)) return HUB_GIT_ERR_CORRUPT;
            strcpy(g->refname, t);
            continue;
        }
        if(n == 40 && hub_oid_from_hex((const char*)g->buf, oid)) return HUB_GIT_OK;
        return HUB_GIT_ERR_CORRUPT;
    }
    return HUB_GIT_ERR_CORRUPT;
}

int hub_git_peel(HubGit* g, HubOid* oid) {
    for(int i = 0; i < PEEL_MAX; i++) {
        uint8_t type;
        size_t size;
        int r = hub_git_info(g, oid, &type, &size);
        if(r) return r;
        if(type != HUB_OBJ_TAG) return HUB_GIT_OK;
        uint8_t* data;
        r = hub_git_read(g, oid, &type, &data, &size);
        if(r) return r;
        HubGitTag t;
        r = hub_git_parse_tag(data, size, &t);
        if(r == HUB_GIT_OK) *oid = t.object;
        free(data);
        if(r) return r;
    }
    return HUB_GIT_ERR_CORRUPT;
}

int hub_git_resolve(HubGit* g, const char* ref, HubOid* oid) {
    size_t len = strlen(ref);
    if(len == 0 || len > HUB_GIT_REF_MAX - 12) return HUB_GIT_ERR_BADARG;
    int r = HUB_GIT_ERR_NOTFOUND;
    if(len == 40 && hub_oid_from_hex(ref, oid)) {
        r = HUB_GIT_OK;
    } else if(strcmp(ref, "HEAD") == 0) {
        r = hub_git_ref_read(g, "HEAD", oid);
    } else {
        if(strncmp(ref, "refs/", 5) == 0) r = hub_git_ref_read(g, ref, oid);
        if(r == HUB_GIT_ERR_NOTFOUND || r == HUB_GIT_ERR_BADARG) {
            snprintf(g->cand, sizeof(g->cand), "refs/heads/%s", ref);
            r = hub_git_ref_read(g, g->cand, oid);
        }
        if(r == HUB_GIT_ERR_NOTFOUND) {
            snprintf(g->cand, sizeof(g->cand), "refs/tags/%s", ref);
            r = hub_git_ref_read(g, g->cand, oid);
        }
    }
    if(r) return r;
    return hub_git_peel(g, oid);
}

int hub_git_tree_of(HubGit* g, const HubOid* oid, HubOid* tree) {
    HubOid cur = *oid;
    for(int i = 0; i < PEEL_MAX; i++) {
        uint8_t type;
        size_t size;
        uint8_t* data;
        int r = hub_git_read(g, &cur, &type, &data, &size);
        if(r) return r;
        if(type == HUB_OBJ_TREE) {
            free(data);
            *tree = cur;
            return HUB_GIT_OK;
        }
        if(type == HUB_OBJ_COMMIT) {
            HubGitCommit c;
            r = hub_git_parse_commit(data, size, &c);
            if(r == HUB_GIT_OK) *tree = c.tree;
            free(data);
            return r;
        }
        if(type == HUB_OBJ_TAG) {
            HubGitTag t;
            r = hub_git_parse_tag(data, size, &t);
            if(r == HUB_GIT_OK) cur = t.object;
            free(data);
            if(r) return r;
            continue;
        }
        free(data);
        return HUB_GIT_ERR_BADARG;
    }
    return HUB_GIT_ERR_CORRUPT;
}
