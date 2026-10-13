#include "hub_gitwrite.h"
#include "hub_sha1.h"

#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

#define OBUF_SIZE 512
#define BLOCK_MAX 65535u

struct HubGitWriter {
    const HubGitWriteIo* io;
    void* file;
    HubSha1 sha;
    uint32_t adler_a, adler_b;
    uint64_t total; /* header + content bytes the deflate stream carries */
    uint64_t fed; /* of those, taken so far */
    uint32_t block_left;
    size_t size; /* content size given at begin */
    bool failed;
    uint16_t on; /* bytes in obuf */
    uint8_t obuf[OBUF_SIZE];
    char repo[HUB_GW_REPO_MAX + 1];
    char tmp[HUB_GW_REPO_MAX + 40];
    char fin[HUB_GW_REPO_MAX + 60];
};

static const char* type_name(int type) {
    switch(type) {
    case HUB_GW_COMMIT:
        return "commit";
    case HUB_GW_TREE:
        return "tree";
    case HUB_GW_BLOB:
        return "blob";
    case HUB_GW_TAG:
        return "tag";
    default:
        return NULL;
    }
}

/* "<type> <size>\0"; returns the length, 0 on a bad type */
static size_t make_header(int type, size_t size, char* out /* >= 32 */) {
    const char* t = type_name(type);
    if(!t) return 0;
    size_t n = strlen(t);
    memcpy(out, t, n);
    out[n++] = ' ';
    char dig[24];
    int d = 0;
    uint64_t v = size;
    do {
        dig[d++] = (char)('0' + (int)(v % 10));
        v /= 10;
    } while(v);
    while(d)
        out[n++] = dig[--d];
    out[n++] = '\0';
    return n;
}

void hub_gitwrite_hash(int type, const void* data, size_t len, uint8_t id[20]) {
    char hdr[32];
    size_t hn = make_header(type, len, hdr);
    HubSha1 s;
    hub_sha1_init(&s);
    hub_sha1_update(&s, hdr, hn);
    hub_sha1_update(&s, data, len);
    hub_sha1_final(&s, id);
}

void hub_gitwrite_hex(const uint8_t id[20], char out[41]) {
    static const char hex[] = "0123456789abcdef";
    for(int i = 0; i < 20; i++) {
        out[2 * i] = hex[id[i] >> 4];
        out[2 * i + 1] = hex[id[i] & 15];
    }
    out[40] = '\0';
}

bool hub_gitwrite_unhex(const char* hex, uint8_t id[20]) {
    for(int i = 0; i < 40; i++) {
        char c = hex[i];
        int v;
        if(c >= '0' && c <= '9') {
            v = c - '0';
        } else if(c >= 'a' && c <= 'f') {
            v = c - 'a' + 10;
        } else if(c >= 'A' && c <= 'F') {
            v = c - 'A' + 10;
        } else {
            return false;
        }
        if(i & 1) {
            id[i / 2] |= (uint8_t)v;
        } else {
            id[i / 2] = (uint8_t)(v << 4);
        }
    }
    return hex[40] == '\0';
}

/* ---------- output (buffered) ---------- */

static void out_flush(HubGitWriter* w) {
    if(w->on > 0) {
        if(!w->failed && !w->io->write(w->io->ctx, w->file, w->obuf, w->on)) w->failed = true;
        w->on = 0;
    }
}

static void out_put(HubGitWriter* w, const void* data, size_t n) {
    const uint8_t* p = data;
    if(n >= OBUF_SIZE) {
        out_flush(w);
        if(!w->failed && !w->io->write(w->io->ctx, w->file, p, n)) w->failed = true;
        return;
    }
    if(w->on + n > OBUF_SIZE) out_flush(w);
    memcpy(w->obuf + w->on, p, n);
    w->on = (uint16_t)(w->on + n);
}

/* ---------- stream ---------- */

