/* Host test driver for hub_sha1.c / hub_gitwrite.c. The shell script
 * run_write_test.sh compares the results with git and sha1sum.
 *   gitwrite_test sha1 <file> <chunk>
 *   gitwrite_test blob <repo> <file> mem|stream <chunk>
 *   gitwrite_test tree <repo> <spec>      spec lines: <mode> <name> <40 hex>
 *   gitwrite_test commit <repo> <tree> <time> <tz> <msgfile> [parent...]
 *   gitwrite_test self <repo>
 */
#include "../../flipper_app/hub_gitwrite.h"
#include "../../flipper_app/hub_sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

void hub_gitwrite_io_stdio(HubGitWriteIo* io);

static uint8_t* slurp(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if(!f) {
        perror(path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = malloc(n ? (size_t)n : 1);
    if(fread(b, 1, (size_t)n, f) != (size_t)n) exit(2);
    fclose(f);
    *len = (size_t)n;
    return b;
}

static int cmd_sha1(const char* file, size_t chunk) {
    size_t n;
    uint8_t* d = slurp(file, &n);
    HubSha1 s;
    uint8_t out[20];
    char hex[41];
    hub_sha1_init(&s);
    for(size_t i = 0; i < n; i += chunk) hub_sha1_update(&s, d + i, n - i < chunk ? n - i : chunk);
    hub_sha1_final(&s, out);
    hub_gitwrite_hex(out, hex);
    printf("%s\n", hex);
    free(d);
    return 0;
}

static int cmd_blob(const char* repo, const char* file, bool stream, size_t chunk) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    size_t n;
    uint8_t* d = slurp(file, &n);
    uint8_t id[20];
    char hex[41];
    bool existed = false;
    int rc;
    if(!stream) {
        rc = hub_gitwrite_object(&io, repo, HUB_GW_BLOB, d, n, id, &existed);
    } else {
        HubGitWriter* w = hub_gitwrite_begin(&io, repo, HUB_GW_BLOB, n);
        if(!w) return 3;
        for(size_t i = 0; i < n; i += chunk) {
            if(!hub_gitwrite_update(w, d + i, n - i < chunk ? n - i : chunk)) return 4;
        }
        rc = hub_gitwrite_end(w, id, &existed);
    }
    free(d);
    if(rc != 0) {
        fprintf(stderr, "write failed: %d\n", rc);
        return 5;
    }
    hub_gitwrite_hex(id, hex);
    printf("%s %s\n", hex, existed ? "existed" : "new");
    return 0;
}

static int cmd_tree(const char* repo, const char* spec) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    FILE* f = fopen(spec, "r");
    if(!f) return 2;
    HubGitTreeEnt ents[256];
    char names[256][128];
    size_t n = 0;
    char line[512];
    while(fgets(line, sizeof(line), f)) {
        char mode[16], name[128], hexid[64];
        if(sscanf(line, "%15s %127s %63s", mode, name, hexid) != 3) continue;
        ents[n].mode = hub_gitwrite_mode_parse(mode);
        strcpy(names[n], name);
        ents[n].name = names[n];
        if(!hub_gitwrite_unhex(hexid, ents[n].id)) return 2;
        n++;
    }
    fclose(f);
    size_t len;
    uint8_t* t = hub_gitwrite_tree_build(ents, n, &len);
    if(!t) {
        printf("rejected\n");
        return 6;
    }
    uint8_t id[20];
    char hex[41];
    int rc = hub_gitwrite_object(&io, repo, HUB_GW_TREE, t, len, id, NULL);
    free(t);
    if(rc) return 5;
    hub_gitwrite_hex(id, hex);
    printf("%s\n", hex);
    return 0;
}

static int cmd_commit(const char* repo, int argc, char** argv) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    uint8_t tree[20];
    if(!hub_gitwrite_unhex(argv[0], tree)) return 2;
    HubGwIdent a = {"A U Thor", "author@example.com", atoll(argv[1]), argv[2]};
    HubGwIdent c = {"C O Mitter", "committer@example.com", atoll(argv[1]) + 5, argv[2]};
    size_t ml;
    uint8_t* msg = slurp(argv[3], &ml);
    char* m = malloc(ml + 1);
    memcpy(m, msg, ml);
    m[ml] = '\0';
    free(msg);
    uint8_t parents[8][20];
    int np = 0;
    for(int i = 4; i < argc && np < 8; i++) {
        if(!hub_gitwrite_unhex(argv[i], parents[np++])) return 2;
    }
    size_t len;
    char* buf = hub_gitwrite_commit_build(tree, parents, (size_t)np, &a, &c, m, &len);
    free(m);
    if(!buf) return 6;
    uint8_t id[20];
    char hex[41];
    int rc = hub_gitwrite_object(&io, repo, HUB_GW_COMMIT, buf, len, id, NULL);
    free(buf);
    if(rc) return 5;
    hub_gitwrite_hex(id, hex);
    printf("%s\n", hex);
    return 0;
}

