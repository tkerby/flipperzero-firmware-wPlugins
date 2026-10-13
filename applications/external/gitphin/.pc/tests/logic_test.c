/* Host driver for hub_gitdiff.c and hub_gitcommit.c. run_logic_test.sh and
 * logic_check.py compare the output with git. All commands print the heap peak
 * of the operation (bytes allocated by the library code) on stderr.
 *   logic_test diff <repo> [max]            stdin: "<treeA|-> <treeB|->" per line
 *   logic_test compare <repo> <base> <head> [cap]
 *   logic_test cfile <repo> <tip|-> <path> put <file> <msgfile>
 *   logic_test cfile <repo> <tip|-> <path> del - <msgfile>
 *   logic_test merge <repo> <ours> <theirs> <base|-> <msgfile>
 */
#include "../../flipper_app/hub_gitdiff.h"
#include "../../flipper_app/hub_gitcommit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hub_gitwrite_io_stdio(HubGitWriteIo* io);

/* ---- heap accounting (the library files are built with malloc_wrap.h) ---- */
static size_t cur_bytes, peak_bytes;
typedef struct {
    size_t n;
    size_t pad[3];
} Hdr;

void* t_malloc(size_t n) {
    Hdr* h = malloc(sizeof(Hdr) + n);
    if(!h) return NULL;
    h->n = n;
    cur_bytes += n;
    if(cur_bytes > peak_bytes) peak_bytes = cur_bytes;
    return h + 1;
}
void* t_calloc(size_t a, size_t b) {
    void* p = t_malloc(a * b);
    if(p) memset(p, 0, a * b);
    return p;
}
void t_free(void* p) {
    if(!p) return;
    Hdr* h = (Hdr*)p - 1;
    cur_bytes -= h->n;
    free(h);
}
void* t_realloc(void* p, size_t n) {
    if(!p) return t_malloc(n);
    Hdr* h = (Hdr*)p - 1;
    size_t old = h->n;
    Hdr* nh = realloc(h, sizeof(Hdr) + n);
    if(!nh) return NULL;
    nh->n = n;
    cur_bytes = cur_bytes - old + n;
    if(cur_bytes > peak_bytes) peak_bytes = cur_bytes;
    return nh + 1;
}

static void report_peak(void) {
    fprintf(stderr, "heap_peak=%zu\n", peak_bytes);
}

static bool parse_oid(const char* s, HubOid* o) {
    return hub_oid_from_hex(s, o);
}

/* ---- diff ---- */
static bool diff_cb(void* ctx, const HubGitDiffFile* f) {
    (void)ctx;
    char a[41], b[41];
    hub_oid_to_hex(&f->old_oid, a);
    hub_oid_to_hex(&f->new_oid, b);
    printf("%c %06o %06o %s %s %s\n", f->status, f->old_mode, f->new_mode, a, b, f->path);
    return true;
}

static int cmd_diff(HubGit* g, size_t max) {
    char line[256];
    while(fgets(line, sizeof(line), stdin)) {
        char sa[64], sb[64];
        if(sscanf(line, "%63s %63s", sa, sb) != 2) continue;
        HubOid a, b;
        bool ha = strcmp(sa, "-") != 0, hb = strcmp(sb, "-") != 0;
        if((ha && !parse_oid(sa, &a)) || (hb && !parse_oid(sb, &b))) return 2;
        printf("== %s %s\n", sa, sb);
        bool trunc = false;
        int rc = hub_git_diff_trees(g, ha ? &a : NULL, hb ? &b : NULL, diff_cb, NULL, max, &trunc);
        printf("end rc=%d truncated=%d\n", rc, trunc);
    }
    return 0;
}

extern unsigned long gitio_nopen, gitio_nread;
static void reads_line(const char* what) {
    fprintf(stderr, "%s opens=%lu reads=%lu\n", what, gitio_nopen, gitio_nread);
}

