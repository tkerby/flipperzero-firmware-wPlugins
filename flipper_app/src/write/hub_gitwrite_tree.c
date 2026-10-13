/* Object writer: tree entries and order, tree and commit objects.
 * Part of hub_gitwrite.c: included there, in this order, not built alone. */
#include "hub_gitwrite_int.h"

/* ---------- tree ---------- */

uint32_t hub_gitwrite_mode_parse(const char* s) {
    uint32_t m = 0;
    if(!s || !*s) return 0;
    for(; *s; s++) {
        if(*s < '0' || *s > '7') return 0;
        m = (m << 3) | (uint32_t)(*s - '0');
        if(m > 0177777) return 0;
    }
    switch(m) {
    case HUB_GW_MODE_TREE:
    case HUB_GW_MODE_FILE:
    case HUB_GW_MODE_EXEC:
    case HUB_GW_MODE_LINK:
    case HUB_GW_MODE_COMMIT:
        return m;
    default:
        return 0;
    }
}

static bool ent_is_tree(const HubGitTreeEnt* e) {
    return e->mode == HUB_GW_MODE_TREE;
}

/* git's base_name_compare: a tree name counts as if it ended with "/" */
int hub_gitwrite_ent_cmp(
    const char* an,
    size_t al,
    bool a_tree,
    const char* bn,
    size_t bl,
    bool b_tree) {
    size_t m = al < bl ? al : bl;
    int c = memcmp(an, bn, m);
    if(c != 0) return c < 0 ? -1 : 1;
    unsigned c1 = al > m ? (unsigned char)an[m] : (a_tree ? '/' : 0);
    unsigned c2 = bl > m ? (unsigned char)bn[m] : (b_tree ? '/' : 0);
    if(c1 == c2) return 0;
    return c1 < c2 ? -1 : 1;
}

static int ent_cmp(const HubGitTreeEnt* a, const HubGitTreeEnt* b) {
    return hub_gitwrite_ent_cmp(
        a->name, strlen(a->name), ent_is_tree(a), b->name, strlen(b->name), ent_is_tree(b));
}

static void sift_down(HubGitTreeEnt* a, size_t root, size_t end) {
    for(;;) {
        size_t child = 2 * root + 1;
        if(child >= end) return;
        if(child + 1 < end && ent_cmp(&a[child], &a[child + 1]) < 0) child++;
        if(ent_cmp(&a[root], &a[child]) >= 0) return;
        HubGitTreeEnt t = a[root];
        a[root] = a[child];
        a[child] = t;
        root = child;
    }
}

static void tree_sort(HubGitTreeEnt* a, size_t n) {
    if(n < 2) return;
    for(size_t i = n / 2; i-- > 0;)
        sift_down(a, i, n);
    for(size_t end = n - 1; end > 0; end--) {
        HubGitTreeEnt t = a[0];
        a[0] = a[end];
        a[end] = t;
        sift_down(a, 0, end);
    }
}

static bool ent_name_ok(const char* n) {
    if(!n || !*n) return false;
    if(strchr(n, '/')) return false;
    if(strcmp(n, ".") == 0 || strcmp(n, "..") == 0) return false;
    if(n[0] == '.' && (n[1] | 0x20) == 'g' && (n[2] | 0x20) == 'i' && (n[3] | 0x20) == 't' &&
       n[4] == '\0')
        return false;
    return true;
}

static size_t put_mode(uint8_t* out, uint32_t mode) {
    char tmp[12];
    int d = 0;
    do {
        tmp[d++] = (char)('0' + (mode & 7));
        mode >>= 3;
    } while(mode);
    size_t n = 0;
    while(d)
        out[n++] = (uint8_t)tmp[--d];
    return n;
}

size_t hub_gitwrite_ent_put(
    uint8_t* out,
    uint32_t mode,
    const char* name,
    size_t nlen,
    const uint8_t id[20]) {
    size_t pos = put_mode(out, mode);
    out[pos++] = ' ';
    memcpy(out + pos, name, nlen);
    pos += nlen;
    out[pos++] = '\0';
    memcpy(out + pos, id, 20);
    return pos + 20;
}

