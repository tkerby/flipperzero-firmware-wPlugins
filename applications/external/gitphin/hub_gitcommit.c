#include "hub_gitcommit.h"
#include "hub_gittreecur.h"

#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

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

/* ========== tree merge ========== */

typedef struct {
    HubOid o[3]; /* base, ours, theirs */
    bool h[3];
    size_t p[3];
    uint8_t* out;
    size_t olen, ocap;
    uint16_t prefix;
} MFrame;

typedef struct {
    HubGit* g;
    const HubGitWriteIo* io;
    const char* repo;
    HubGitPathCb cb;
    void* ctx;
    unsigned nconf;
    bool stop;
    MFrame* st;
    size_t cap, sp;
    char path[HUB_GC_PATH_MAX + 2];
    /* working copies live here, not on the 4 KB stack */
    MFrame fr_;
    HubTreeCur cur_[3];
    HubOid sub_[3];
} MCtx;

static void conflict(MCtx* m, size_t plen, const char* name, size_t nl) {
    if(m->nconf >= HUB_GM_MAX_CONFLICTS) {
        m->stop = true;
        return;
    }
    m->nconf++;
    if(plen + nl <= HUB_GC_PATH_MAX && m->cb) {
        memcpy(m->path + plen, name, nl);
        m->path[plen + nl] = '\0';
        if(!m->cb(m->ctx, m->path)) m->stop = true;
    }
    if(m->nconf >= HUB_GM_MAX_CONFLICTS) m->stop = true;
}

static int mpush(MCtx* m, const HubOid* o[3], size_t prefix) {
    if(m->sp == HUB_GC_MAX_DEPTH) return HUB_GIT_ERR_TOOLARGE;
    if(m->sp == m->cap) {
        size_t nc = m->cap ? m->cap * 2 : 8;
        MFrame* n = realloc(m->st, nc * sizeof(MFrame));
        if(!n) return HUB_GIT_ERR_NOMEM;
        m->st = n;
        m->cap = nc;
    }
    MFrame* f = &m->st[m->sp++];
    memset(f, 0, sizeof(*f));
    for(int i = 0; i < 3; i++) {
        if(o[i]) {
            f->o[i] = *o[i];
            f->h[i] = true;
        }
    }
    f->prefix = (uint16_t)prefix;
    return HUB_GIT_OK;
}

/* hint = size of the biggest input tree: the output is about that big, so the
 * buffer is made once (no doubling, which would double the peak) */
static int out_add(
    MFrame* f,
    uint32_t mode,
    const char* name,
    size_t nl,
    const HubOid* oid,
    size_t limit,
    size_t hint) {
    size_t need = f->olen + nl + 40;
    if(need > f->ocap) {
        size_t nc = f->ocap ? f->ocap + f->ocap / 4 : hint + 256;
        if(nc < need) nc = need + 256;
        uint8_t* n = realloc(f->out, nc);
        if(!n) return HUB_GIT_ERR_NOMEM;
        f->out = n;
        f->ocap = nc;
    }
    f->olen += hub_gitwrite_ent_put(f->out + f->olen, mode, name, nl, oid->b);
    return f->olen > limit ? HUB_GIT_ERR_TOOLARGE : HUB_GIT_OK;
}

/* A file and a folder with the same name side by side cannot be a tree. Returns
 * the number of reported duplicates (and reports them). */
static void dup_check(MCtx* m, const MFrame* f) {
    HubTreeIter a, b;
    HubTreeEntry ea, eb;
    hub_tree_iter_init(&a, f->out, f->olen);
    while(!m->stop && hub_tree_next(&a, &ea) == 1) {
        b = a;
        while(hub_tree_next(&b, &eb) == 1) {
            if(eb.name_len < ea.name_len || memcmp(eb.name, ea.name, ea.name_len) != 0) break;
            if(eb.name_len == ea.name_len) {
                conflict(m, f->prefix, ea.name, ea.name_len);
                break;
            }
        }
    }
}