static void adler_update(HubGitWriter* w, const uint8_t* p, size_t n) {
    uint32_t a = w->adler_a, b = w->adler_b;
    while(n > 0) {
        size_t k = n < 5552 ? n : 5552;
        n -= k;
        while(k--) {
            a += *p++;
            b += a;
        }
        a %= 65521;
        b %= 65521;
    }
    w->adler_a = a;
    w->adler_b = b;
}

/* Take bytes of the deflate payload (header first, then content). */
static bool feed(HubGitWriter* w, const uint8_t* p, size_t n) {
    if(w->failed || w->fed + n > w->total) {
        w->failed = true;
        return false;
    }
    hub_sha1_update(&w->sha, p, n);
    adler_update(w, p, n);
    while(n > 0) {
        if(w->block_left == 0) {
            uint64_t remain = w->total - w->fed; /* payload bytes not written yet */
            uint32_t blen = remain > BLOCK_MAX ? BLOCK_MAX : (uint32_t)remain;
            uint8_t bh[5] = {
                (uint8_t)(remain == blen ? 1 : 0), /* BFINAL, BTYPE = 00 (stored) */
                (uint8_t)(blen & 0xff),
                (uint8_t)(blen >> 8),
                (uint8_t)(~blen & 0xff),
                (uint8_t)((~blen >> 8) & 0xff)};
            out_put(w, bh, 5);
            w->block_left = blen;
        }
        size_t take = n < w->block_left ? n : w->block_left;
        out_put(w, p, take);
        w->block_left -= (uint32_t)take;
        w->fed += take;
        p += take;
        n -= take;
    }
    return !w->failed;
}

/* ---------- writer ---------- */

static uint32_t tmp_counter;

HubGitWriter*
    hub_gitwrite_begin(const HubGitWriteIo* io, const char* repo_dir, int type, size_t size) {
    char hdr[32];
    size_t hn = make_header(type, size, hdr);
    size_t rl = repo_dir ? strlen(repo_dir) : 0;
    if(hn == 0 || rl == 0 || rl > HUB_GW_REPO_MAX) return NULL;
    HubGitWriter* w = malloc(sizeof(HubGitWriter));
    if(!w) return NULL;
    memset(w, 0, sizeof(HubGitWriter));
    w->io = io;
    w->size = size;
    memcpy(w->repo, repo_dir, rl + 1);
    while(rl > 1 && w->repo[rl - 1] == '/')
        w->repo[--rl] = '\0';

    uint32_t c = ++tmp_counter;
    char num[9];
    for(int i = 0; i < 8; i++)
        num[i] = "0123456789abcdef"[(c >> (28 - 4 * i)) & 15];
    num[8] = '\0';
    memcpy(w->tmp, w->repo, rl);
    static const char tmp_prefix[] = "/objects/tmp_obj_";
    memcpy(w->tmp + rl, tmp_prefix, sizeof(tmp_prefix) - 1);
    memcpy(w->tmp + rl + sizeof(tmp_prefix) - 1, num, sizeof(num));

    w->file = io->open(io->ctx, w->tmp);
    if(!w->file) {
        free(w);
        return NULL;
    }
    hub_sha1_init(&w->sha);
    w->adler_a = 1;
    w->adler_b = 0;
    w->total = (uint64_t)hn + size;
    static const uint8_t zhdr[2] = {
        0x08, 0x1d}; /* deflate, CINFO 0 (stored blocks need no window), no preset dict */
    out_put(w, zhdr, 2);
    feed(w, (const uint8_t*)hdr, hn);
    return w;
}

bool hub_gitwrite_update(HubGitWriter* w, const void* data, size_t len) {
    return feed(w, data, len);
}

void hub_gitwrite_abort(HubGitWriter* w) {
    if(!w) return;
    w->io->close(w->io->ctx, w->file);
    w->io->remove(w->io->ctx, w->tmp);
    free(w);
}

