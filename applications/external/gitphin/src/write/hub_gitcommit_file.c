/* Commits: the shared helpers and hub_git_commit_file (put or delete one file).
 * Part of hub_gitcommit.c: included there, in this order, not built alone. */

/* ---------- helpers ---------- */

static int map_wr(int rc) {
    if(rc == HUB_GW_OK) return HUB_GIT_OK;
    return rc == HUB_GW_ERR_NOMEM ? HUB_GIT_ERR_NOMEM : HUB_GIT_ERR_IO;
}

static int write_tree(
    const HubGitWriteIo* io,
    const char* repo_dir,
    const uint8_t* data,
    size_t len,
    HubOid* out) {
    return map_wr(hub_gitwrite_object(io, repo_dir, HUB_GW_TREE, data, len, out->b, NULL));
}

/* ========== commit_file ========== */

typedef struct {
    HubOid oid;
    bool exists;
} Lvl;

typedef struct {
    Lvl lvl[HUB_GC_MAX_DEPTH];
    const char* seg[HUB_GC_MAX_DEPTH];
    uint16_t sl[HUB_GC_MAX_DEPTH];
    size_t n;
} CF;

static bool seg_ok(const char* s, size_t n) {
    if(n == 0 || n > 120) return false;
    if(n == 1 && s[0] == '.') return false;
    if(n == 2 && s[0] == '.' && s[1] == '.') return false;
    if(n == 4 && s[0] == '.' && (s[1] | 0x20) == 'g' && (s[2] | 0x20) == 'i' &&
       (s[3] | 0x20) == 't')
        return false;
    for(size_t i = 0; i < n; i++) {
        if((unsigned char)s[i] < 0x20 || s[i] == 0x7f) return false;
    }
    return true;
}

static bool split_path(CF* c, const char* path) {
    size_t len = strlen(path);
    if(len == 0 || len > HUB_GC_PATH_MAX) return false;
    const char* s = path;
    c->n = 0;
    for(size_t i = 0; i <= len; i++) {
        if(i == len || path[i] == '/') {
            if(c->n == HUB_GC_MAX_DEPTH || !seg_ok(s, (size_t)(path + i - s))) return false;
            c->seg[c->n] = s;
            c->sl[c->n] = (uint16_t)(path + i - s);
            c->n++;
            s = path + i + 1;
        }
    }
    return true;
}

/* Find the entry called name in a tree (any kind). */
static int tree_lookup(
    HubGit* g,
    const HubOid* tree,
    const char* name,
    size_t nl,
    bool* found,
    uint32_t* mode,
    HubOid* oid) {
    HubTreeCur c;
    *found = false;
    int rc = cur_open(g, tree, 0, &c);
    if(rc != HUB_GIT_OK) return rc;
    while(c.valid) {
        if(c.e.name_len == nl && memcmp(c.e.name, name, nl) == 0) {
            *found = true;
            *mode = c.e.mode;
            *oid = *c.e.oid;
            break;
        }
        cur_advance(&c);
    }
    cur_close(&c);
    return HUB_GIT_OK;
}

/* Replace / insert (op put) or remove (op del) one entry of a tree. The tree
 * is the raw object (NULL = empty). *out is malloc'd. */
static int splice(
    HubGit* g,
    const HubOid* tree,
    const char* name,
    size_t nl,
    bool put,
    uint32_t mode,
    const HubOid* oid,
    uint8_t** out,
    size_t* olen) {
    HubTreeCur c;
    int rc = cur_open(g, tree, 0, &c);
    if(rc != HUB_GIT_OK) return rc;
    bool is_tree = mode == 0040000;
    bool match = false;
    size_t ms = 0, me = 0, ins = c.len;
    bool have_ins = false;
    while(c.valid) {
        if(c.e.name_len == nl && memcmp(c.e.name, name, nl) == 0) {
            match = true;
            ms = c.pos;
            me = c.next;
        }
        if(!have_ins &&
           hub_gitwrite_ent_cmp(c.e.name, c.e.name_len, cur_is_tree(&c), name, nl, is_tree) > 0) {
            ins = c.pos;
            have_ins = true;
        }
        cur_advance(&c);
    }
    if(!put && !match) {
        cur_close(&c);
        return HUB_GIT_ERR_NOTFOUND;
    }
    size_t cut_a = match ? ms : ins;
    size_t cut_b = match ? me : ins;
    uint8_t* o = malloc(c.len + nl + 40);
    if(!o) {
        cur_close(&c);
        return HUB_GIT_ERR_NOMEM;
    }
    size_t pos = cut_a;
    if(pos) memcpy(o, c.buf, pos);
    if(put) pos += hub_gitwrite_ent_put(o + pos, mode, name, nl, oid->b);
    if(c.len > cut_b) {
        memcpy(o + pos, c.buf + cut_b, c.len - cut_b);
        pos += c.len - cut_b;
    }
    cur_close(&c);
    if(pos > hub_git_limit(g)) {
        free(o);
        return HUB_GIT_ERR_TOOLARGE;
    }
    *out = o;
    *olen = pos;
    return HUB_GIT_OK;
}