static int same(const HubTreeCur* c[3], const bool pr[3], int a, int b) {
    if(!pr[a] && !pr[b]) return 1;
    if(!pr[a] || !pr[b]) return 0;
    return c[a]->e.mode == c[b]->e.mode && memcmp(c[a]->e.oid, c[b]->e.oid, 20) == 0;
}

#define fr  (m->fr_)
#define cur (m->cur_)
#define sub (m->sub_)

int hub_git_tree_merge(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* base,
    const HubOid* ours,
    const HubOid* theirs,
    HubOid* out,
    HubGitPathCb cb,
    void* ctx,
    unsigned* nconf) {
    MCtx* m = calloc(1, sizeof(MCtx));
    if(!m) return HUB_GIT_ERR_NOMEM;
    m->g = g;
    m->io = io;
    m->repo = repo_dir;
    m->cb = cb;
    m->ctx = ctx;
    size_t limit = hub_git_limit(g);
    int rc;
    {
        const HubOid* roots[3] = {base, ours, theirs};
        rc = mpush(m, roots, 0);
    }
    HubOid result;
    bool have_result = false;

    while(rc == HUB_GIT_OK && m->sp > 0 && !m->stop) {
        size_t fi = m->sp - 1;
        fr = m->st[fi];
        int opened = 0;
        for(; opened < 3 && rc == HUB_GIT_OK; opened++) {
            rc = cur_open(g, fr.h[opened] ? &fr.o[opened] : NULL, fr.p[opened], &cur[opened]);
            if(rc != HUB_GIT_OK) break;
        }
        if(rc != HUB_GIT_OK) {
            for(int i = 0; i < opened; i++)
                cur_close(&cur[i]);
            break;
        }
        size_t plen = fr.prefix;
        bool pushed = false;

        while((cur[0].valid || cur[1].valid || cur[2].valid) && !pushed && !m->stop) {
            /* the smallest head entry in git order */
            int mi = -1;
            for(int i = 0; i < 3; i++) {
                if(cur[i].valid && (mi < 0 || cur_cmp(&cur[i], &cur[mi]) < 0)) mi = i;
            }
            bool pr[3];
            const HubTreeCur* cp[3] = {&cur[0], &cur[1], &cur[2]};
            for(int i = 0; i < 3; i++)
                pr[i] = cur[i].valid && cur_cmp(&cur[i], &cur[mi]) == 0;
            const char* name = cur[mi].e.name;
            size_t nl = cur[mi].e.name_len;
            if(plen + nl + 1 > HUB_GC_PATH_MAX) {
                rc = HUB_GIT_ERR_TOOLARGE;
                break;
            }
            /* decide: -1 conflict, 0 absent, else side index + 1 to take, 4 = descend */
            int take;
            if(same(cp, pr, 1, 2)) {
                take = pr[1] ? 2 : 0;
            } else if(same(cp, pr, 1, 0)) {
                take = pr[2] ? 3 : 0;
            } else if(same(cp, pr, 2, 0)) {
                take = pr[1] ? 2 : 0;
            } else if(pr[1] && pr[2] && cur_is_tree(&cur[1])) {
                take = 4;
            } else {
                take = -1;
            }

            const HubTreeEntry* te = take == 2 ? &cur[1].e : take == 3 ? &cur[2].e : NULL;
            const HubOid* subp[3] = {NULL, NULL, NULL};
            if(take == 4) {
                for(int i = 0; i < 3; i++) {
                    if(pr[i] && cur_is_tree(&cur[i])) {
                        sub[i] = *cur[i].e.oid;
                        subp[i] = &sub[i];
                    }
                }
                memcpy(m->path + plen, name, nl);
                m->path[plen + nl] = '/';
            }
            if(take == -1) {
                conflict(m, plen, name, nl);
            } else if(te) {
                rc = out_add(
                    &fr,
                    te->mode,
                    name,
                    nl,
                    te->oid,
                    limit,
                    cur[1].len > cur[2].len ? cur[1].len : cur[2].len);
                if(rc != HUB_GIT_OK) {
                    m->st[fi] = fr;
                    for(int i = 0; i < 3; i++)
                        cur_close(&cur[i]);
                    goto done;
                }
            }
            for(int i = 0; i < 3; i++) {
                if(pr[i]) cur_advance(&cur[i]);
            }
            if(take == 4) {
                for(int i = 0; i < 3; i++)
                    fr.p[i] = cur[i].pos;
                m->st[fi] = fr; /* output so far and resume offsets */
                rc = mpush(m, subp, plen + nl + 1);
                if(rc != HUB_GIT_OK) {
                    for(int i = 0; i < 3; i++)
                        cur_close(&cur[i]);
                    goto done;
                }
                pushed = true;
            }
        }
        for(int i = 0; i < 3; i++)
            cur_close(&cur[i]);
        if(rc != HUB_GIT_OK) {
            m->st[fi] = fr;
            break;
        }
        if(pushed || m->stop) {
            if(!pushed) m->st[fi] = fr;
            continue;
        }
        /* this folder is done */
        m->st[fi] = fr;
        dup_check(m, &fr);
        HubOid tid;
        bool empty = fr.olen == 0;
        if(m->nconf == 0 && (fi == 0 || !empty)) {
            rc = write_tree(io, repo_dir, fr.out, fr.olen, &tid);
            if(rc != HUB_GIT_OK) break;
        }
        free(fr.out);
        m->st[fi].out = NULL;
        m->sp--;
        if(fi == 0) {
            result = tid;
            have_result = m->nconf == 0;
        } else if(m->nconf == 0 && !empty) {
            MFrame* par = &m->st[fi - 1];
            size_t pl = par->prefix;
            size_t nl2 = fr.prefix - pl - 1;
            rc = out_add(par, 0040000, m->path + pl, nl2, &tid, limit, 512);
            /* m->path holds the child's name only if no sibling overwrote it: it
             * did not, the child was the last thing written */
        }
    }
done:
    if(rc == HUB_GIT_OK && m->stop && m->nconf == 0) rc = HUB_GIT_ERR_WRITE;
    if(nconf) *nconf = m->nconf;
    if(rc == HUB_GIT_OK) {
        if(m->nconf > 0) {
            rc = HUB_GC_ERR_MERGE;
        } else if(have_result) {
            *out = result;
        } else {
            rc = HUB_GIT_ERR_CORRUPT;
        }
    }
    for(size_t i = 0; i < m->sp; i++)
        free(m->st[i].out);
    free(m->st);
    free(m);
    return rc;
}