uint8_t* hub_gitwrite_tree_build(HubGitTreeEnt* ents, size_t n, size_t* len) {
    size_t cap = 0;
    for(size_t i = 0; i < n; i++) {
        if(!ent_name_ok(ents[i].name)) return NULL;
        uint32_t m = ents[i].mode;
        if(m != HUB_GW_MODE_TREE && m != HUB_GW_MODE_FILE && m != HUB_GW_MODE_EXEC &&
           m != HUB_GW_MODE_LINK && m != HUB_GW_MODE_COMMIT)
            return NULL;
        cap += 8 + strlen(ents[i].name) + 20; /* "100644 " + name + NUL + id */
    }
    tree_sort(ents, n);
    /* equal names can sit apart when a file and a tree share a name
     * ("a", "a.b", "a/"); names with the same prefix are contiguous */
    for(size_t i = 0; i < n; i++) {
        size_t nl = strlen(ents[i].name);
        for(size_t j = i + 1; j < n && strncmp(ents[j].name, ents[i].name, nl) == 0; j++) {
            if(ents[j].name[nl] == '\0') return NULL;
        }
    }
    uint8_t* buf = malloc(cap ? cap : 1);
    if(!buf) return NULL;
    size_t pos = 0;
    for(size_t i = 0; i < n; i++) {
        pos += put_mode(buf + pos, ents[i].mode);
        buf[pos++] = ' ';
        size_t nl = strlen(ents[i].name);
        memcpy(buf + pos, ents[i].name, nl + 1);
        pos += nl + 1;
        memcpy(buf + pos, ents[i].id, 20);
        pos += 20;
    }
    *len = pos;
    return buf;
}

/* ---------- commit ---------- */

/* Copy s without "<", ">" and line breaks; trim blanks at both ends. */
static size_t clean_copy(char* out, const char* s) {
    size_t n = 0;
    for(; *s; s++) {
        if(*s == '<' || *s == '>' || *s == '\n' || *s == '\r') continue;
        out[n++] = *s;
    }
    while(n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
        n--;
    size_t skip = 0;
    while(skip < n && (out[skip] == ' ' || out[skip] == '\t'))
        skip++;
    if(skip) memmove(out, out + skip, n - skip);
    return n - skip;
}

static bool tz_ok(const char* tz) {
    if(!tz) return true;
    if(strlen(tz) != 5 || (tz[0] != '+' && tz[0] != '-')) return false;
    for(int i = 1; i < 5; i++) {
        if(tz[i] < '0' || tz[i] > '9') return false;
    }
    return true;
}

static size_t put_u64(char* out, uint64_t v) {
    char tmp[24];
    int d = 0;
    do {
        tmp[d++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while(v);
    size_t n = 0;
    while(d)
        out[n++] = tmp[--d];
    return n;
}

/* "<tag> Name <email> <time> <tz>\n" */
static size_t put_ident(char* out, const char* tag, const HubGwIdent* id) {
    size_t n = strlen(tag);
    memcpy(out, tag, n);
    out[n++] = ' ';
    n += clean_copy(out + n, id->name);
    out[n++] = ' ';
    out[n++] = '<';
    n += clean_copy(out + n, id->email);
    out[n++] = '>';
    out[n++] = ' ';
    n += put_u64(out + n, (uint64_t)id->time);
    out[n++] = ' ';
    const char* tz = id->tz ? id->tz : "+0000";
    memcpy(out + n, tz, 5);
    n += 5;
    out[n++] = '\n';
    return n;
}

static bool ident_ok(const HubGwIdent* id) {
    if(!id || !id->name || !id->email || id->time < 0 || !tz_ok(id->tz)) return false;
    /* the name must not be empty after cleaning */
    for(const char* p = id->name; *p; p++) {
        if(*p != ' ' && *p != '\t' && *p != '<' && *p != '>' && *p != '\n' && *p != '\r') {
            return true;
        }
    }
    return false;
}

char* hub_gitwrite_commit_build(
    const uint8_t tree[20],
    const uint8_t (*parents)[20],
    size_t nparents,
    const HubGwIdent* author,
    const HubGwIdent* committer,
    const char* message,
    size_t* len) {
    if(!tree || (nparents && !parents) || !ident_ok(author) || !ident_ok(committer)) return NULL;
    if(!message) message = "";
    size_t ml = strlen(message);
    size_t cap = 5 + 41 + nparents * (7 + 41) + 2 * (16 + 40 + 5 + 2) + strlen(author->name) +
                 strlen(author->email) + strlen(committer->name) + strlen(committer->email) + 4 +
                 ml + 2;
    char* buf = malloc(cap);
    if(!buf) return NULL;
    char hex[41];
    size_t pos = 0;
    memcpy(buf, "tree ", 5);
    pos = 5;
    hub_gitwrite_hex(tree, hex);
    memcpy(buf + pos, hex, 40);
    pos += 40;
    buf[pos++] = '\n';
    for(size_t i = 0; i < nparents; i++) {
        memcpy(buf + pos, "parent ", 7);
        pos += 7;
        hub_gitwrite_hex(parents[i], hex);
        memcpy(buf + pos, hex, 40);
        pos += 40;
        buf[pos++] = '\n';
    }
    pos += put_ident(buf + pos, "author", author);
    pos += put_ident(buf + pos, "committer", committer);
    buf[pos++] = '\n';
    memcpy(buf + pos, message, ml);
    pos += ml;
    if(ml > 0 && message[ml - 1] != '\n') buf[pos++] = '\n';
    *len = pos;
    return buf;
}