int hub_gitwrite_end(HubGitWriter* w, uint8_t id[20], bool* existed) {
    int rc = HUB_GW_OK;
    if(existed) *existed = false;
    if(w->fed != w->total) {
        hub_gitwrite_abort(w);
        return HUB_GW_ERR_SIZE;
    }
    uint32_t ad = (w->adler_b << 16) | w->adler_a;
    uint8_t tail[4] = {(uint8_t)(ad >> 24), (uint8_t)(ad >> 16), (uint8_t)(ad >> 8), (uint8_t)ad};
    out_put(w, tail, 4);
    out_flush(w);
    bool closed = w->io->close(w->io->ctx, w->file);
    if(w->failed || !closed) {
        w->io->remove(w->io->ctx, w->tmp);
        free(w);
        return HUB_GW_ERR_IO;
    }

    uint8_t oid[20];
    hub_sha1_final(&w->sha, oid);
    char hex[41];
    hub_gitwrite_hex(oid, hex);
    size_t rl = strlen(w->repo);
    memcpy(w->fin, w->repo, rl);
    strcpy(w->fin + rl, "/objects/");
    size_t dl = strlen(w->fin);
    w->fin[dl] = hex[0];
    w->fin[dl + 1] = hex[1];
    w->fin[dl + 2] = '\0';

    w->fin[dl + 2] = '\0';
    if(!w->io->mkdir(w->io->ctx, w->fin)) {
        rc = HUB_GW_ERR_IO;
    } else {
        w->fin[dl + 2] = '/';
        memcpy(w->fin + dl + 3, hex + 2, 39);
        if(w->io->exists(w->io->ctx, w->fin)) {
            if(existed) *existed = true;
        } else if(!w->io->rename(w->io->ctx, w->tmp, w->fin)) {
            /* somebody else may have won the race */
            if(w->io->exists(w->io->ctx, w->fin)) {
                if(existed) *existed = true;
            } else {
                rc = HUB_GW_ERR_IO;
            }
        } else {
            w->tmp[0] = '\0'; /* moved: nothing to remove */
        }
    }
    if(w->tmp[0]) w->io->remove(w->io->ctx, w->tmp);
    if(rc == HUB_GW_OK) memcpy(id, oid, 20);
    free(w);
    return rc;
}

/* Object path of an id: "<repo>/objects/xx/<38>" into out (>= repo + 52). */
static void object_path(const char* repo, const char* hex, char* out) {
    size_t rl = strlen(repo);
    while(rl > 1 && repo[rl - 1] == '/')
        rl--;
    memcpy(out, repo, rl);
    memcpy(out + rl, "/objects/", 9);
    size_t dl = rl + 9;
    out[dl] = hex[0];
    out[dl + 1] = hex[1];
    out[dl + 2] = '/';
    memcpy(out + dl + 3, hex + 2, 39);
}

int hub_gitwrite_object(
    const HubGitWriteIo* io,
    const char* repo_dir,
    int type,
    const void* data,
    size_t len,
    uint8_t id[20],
    bool* existed) {
    if(existed) *existed = false;
    if(!repo_dir || strlen(repo_dir) > HUB_GW_REPO_MAX || !type_name(type)) {
        return HUB_GW_ERR_BADARG;
    }
    /* hash first: an object that exists is not written again */
    uint8_t oid[20];
    char hex[41];
    char* path = malloc(HUB_GW_REPO_MAX + 60); /* keep the stack small */
    if(!path) return HUB_GW_ERR_NOMEM;
    hub_gitwrite_hash(type, data, len, oid);
    hub_gitwrite_hex(oid, hex);
    object_path(repo_dir, hex, path);
    bool have = io->exists(io->ctx, path);
    free(path);
    if(have) {
        memcpy(id, oid, 20);
        if(existed) *existed = true;
        return HUB_GW_OK;
    }
    HubGitWriter* w = hub_gitwrite_begin(io, repo_dir, type, len);
    if(!w) return HUB_GW_ERR_IO;
    if(!hub_gitwrite_update(w, data, len)) {
        hub_gitwrite_abort(w);
        return HUB_GW_ERR_IO;
    }
    return hub_gitwrite_end(w, id, existed);
}

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