__attribute__((noinline)) static int commit_file_run(
    CF* c,
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* tip,
    const void* content,
    size_t len,
    const char* message,
    const HubGwIdent* who,
    HubOid* out_commit,
    HubOid* out_tree) {
    bool put = content != NULL;
    size_t n = c->n;
    int rc;

    if(tip) {
        rc = hub_git_tree_of(g, tip, &c->lvl[0].oid);
        if(rc != HUB_GIT_OK) return rc;
        c->lvl[0].exists = true;
    }
    /* walk down, remember the tree id of every level */
    uint32_t old_mode = 0;
    HubOid old_oid;
    bool leaf_found = false;
    for(size_t k = 0; k < n; k++) {
        bool last = k == n - 1;
        bool found = false;
        uint32_t mode = 0;
        HubOid oid;
        if(c->lvl[k].exists) {
            rc = tree_lookup(g, &c->lvl[k].oid, c->seg[k], c->sl[k], &found, &mode, &oid);
            if(rc != HUB_GIT_OK) return rc;
        }
        if(!last) {
            if(found && mode != 0040000) return put ? HUB_GC_ERR_CONFLICT : HUB_GIT_ERR_NOTFOUND;
            if(!found && !put) return HUB_GIT_ERR_NOTFOUND;
            c->lvl[k + 1].exists = found;
            if(found) c->lvl[k + 1].oid = oid;
        } else {
            if(found && mode == 0040000) return HUB_GC_ERR_CONFLICT;
            if(found && put && mode == 0160000) return HUB_GC_ERR_CONFLICT;
            if(!found && !put) return HUB_GIT_ERR_NOTFOUND;
            leaf_found = found;
            old_mode = mode;
            if(found) old_oid = oid;
        }
    }

    /* the leaf */
    uint32_t mode = 0100644;
    HubOid child;
    if(put) {
        if(leaf_found && (old_mode == 0100644 || old_mode == 0100755 || old_mode == 0120000)) {
            mode = old_mode;
        }
        rc = map_wr(hub_gitwrite_object(io, repo_dir, HUB_GW_BLOB, content, len, child.b, NULL));
        if(rc != HUB_GIT_OK) return rc;
        if(leaf_found && old_mode == mode && memcmp(&old_oid, &child, 20) == 0) {
            return HUB_GC_ERR_UNCHANGED;
        }
    }

    /* rebuild bottom-up */
    bool op_put = put;
    const char* name = c->seg[n - 1];
    size_t nl = c->sl[n - 1];
    HubOid root;
    for(size_t k = n; k-- > 0;) {
        uint8_t* buf;
        size_t blen;
        rc = splice(
            g,
            c->lvl[k].exists ? &c->lvl[k].oid : NULL,
            name,
            nl,
            op_put,
            mode,
            &child,
            &buf,
            &blen);
        if(rc != HUB_GIT_OK) return rc;
        if(k > 0 && blen == 0) {
            free(buf);
            op_put = false; /* the folder is empty now: remove it from its parent */
        } else {
            HubOid t;
            rc = write_tree(io, repo_dir, buf, blen, &t);
            free(buf);
            if(rc != HUB_GIT_OK) return rc;
            child = t;
            op_put = true;
            mode = 0040000;
            if(k == 0) root = t;
        }
        if(k > 0) {
            name = c->seg[k - 1];
            nl = c->sl[k - 1];
        }
    }
    if(tip && memcmp(&root, &c->lvl[0].oid, 20) == 0) return HUB_GC_ERR_UNCHANGED;
    if(out_tree) *out_tree = root;

    size_t clen;
    char* cbuf = hub_gitwrite_commit_build(
        root.b, tip ? (const uint8_t(*)[20])tip->b : NULL, tip ? 1 : 0, who, who, message, &clen);
    if(!cbuf) return HUB_GIT_ERR_NOMEM;
    HubOid cid;
    rc = map_wr(hub_gitwrite_object(io, repo_dir, HUB_GW_COMMIT, cbuf, clen, cid.b, NULL));
    free(cbuf);
    if(rc != HUB_GIT_OK) return rc;
    if(out_commit) *out_commit = cid;
    return HUB_GIT_OK;
}

int hub_git_commit_file(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* tip,
    const char* path,
    const void* content,
    size_t len,
    const char* message,
    const HubGwIdent* who,
    HubOid* out_commit,
    HubOid* out_tree) {
    CF* c = calloc(1, sizeof(CF));
    if(!c) return HUB_GIT_ERR_NOMEM;
    int rc;
    if(!split_path(c, path)) {
        rc = HUB_GC_ERR_BADPATH;
    } else {
        rc = commit_file_run(
            c, g, io, repo_dir, tip, content, len, message, who, out_commit, out_tree);
    }
    free(c);
    return rc;
}
