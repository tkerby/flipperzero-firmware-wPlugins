/* Object reader: commit, tag and tree parsing.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

/* ---------- parsing ---------- */

static bool parse_ident(const char* s, size_t len, HubGitIdent* id) {
    memset(id, 0, sizeof(*id));
    const char* end = s + len;
    const char* lt = memchr(s, '<', len);
    if(!lt) {
        id->name = s;
        id->name_len = len;
        id->email = s;
        strcpy(id->tz, "+0000");
        return true;
    }
    const char* gt = memchr(lt, '>', (size_t)(end - lt));
    id->name = s;
    id->name_len = (size_t)(lt - s);
    while(id->name_len > 0 && id->name[id->name_len - 1] == ' ')
        id->name_len--;
    id->email = lt + 1;
    id->email_len = gt ? (size_t)(gt - lt - 1) : (size_t)(end - lt - 1);
    strcpy(id->tz, "+0000");
    if(!gt) return true;
    const char* p = gt + 1;
    while(p < end && *p == ' ')
        p++;
    int64_t t = 0;
    while(p < end && *p >= '0' && *p <= '9')
        t = t * 10 + (*p++ - '0');
    id->time = t;
    while(p < end && *p == ' ')
        p++;
    size_t k = 0;
    while(p < end && k < sizeof(id->tz) - 1 && *p != ' ')
        id->tz[k++] = *p++;
    if(k) id->tz[k] = '\0';
    return true;
}

/* Walk the header lines. For each header calls cb(key, keylen, value, valuelen).
 * Returns the message start. */
typedef void (*HdrCb)(void* ctx, const char* k, size_t kl, const char* v, size_t vl);

static const char*
    parse_headers(const uint8_t* data, size_t len, HdrCb cb, void* ctx, const char** msg_end) {
    const char* p = (const char*)data;
    const char* end = p + len;
    while(p < end) {
        const char* nl = memchr(p, '\n', (size_t)(end - p));
        const char* le = nl ? nl : end;
        if(le == p) { /* empty line: message follows */
            p = nl ? nl + 1 : end;
            break;
        }
        if(*p != ' ') {
            const char* sp = memchr(p, ' ', (size_t)(le - p));
            if(sp) cb(ctx, p, (size_t)(sp - p), sp + 1, (size_t)(le - sp - 1));
        }
        p = nl ? nl + 1 : end;
    }
    *msg_end = end;
    return p;
}

typedef struct {
    HubGitCommit* c;
    bool has_tree;
} CommitCtx;

static void commit_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    CommitCtx* cc = ctx;
    HubGitCommit* c = cc->c;
    char hex[41];
    if(kl == 4 && memcmp(k, "tree", 4) == 0 && vl == 40) {
        memcpy(hex, v, 40);
        hex[40] = '\0';
        cc->has_tree = hub_oid_from_hex(hex, &c->tree);
    } else if(kl == 6 && memcmp(k, "parent", 6) == 0 && vl == 40) {
        if(c->nparents < HUB_GIT_MAX_PARENTS) {
            memcpy(hex, v, 40);
            hex[40] = '\0';
            if(hub_oid_from_hex(hex, &c->parent[c->nparents])) c->nparents++;
        } else {
            c->nparents++;
        }
    } else if(kl == 6 && memcmp(k, "author", 6) == 0) {
        parse_ident(v, vl, &c->author);
    } else if(kl == 9 && memcmp(k, "committer", 9) == 0) {
        parse_ident(v, vl, &c->committer);
    }
}

int hub_git_parse_commit(const uint8_t* data, size_t len, HubGitCommit* c) {
    memset(c, 0, sizeof(*c));
    strcpy(c->author.tz, "+0000");
    strcpy(c->committer.tz, "+0000");
    CommitCtx cc = {c, false};
    const char* end;
    const char* m = parse_headers(data, len, commit_hdr, &cc, &end);
    if(!cc.has_tree) return HUB_GIT_ERR_CORRUPT;
    c->msg = m;
    c->msg_len = (size_t)(end - m);
    return HUB_GIT_OK;
}

typedef struct {
    HubGitTag* t;
    bool has_obj;
} TagCtx;