/* ---- compare ---- */
static int cmd_compare(HubGit* g, const char* sb, const char* sh, size_t cap) {
    HubOid b, h;
    if(!parse_oid(sb, &b) || !parse_oid(sh, &h)) return 2;
    HubGitCompare* c = calloc(1, sizeof(*c));
    c->cap = cap;
    int rc = hub_git_compare(g, &b, &h, c);
    printf("rc=%d\n", rc);
    if(rc == 0) {
        char x[41];
        if(c->has_merge_base) {
            hub_oid_to_hex(&c->merge_base, x);
            printf("merge_base=%s\n", x);
        } else {
            printf("merge_base=none\n");
        }
        printf("ff=%d capped=%d ahead_count=%u behind_count=%u n_ahead=%u\n", c->ff, c->capped,
               c->ahead_count, c->behind_count, c->n_ahead);
        for(unsigned i = 0; i < c->n_ahead; i++) {
            hub_oid_to_hex(&c->ahead[i], x);
            printf("ahead %s\n", x);
        }
    }
    reads_line("compare:");
    for(unsigned i = 0; i < c->n_ahead; i++) { /* the route reads each ahead commit for its JSON */
        uint8_t t, *d;
        size_t sz;
        if(hub_git_read(g, &c->ahead[i], &t, &d, &sz) == 0) hub_git_free(d);
    }
    reads_line("compare+ahead objects:");
    free(c);
    return 0;
}

/* ---- log: the route's first-parent walk (skip by meta, the shown commits by object) ---- */
static int cmd_log(HubGit* g, const char* ref, unsigned skip, unsigned n) {
    HubOid oid;
    if(hub_git_resolve(g, ref, &oid) != 0) return 2;
    HubCommitMeta m;
    bool end = false;
    for(unsigned i = 0; i < skip && !end; i++) {
        if(hub_git_commit_meta(g, &oid, &m) != 0 || m.nparents == 0) end = true;
        else oid = m.parent[0];
    }
    unsigned done = 0;
    bool more = false;
    while(!end && done < n) {
        uint8_t t, *d;
        size_t sz;
        HubGitCommit c;
        if(hub_git_read(g, &oid, &t, &d, &sz) != 0 || hub_git_parse_commit(d, sz, &c) != 0) break;
        char x[41];
        hub_oid_to_hex(&oid, x);
        printf("%s %lld\n", x, (long long)c.committer.time);
        done++;
        more = c.nparents > 0;
        if(more) oid = c.parent[0]; else end = true;
        hub_git_free(d);
    }
    printf("more=%d next_skip=%u\n", more, skip + done);
    reads_line("log:");
    return 0;
}

/* ---- blob: stream an object through hub_git_stream to stdout ---- */
extern unsigned long hub_far_reads, hub_far_bytes;
static bool blob_sink(void* ctx, const uint8_t* p, size_t n) {
    (void)ctx;
    return fwrite(p, 1, n, stdout) == n;
}
static int cmd_blob(HubGit* g, const char* hex) {
    HubOid o;
    if(!parse_oid(hex, &o)) return 2;
    uint8_t t;
    size_t sz = 0;
    int rc = hub_git_stream(g, &o, blob_sink, NULL, &t, &sz);
    fflush(stdout);
    fprintf(stderr, "blob: rc=%d size=%zu far_reads=%lu far_bytes=%lu opens=%lu reads=%lu\n", rc, sz, hub_far_reads,
            hub_far_bytes, gitio_nopen, gitio_nread);
    return rc ? 1 : 0;
}

/* ---- treewalk: tree ids on stdin -> "mode type sha<TAB>name" in git order ---- */
static bool tw_cb(void* ctx, uint32_t mode, const char* name, size_t nl, const HubOid* oid) {
    (void)ctx; (void)nl;
    char x[41];
    hub_oid_to_hex(oid, x);
    printf("%06o %s %s\t%s\n", mode, hub_tree_mode_type(mode)[0] == 't' ? "tree" : hub_tree_mode_type(mode)[0] == 'c' ? "commit" : "blob", x, name);
    return true;
}
static int cmd_treewalk(HubGit* g) {
    char line[128];
    while(fgets(line, sizeof line, stdin)) {
        HubOid o;
        line[strcspn(line, "\n")] = 0;
        if(!hub_oid_from_hex(line, &o)) continue;
        printf("== %s\n", line);
        printf("rc=%d\n", hub_git_tree_walk(g, &o, tw_cb, NULL));
    }
    return 0;
}

/* ---- meta: ids on stdin -> "id time nparents p1 p2 cached" ---- */
static int cmd_meta(HubGit* g) {
    char line[128];
    while(fgets(line, sizeof line, stdin)) {
        HubOid o;
        HubCommitMeta m;
        line[strcspn(line, "\n")] = 0;
        if(!hub_oid_from_hex(line, &o)) continue;
        int rc = hub_git_commit_meta(g, &o, &m);
        char a[41] = "-", b[41] = "-";
        if(rc == 0 && m.nparents > 0) hub_oid_to_hex(&m.parent[0], a);
        if(rc == 0 && m.nparents > 1) hub_oid_to_hex(&m.parent[1], b);
        printf("%s %u %u %s %s %d\n", line, m.time, m.nparents, a, b, m.cached);
    }
    reads_line("meta:");
    return 0;
}

