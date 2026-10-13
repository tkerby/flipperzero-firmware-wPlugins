/* gitobj_test part: trees and commits. Included by ../gitobj_test.c (one translation unit). */
/* ---------- trees, commits, refs ---------- */

typedef struct {
    char* v;
    size_t n, cap;
} Str;
static void sadd(Str* s, const char* p, size_t n) {
    if(s->n + n + 1 > s->cap) {
        s->cap = (s->n + n + 1) * 2;
        s->v = realloc(s->v, s->cap);
    }
    memcpy(s->v + s->n, p, n);
    s->n += n;
    s->v[s->n] = 0;
}

/* Recursive in the harness only: the library iterates without recursion. */
static void walk_tree(HubGit* g, const HubOid* tree, const char* prefix, Str* out) {
    uint8_t type;
    uint8_t* data;
    size_t size;
    int e = hub_git_read(g, tree, &type, &data, &size);
    if(e) {
        fail("walk_tree read: %s", errname(e));
        return;
    }
    HubTreeIter it;
    HubTreeEntry te;
    hub_tree_iter_init(&it, data, size);
    while((e = hub_tree_next(&it, &te)) > 0) {
        char hex[41], line[1500], path[1100];
        hexstr(te.oid, hex);
        snprintf(path, sizeof path, "%s%s%.*s", prefix, prefix[0] ? "/" : "", (int)te.name_len, te.name);
        const char* ty = hub_tree_mode_type(te.mode);
        if(!strcmp(ty, "link")) ty = "blob";
        int n = snprintf(line, sizeof line, "%06o %s %s\t%s", te.mode, ty, hex, path);
        sadd(out, line, (size_t)n + 1); /* include NUL like ls-tree -z */
        if(te.mode == 040000) walk_tree(g, te.oid, path, out);
    }
    CHECK(e == 0, "tree iteration error %d", e);
    hub_git_free(data);
}

static void test_trees(HubGit* g) {
    char* revs = git_out("rev-list --all");
    unsigned idx = 0, tested = 0;
    HubOid root_head = {{0}};
    for(char* c = strtok(revs, "\n"); c; c = strtok(NULL, "\n"), idx++) {
        if(idx % 23 != 0 && idx != 1) continue;
        HubOid co, tree;
        hub_oid_from_hex(c, &co);
        int e = hub_git_tree_of(g, &co, &tree);
        CHECK(e == 0, "tree_of %s: %s", c, errname(e));
        if(e) continue;
        char expect_tree[64];
        char* t = git_out("rev-parse %s^{tree}", c);
        t[40] = 0;
        hexstr(&tree, expect_tree);
        CHECK(strcmp(t, expect_tree) == 0, "tree_of mismatch %s", c);
        free(t);

        Str mine = {0};
        heap_mark();
        walk_tree(g, &tree, "", &mine);
        size_t exp_n;
        char* exp = slurp(gitp("ls-tree -r -t -z %s", c), &exp_n);
        CHECK(mine.n == exp_n && memcmp(mine.v, exp, exp_n) == 0, "ls-tree -r -t mismatch for commit %s (mine %zu, git %zu)", c, mine.n, exp_n);
        if(idx == 0) {
            root_head = tree;
            /* tree_find for every path */
            for(char* p = exp; p < exp + exp_n;) {
                char mode[8], ty[16], hex[48];
                char* tab = strchr(p, '\t');
                sscanf(p, "%7s %15s %47s", mode, ty, hex);
                const char* path = tab + 1;
                HubOid o, want;
                uint32_t m = 0;
                hub_oid_from_hex(hex, &want);
                heap_mark();
                e = hub_git_tree_find(g, &root_head, path, &o, &m);
                heap_note("tree_find(deep)", heap_used(), 0);
                CHECK(e == 0 && memcmp(&o, &want, 20) == 0 && m == strtoul(mode, NULL, 8), "tree_find '%s': %s", path, errname(e));
                p = (char*)path + strlen(path) + 1;
            }
            HubOid o;
            uint32_t m;
            CHECK(hub_git_tree_find(g, &root_head, "", &o, &m) == 0 && !memcmp(&o, &root_head, 20) && m == 040000, "tree_find root");
            CHECK(hub_git_tree_find(g, &root_head, "nope", &o, &m) == HUB_GIT_ERR_NOTFOUND, "tree_find missing");
            CHECK(hub_git_tree_find(g, &root_head, "README.md/x", &o, &m) == HUB_GIT_ERR_NOTFOUND, "tree_find through blob");
            CHECK(hub_git_tree_find(g, &root_head, "src/../x", &o, &m) == HUB_GIT_ERR_BADARG, "tree_find ..");
            CHECK(hub_git_tree_find(g, &root_head, "/src//core/", &o, &m) == 0 && m == 040000, "tree_find odd slashes");
        }
        free(exp);
        free(mine.v);
        tested++;
    }
    free(revs);
    printf("trees: %u commits' full trees compared with git ls-tree -r -t\n", tested);
}