static void tag_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    TagCtx* tc = ctx;
    HubGitTag* t = tc->t;
    char hex[41];
    if(kl == 6 && memcmp(k, "object", 6) == 0 && vl == 40) {
        memcpy(hex, v, 40);
        hex[40] = '\0';
        tc->has_obj = hub_oid_from_hex(hex, &t->object);
    } else if(kl == 4 && memcmp(k, "type", 4) == 0) {
        t->type = (uint8_t)type_from_name(v, vl);
    } else if(kl == 3 && memcmp(k, "tag", 3) == 0) {
        t->tag = v;
        t->tag_len = vl;
    } else if(kl == 6 && memcmp(k, "tagger", 6) == 0) {
        parse_ident(v, vl, &t->tagger);
        t->has_tagger = true;
    }
}

int hub_git_parse_tag(const uint8_t* data, size_t len, HubGitTag* t) {
    memset(t, 0, sizeof(*t));
    TagCtx tc = {t, false};
    const char* end;
    const char* m = parse_headers(data, len, tag_hdr, &tc, &end);
    if(!tc.has_obj) return HUB_GIT_ERR_CORRUPT;
    t->msg = m;
    t->msg_len = (size_t)(end - m);
    return HUB_GIT_OK;
}

void hub_tree_iter_init(HubTreeIter* it, const uint8_t* data, size_t len) {
    it->p = data;
    it->end = data + len;
}

int hub_tree_next(HubTreeIter* it, HubTreeEntry* e) {
    const uint8_t* p = it->p;
    if(p >= it->end) return 0;
    uint32_t mode = 0;
    const uint8_t* q = p;
    while(q < it->end && *q != ' ') {
        if(*q < '0' || *q > '7' || mode > 0x1FFFFFFu) return HUB_GIT_ERR_CORRUPT;
        mode = mode * 8 + (*q - '0');
        q++;
    }
    if(q == p || q >= it->end) return HUB_GIT_ERR_CORRUPT;
    q++;
    const uint8_t* name = q;
    while(q < it->end && *q != '\0')
        q++;
    if(q >= it->end || q == name || (size_t)(it->end - q) < 21) return HUB_GIT_ERR_CORRUPT;
    e->mode = mode;
    e->name = (const char*)name;
    e->name_len = (size_t)(q - name);
    e->oid = (const HubOid*)(q + 1);
    it->p = q + 21;
    return 1;
}

const char* hub_tree_mode_type(uint32_t mode) {
    switch(mode & 0170000) {
    case 0040000:
        return "tree";
    case 0120000:
        return "link";
    case 0160000:
        return "commit";
    default:
        return "blob";
    }
}

int hub_git_tree_find(HubGit* g, const HubOid* tree, const char* path, HubOid* oid, uint32_t* mode) {
    HubOid cur = *tree;
    uint32_t cur_mode = 0040000;
    const char* p = path;
    for(;;) {
        while(*p == '/')
            p++;
        if(*p == '\0') break;
        const char* e = p;
        while(*e && *e != '/')
            e++;
        size_t cl = (size_t)(e - p);
        if((cl == 1 && p[0] == '.') || (cl == 2 && p[0] == '.' && p[1] == '.'))
            return HUB_GIT_ERR_BADARG;
        if((cur_mode & 0170000) != 0040000) return HUB_GIT_ERR_NOTFOUND;

        uint8_t type;
        uint8_t* data;
        size_t size;
        int r = hub_git_read(g, &cur, &type, &data, &size);
        if(r) return r;
        if(type != HUB_OBJ_TREE) {
            free(data);
            return HUB_GIT_ERR_CORRUPT;
        }
        HubTreeIter it;
        HubTreeEntry te;
        hub_tree_iter_init(&it, data, size);
        bool found = false;
        while((r = hub_tree_next(&it, &te)) > 0) {
            if(te.name_len == cl && memcmp(te.name, p, cl) == 0) {
                cur = *te.oid;
                cur_mode = te.mode;
                found = true;
                break;
            }
        }
        free(data);
        if(r < 0) return r;
        if(!found) return HUB_GIT_ERR_NOTFOUND;
        p = e;
    }
    *oid = cur;
    if(mode) *mode = cur_mode;
    return HUB_GIT_OK;
}
