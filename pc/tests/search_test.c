/* Host driver for hub_gitsearch.c.
 *   search_test <repo> <tree> <query> [max_hits] [max_blob] [stop_after_chunks] [flags]
 * flags: n = no name hits, c = no content hits
 * Prints "H<TAB>path<TAB>line<TAB>cut<TAB>text" per hit and a summary line "S ...". */
#include "../../flipper_app/hub_gitsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool on_hit(void* ctx, const HubGitHit* h) {
    (void)ctx;
    printf("H\t%s\t%u\t%d\t", h->path, h->line, h->cut);
    fwrite(h->text, 1, h->text_len, stdout);
    printf("\n");
    return true;
}

static size_t stop_calls;
static bool should_stop(void* ctx) {
    size_t* left = ctx;
    if(*left == 0) return true;
    (*left)--;
    stop_calls++;
    return false;
}

int main(int argc, char** argv) {
    if(argc < 4) return 1;
    HubGit* g = hub_git_open(NULL, argv[1]);
    if(!g) return 3;
    HubOid tree;
    if(!hub_oid_from_hex(argv[2], &tree)) return 2;
    HubGitSearch s;
    memset(&s, 0, sizeof(s));
    s.query = argv[3];
    s.max_hits = argc > 4 ? (size_t)atol(argv[4]) : 0;
    s.max_blob = argc > 5 ? (size_t)atol(argv[5]) : 0;
    size_t budget = argc > 6 && atol(argv[6]) > 0 ? (size_t)atol(argv[6]) : 0;
    if(budget) {
        s.should_stop = should_stop;
        s.stop_ctx = &budget;
    }
    if(argc > 7) {
        s.no_names = strchr(argv[7], 'n') != NULL;
        s.no_content = strchr(argv[7], 'c') != NULL;
    }
    s.on_hit = on_hit;
    peak_bytes = cur_bytes;
    int rc = hub_git_search(g, &tree, &s);
    printf("S rc=%d hits=%zu scanned=%u skipped=%u binary=%u truncated=%d timed_out=%d\n", rc, s.hits,
           s.scanned, s.skipped, s.binary, s.truncated, s.timed_out);
    fprintf(stderr, "heap_peak=%zu\n", peak_bytes);
    hub_git_close(g);
    return 0;
}
