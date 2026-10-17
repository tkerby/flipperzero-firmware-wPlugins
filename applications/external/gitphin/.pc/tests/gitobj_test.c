/* Host test harness for flipper_app/hub_inflate.c and hub_gitobj.c.
 *
 *   gitobj_test --inflate
 *   gitobj_test <repo.git> [--limit N]       full comparison against git
 *   gitobj_test <repo.git> --fuzz            corrupt a COPY of the repo, no crash expected
 *
 * Everything in the library is compared with the real git. malloc of the library
 * code is wrapped (malloc_wrap.h) to measure the peak heap per operation.
 * The test parts live in pc/tests/gitobj/ and are included below (one translation unit). */
#include "gitobj/common.h"
#include "gitobj/inflate.h"
#include "gitobj/helpers.h"
#include "gitobj/matrix.h"
#include "gitobj/trees.h"
#include "gitobj/refs.h"
#include "gitobj/fuzz.h"

/* ---------- main ---------- */

int main(int argc, char** argv) {
    if(argc >= 2 && strcmp(argv[1], "--inflate") == 0) return test_inflate() | test_far();
    if(argc < 2) {
        fprintf(stderr, "usage: %s --inflate | <repo.git> [--limit N] [--fuzz]\n", argv[0]);
        return 2;
    }
    snprintf(repo, sizeof repo, "%s", argv[1]);
    size_t limit = HUB_GIT_MAX_OBJ;
    bool fuzz = false;
    for(int i = 2; i < argc; i++) {
        if(!strcmp(argv[i], "--limit") && i + 1 < argc) limit = (size_t)atol(argv[++i]);
        if(!strcmp(argv[i], "--fuzz")) fuzz = true;
    }
    if(fuzz) {
        test_fuzz(argv[1]);
        printf("%s: checks=%u fail=%u\n", argv[1], n_checks, n_fail);
        return n_fail != 0;
    }

    heap_mark();
    HubGit* g = hub_git_open(NULL, repo);
    if(!g) {
        fprintf(stderr, "cannot open %s\n", repo);
        return 2;
    }
    hub_git_set_limit(g, limit);
    printf("== %s  limit=%zu\n", argv[1], limit);

    test_matrix(g, limit);

    /* the structure tests need room for big trees */
    hub_git_set_limit(g, 1024 * 1024);
    test_trees(g);
    test_commits(g);
    test_refs(g);
    {
        char* head = git_out("rev-parse HEAD");
        head[40] = 0;
        HubOid o;
        hub_oid_from_hex(head, &o);
        free(head);
        test_nomem(g, &o);
    }
    hub_git_close(g);
    CHECK(heap_cur == 0, "leak: %zu bytes still allocated after close", heap_cur);

    printf("peak heap per operation (bytes above the open handle; limit %zu):\n", limit);
    printf("  %-20s %8s %10s %9s %6s\n", "operation", "peak", "at size", "overhead", "count");
    for(unsigned i = 0; i < nclasses; i++) {
        printf("  %-20s %8zu %10zu %9zu %6u\n", classes[i].name, classes[i].peak, classes[i].at_size,
               classes[i].over, classes[i].n);
    }
    printf("%s: checks=%u fail=%u\n", argv[1], n_checks, n_fail);
    return n_fail != 0;
}