static void trim_nl(const char* s, size_t* n) {
    while(*n > 0 && (s[*n - 1] == '\n' || s[*n - 1] == ' ')) (*n)--;
}

static void test_commits(HubGit* g) {
    size_t n;
    char* log = slurp(gitp("log --all --format='%%H%%x01%%T%%x01%%P%%x01%%an%%x01%%ae%%x01%%at%%x01%%cn%%x01%%ce%%x01%%ct%%x01%%B%%x02' --date=raw"), &n);
    char* tzs = git_out("log --all --format='%%ad%%x01%%cd' --date=format:%%z");
    unsigned cnt = 0, merges = 0, big_merge = 0;
    char* tzp = tzs;
    for(char* rec = strtok(log, "\x02"); rec; rec = strtok(NULL, "\x02")) {
        while(*rec == '\n') rec++;
        if(!*rec) break;
        char* f[10];
        char* p = rec;
        for(int k = 0; k < 10; k++) {
            f[k] = p;
            if(k < 9) {
                p = strchr(p, 1);
                if(!p) {
                    fail("log format");
                    return;
                }
                *p++ = 0;
            }
        }
        char* tz_a = tzp;
        char* tz_c = strchr(tz_a, 1);
        *tz_c++ = 0;
        char* nl = strchr(tz_c, '\n');
        *nl = 0;
        tzp = nl + 1;

        HubOid co;
        hub_oid_from_hex(f[0], &co);
        uint8_t type;
        uint8_t* data;
        size_t size;
        int e = hub_git_read(g, &co, &type, &data, &size);
        if(e) {
            fail("commit read %s: %s", f[0], errname(e));
            continue;
        }
        HubGitCommit c;
        e = hub_git_parse_commit(data, size, &c);
        CHECK(e == 0, "parse_commit %s", f[0]);
        char hex[41];
        hexstr(&c.tree, hex);
        CHECK(strcmp(hex, f[1]) == 0, "commit tree %s", f[0]);
        unsigned np = 0;
        for(char* q = f[2]; *q;) {
            if(np < HUB_GIT_MAX_PARENTS) {
                hexstr(&c.parent[np], hex);
                CHECK(strncmp(hex, q, 40) == 0, "commit parent %u of %s", np, f[0]);
            }
            np++;
            q += 40;
            if(*q == ' ') q++;
        }
        CHECK(c.nparents == np, "nparents %u/%u for %s", c.nparents, np, f[0]);
        if(np > 1) merges++;
        if(np > HUB_GIT_MAX_PARENTS) big_merge++;
        CHECK(c.author.name_len == strlen(f[3]) && !memcmp(c.author.name, f[3], c.author.name_len), "author name %s", f[0]);
        CHECK(c.author.email_len == strlen(f[4]) && !memcmp(c.author.email, f[4], c.author.email_len), "author email %s", f[0]);
        CHECK(c.author.time == atoll(f[5]) && strcmp(c.author.tz, tz_a) == 0, "author time/tz %s: %lld %s vs %s %s", f[0], (long long)c.author.time, c.author.tz, f[5], tz_a);
        CHECK(c.committer.name_len == strlen(f[6]) && !memcmp(c.committer.name, f[6], c.committer.name_len), "committer name %s", f[0]);
        CHECK(c.committer.email_len == strlen(f[7]) && !memcmp(c.committer.email, f[7], c.committer.email_len), "committer email %s", f[0]);
        CHECK(c.committer.time == atoll(f[8]) && strcmp(c.committer.tz, tz_c) == 0, "committer time/tz %s", f[0]);
        size_t ml = c.msg_len, el = strlen(f[9]);
        trim_nl(c.msg, &ml);
        trim_nl(f[9], &el);
        CHECK(ml == el && memcmp(c.msg, f[9], ml) == 0, "commit message %s", f[0]);
        hub_git_free(data);
        cnt++;
    }
    printf("commits: %u parsed and compared with git log (%u merges, %u with > %d parents)\n", cnt, merges, big_merge, HUB_GIT_MAX_PARENTS);
    CHECK(cnt >= 200, "fewer than 200 commits (%u)", cnt);
    free(log);
    free(tzs);
}