/* ---- commit_file ---- */
static uint8_t* slurp(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if(!f) {
        perror(path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = malloc(n ? (size_t)n + 1 : 1);
    if(fread(b, 1, (size_t)n, f) != (size_t)n) exit(2);
    b[n] = 0;
    fclose(f);
    *len = (size_t)n;
    return b;
}

static int cmd_cfile(HubGit* g, const char* repo, char** a) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    HubOid tip;
    bool has_tip = strcmp(a[0], "-") != 0;
    if(has_tip && !parse_oid(a[0], &tip)) return 2;
    size_t mlen, clen = 0;
    char* msg = (char*)slurp(a[4], &mlen);
    uint8_t* content = NULL;
    bool put = strcmp(a[2], "put") == 0;
    if(put) content = slurp(a[3], &clen);
    HubGwIdent who = {"T Er", "t@example.com", 1700000000, "+0000"};
    HubOid commit, tree;
    int rc = hub_git_commit_file(g, &io, repo, has_tip ? &tip : NULL, a[1], content, clen, msg, &who, &commit, &tree);
    if(rc == 0) {
        char c[41], t[41];
        hub_oid_to_hex(&commit, c);
        hub_oid_to_hex(&tree, t);
        printf("commit %s tree %s\n", c, t);
    } else {
        printf("error %d\n", rc);
    }
    free(content);
    free(msg);
    return 0;
}

/* ---- merge ---- */
static bool conf_cb(void* ctx, const char* path) {
    (void)ctx;
    printf("conflict %s\n", path);
    return true;
}

static int cmd_merge(HubGit* g, const char* repo, char** a) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    HubOid ours, theirs, base;
    if(!parse_oid(a[0], &ours) || !parse_oid(a[1], &theirs)) return 2;
    bool hb = strcmp(a[2], "-") != 0;
    if(hb && !parse_oid(a[2], &base)) return 2;
    size_t mlen;
    char* msg = (char*)slurp(a[3], &mlen);
    HubGwIdent who = {"M Er", "m@example.com", 1700000000, "+0000"};
    HubOid commit, tree;
    unsigned nconf = 0;
    int rc = hub_git_merge_commits(g, &io, repo, &ours, &theirs, hb ? &base : NULL, msg, &who, &commit, &tree, conf_cb, NULL, &nconf);
    if(rc == 0) {
        char c[41], t[41];
        hub_oid_to_hex(&commit, c);
        hub_oid_to_hex(&tree, t);
        printf("commit %s tree %s\n", c, t);
    } else {
        printf("error %d nconf=%u\n", rc, nconf);
    }
    free(msg);
    return 0;
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "usage: see the top of logic_test.c\n");
        return 1;
    }
    HubGit* g = hub_git_open(NULL, argv[2]);
    if(!g) {
        fprintf(stderr, "cannot open %s\n", argv[2]);
        return 3;
    }
    if(getenv("LIMIT")) hub_git_set_limit(g, (size_t)atol(getenv("LIMIT")));
    size_t base_bytes = cur_bytes;
    (void)base_bytes;
    peak_bytes = cur_bytes; /* count only what the operation allocates on top of the handle */
    int rc = 1;
    if(strcmp(argv[1], "diff") == 0) {
        rc = cmd_diff(g, argc > 3 ? (size_t)atol(argv[3]) : 0);
    } else if(strcmp(argv[1], "compare") == 0 && argc >= 5) {
        rc = cmd_compare(g, argv[3], argv[4], argc > 5 ? (size_t)atol(argv[5]) : 0);
    } else if(strcmp(argv[1], "log") == 0 && argc >= 6) {
        rc = cmd_log(g, argv[3], (unsigned)atoi(argv[4]), (unsigned)atoi(argv[5]));
    } else if(strcmp(argv[1], "blob") == 0 && argc >= 4) {
        rc = cmd_blob(g, argv[3]);
    } else if(strcmp(argv[1], "treewalk") == 0) {
        rc = cmd_treewalk(g);
    } else if(strcmp(argv[1], "meta") == 0) {
        rc = cmd_meta(g);
    } else if(strcmp(argv[1], "cfile") == 0 && argc >= 8) {
        rc = cmd_cfile(g, argv[2], argv + 3);
    } else if(strcmp(argv[1], "merge") == 0 && argc >= 7) {
        rc = cmd_merge(g, argv[2], argv + 3);
    }
    report_peak();
    hub_git_close(g);
    return rc;
}