#define CHECK(c) \
    do { \
        if(!(c)) { \
            fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while(0)

static int count_tmp(const char* repo) {
    char p[512];
    snprintf(p, sizeof(p), "%s/objects", repo);
    DIR* d = opendir(p);
    int n = 0;
    struct dirent* e;
    while(d && (e = readdir(d))) {
        if(strncmp(e->d_name, "tmp_obj_", 8) == 0) n++;
    }
    if(d) closedir(d);
    return n;
}

static int cmd_self(const char* repo) {
    HubGitWriteIo io;
    hub_gitwrite_io_stdio(&io);
    int fails = 0;
    uint8_t id[20], id2[20];
    bool ex;

    /* empty blob: well known id e69de29... */
    CHECK(hub_gitwrite_object(&io, repo, HUB_GW_BLOB, "", 0, id, &ex) == 0 && !ex);
    char hex[41];
    hub_gitwrite_hex(id, hex);
    CHECK(strcmp(hex, "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391") == 0);
    CHECK(hub_gitwrite_object(&io, repo, HUB_GW_BLOB, "", 0, id2, &ex) == 0 && ex);

    /* streamed with the wrong size */
    HubGitWriter* w = hub_gitwrite_begin(&io, repo, HUB_GW_BLOB, 10);
    CHECK(w != NULL);
    CHECK(hub_gitwrite_update(w, "12345", 5));
    CHECK(hub_gitwrite_end(w, id, NULL) == HUB_GW_ERR_SIZE);
    CHECK(count_tmp(repo) == 0);
    /* too many bytes */
    w = hub_gitwrite_begin(&io, repo, HUB_GW_BLOB, 3);
    CHECK(hub_gitwrite_update(w, "123", 3));
    CHECK(!hub_gitwrite_update(w, "4", 1));
    hub_gitwrite_abort(w);
    CHECK(count_tmp(repo) == 0);
    /* streamed object that exists: no new file, existed set */
    w = hub_gitwrite_begin(&io, repo, HUB_GW_BLOB, 0);
    CHECK(hub_gitwrite_end(w, id, &ex) == 0 && ex);
    CHECK(count_tmp(repo) == 0);
    /* bad arguments */
    CHECK(hub_gitwrite_object(&io, repo, 9, "x", 1, id, NULL) == HUB_GW_ERR_BADARG);
    CHECK(hub_gitwrite_begin(&io, repo, 0, 1) == NULL);

    /* tree rules */
    HubGitTreeEnt e[3];
    e[0] = (HubGitTreeEnt){"a", HUB_GW_MODE_FILE, {0}};
    e[1] = (HubGitTreeEnt){"a.b", HUB_GW_MODE_FILE, {0}};
    e[2] = (HubGitTreeEnt){"a", HUB_GW_MODE_TREE, {0}};
    size_t l;
    CHECK(hub_gitwrite_tree_build(e, 3, &l) == NULL); /* "a" twice, not adjacent */
    e[2] = (HubGitTreeEnt){"x/y", HUB_GW_MODE_FILE, {0}};
    CHECK(hub_gitwrite_tree_build(e, 3, &l) == NULL);
    e[2] = (HubGitTreeEnt){".git", HUB_GW_MODE_FILE, {0}};
    CHECK(hub_gitwrite_tree_build(e, 3, &l) == NULL);
    e[2] = (HubGitTreeEnt){"c", 0100600, {0}};
    CHECK(hub_gitwrite_tree_build(e, 3, &l) == NULL);
    uint8_t* t = hub_gitwrite_tree_build(e, 0, &l);
    CHECK(t != NULL && l == 0);
    free(t);

    /* commit rules */
    uint8_t tree[20] = {0};
    HubGwIdent good = {"N", "e@x", 1, "+0200"};
    HubGwIdent bad_tz = {"N", "e@x", 1, "0200"};
    HubGwIdent empty = {" <> ", "e@x", 1, NULL};
    HubGwIdent evil = {"A\nB <x>", "e\n@x", 1, NULL};
    CHECK(hub_gitwrite_commit_build(tree, NULL, 0, &bad_tz, &good, "m", &l) == NULL);
    CHECK(hub_gitwrite_commit_build(tree, NULL, 0, &empty, &good, "m", &l) == NULL);
    char* c = hub_gitwrite_commit_build(tree, NULL, 0, &evil, &good, "m", &l);
    CHECK(c && strstr(c, "\nauthor AB x <e@x> 1 +0000\n") != NULL);
    free(c);
    return fails;
}

int main(int argc, char** argv) {
    if(argc >= 4 && strcmp(argv[1], "sha1") == 0) return cmd_sha1(argv[2], (size_t)atol(argv[3]));
    if(argc >= 6 && strcmp(argv[1], "blob") == 0)
        return cmd_blob(argv[2], argv[3], strcmp(argv[4], "stream") == 0, (size_t)atol(argv[5]));
    if(argc >= 4 && strcmp(argv[1], "tree") == 0) return cmd_tree(argv[2], argv[3]);
    if(argc >= 7 && strcmp(argv[1], "commit") == 0) return cmd_commit(argv[2], argc - 3, argv + 3);
    if(argc >= 3 && strcmp(argv[1], "self") == 0) return cmd_self(argv[2]);
    fprintf(stderr, "usage: see the top of gitwrite_test.c\n");
    return 1;
}