#undef fr
#undef cur
#undef sub

int hub_git_merge_commits(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* ours_tip,
    const HubOid* theirs_tip,
    const HubOid* merge_base,
    const char* message,
    const HubGwIdent* who,
    HubOid* out_commit,
    HubOid* out_tree,
    HubGitPathCb cb,
    void* ctx,
    unsigned* nconf) {
    HubOid bt, ot, tt, merged;
    int rc = hub_git_tree_of(g, ours_tip, &ot);
    if(rc == HUB_GIT_OK) rc = hub_git_tree_of(g, theirs_tip, &tt);
    if(rc == HUB_GIT_OK && merge_base) rc = hub_git_tree_of(g, merge_base, &bt);
    if(rc != HUB_GIT_OK) return rc;
    rc = hub_git_tree_merge(
        g, io, repo_dir, merge_base ? &bt : NULL, &ot, &tt, &merged, cb, ctx, nconf);
    if(rc != HUB_GIT_OK) return rc;
    if(out_tree) *out_tree = merged;
    uint8_t parents[2][20];
    memcpy(parents[0], ours_tip->b, 20);
    memcpy(parents[1], theirs_tip->b, 20);
    size_t clen;
    char* cbuf = hub_gitwrite_commit_build(
        merged.b, (const uint8_t(*)[20])parents, 2, who, who, message, &clen);
    if(!cbuf) return HUB_GIT_ERR_NOMEM;
    HubOid cid;
    rc = map_wr(hub_gitwrite_object(io, repo_dir, HUB_GW_COMMIT, cbuf, clen, cid.b, NULL));
    free(cbuf);
    if(rc == HUB_GIT_OK && out_commit) *out_commit = cid;
    return rc;
}
