/* Host tool for the loose-object packer and the deflate compressor.
 *   gitpack_tool pack <repo.git> [max_objects] [--stop-after K]
 *   gitpack_tool deflate <in> <out>
 * malloc of the library code is wrapped (malloc_wrap.h) to report the peak heap. */
#include "../../flipper_app/src/pack/hub_deflate.h"
#include "../../flipper_app/src/obj/hub_gitobj.h"
#include "../../flipper_app/src/pack/hub_gitpack.h"
#include "../../flipper_app/src/pack/hub_gitgraph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t cur, peak;
static size_t cur_heap(void) { return cur; }
typedef struct {
    size_t size, pad;
} H;
void* t_malloc(size_t n) {
    H* h = malloc(sizeof(H) + n);
    if(!h) return NULL;
    h->size = n;
    cur += n;
    if(cur > peak) peak = cur;
    return h + 1;
}
void* t_calloc(size_t a, size_t b) {
    void* p = t_malloc(a * b);
    if(p) memset(p, 0, a * b);
    return p;
}
void t_free(void* p) {
    if(!p) return;
    H* h = (H*)p - 1;
    cur -= h->size;
    free(h);
}
void* t_realloc(void* p, size_t n) {
    if(!p) return t_malloc(n);
    H* h = (H*)p - 1;
    void* q = t_malloc(n);
    if(!q) return NULL;
    memcpy(q, p, h->size < n ? h->size : n);
    t_free(p);
    return q;
}

static bool out_file(void* c, const uint8_t* d, size_t n) {
    return fwrite(d, 1, n, c) == n;
}

static unsigned stop_after = 0, seen = 0, drop_after = 0, drop_stop = 0;
static size_t ram_limit = 0;
static bool stop_now = false;
static unsigned polls, stop_every;
static bool merge_stop(void* c) {
    (void)c;
    return stop_every && ++polls >= stop_every;
}
static void prog(void* c, unsigned done, unsigned total) {
    (void)c; (void)total;
    seen = done;
}
static bool stopper(void* c) {
    (void)c;
    return stop_now || (stop_after && seen >= stop_after);
}

int main(int argc, char** argv) {
    if(argc >= 4 && !strcmp(argv[1], "deflate")) {
        FILE* in = fopen(argv[2], "rb");
        FILE* out = fopen(argv[3], "wb");
        if(!in || !out) return 2;
        peak = cur = 0;
        HubDeflate* d = hub_deflate_new(out_file, out);
        uint8_t buf[777]; /* odd chunk size on purpose */
        size_t n;
        while((n = fread(buf, 1, sizeof buf, in)) > 0) hub_deflate_write(d, buf, n);
        bool ok = hub_deflate_finish(d);
        hub_deflate_free(d);
        fclose(out);
        fprintf(stderr, "deflate heap peak %zu bytes\n", peak);
        return ok ? 0 : 1;
    }
    if(argc >= 3 && !strcmp(argv[1], "pack")) {
        unsigned max = argc >= 4 ? (unsigned)atoi(argv[3]) : 300;
        for(int i = 4; i < argc; i++) if(!strcmp(argv[i], "--stop-now")) stop_now = true;
        for(int i = 4; i + 1 < argc; i++)
            if(!strcmp(argv[i], "--stop-after")) stop_after = (unsigned)atoi(argv[i + 1]);
            else if(!strcmp(argv[i], "--ram-limit")) ram_limit = (size_t)atol(argv[i + 1]);
            else if(!strcmp(argv[i], "--drop-after")) drop_after = (unsigned)atoi(argv[i + 1]);
            else if(!strcmp(argv[i], "--drop-stop")) { drop_after = (unsigned)atoi(argv[i + 1]); drop_stop = 1; }
        HubGitPackCursor cursor = {0};
        unsigned calls = 0;
        for(;;) {
            HubGitPackResult r;
            seen = 0;
            peak = 0;
            int e = hub_git_pack_loose(NULL, argv[2], max, ram_limit, &cursor, prog, stopper, NULL, &r);
            calls++;
            printf("pack call: e=%d packed=%u more=%d left=%u bytes=%u stopped=%d fin=%d cursor=%u job=%d heap_peak=%zu leak=%zu\n",
                   e, r.packed, r.more, r.remaining, r.pack_bytes, r.stopped, r.finished, cursor.dir, cursor.job != NULL, peak, cur_heap());
            if(e) return 1;
            if(drop_after && calls == drop_after) { /* a restart: the cursor is lost, the tmp files stay */
                free(cursor.job);
                memset(&cursor, 0, sizeof cursor);
                printf("cursor dropped\n");
                if(argc > 0 && drop_stop) return 0;
            }
            if(!r.more || calls > 100000 || (stop_now && calls >= 3)) {
                printf("%u calls\n", calls);
                return 0;
            }
        }
    }
    if(argc >= 3 && !strcmp(argv[1], "scan")) { /* oids on stdin: count lookup results */
        HubGit* g = hub_git_open(NULL, argv[2]);
        char line[128];
        unsigned ok = 0, toomany = 0, nf = 0, other = 0;
        while(g && fgets(line, sizeof line, stdin)) {
            HubOid o;
            line[strcspn(line, "\n")] = 0;
            if(!hub_oid_from_hex(line, &o)) continue;
            uint8_t ty;
            size_t sz;
            int e = hub_git_info(g, &o, &ty, &sz);
            if(e == 0) ok++;
            else if(e == HUB_GIT_ERR_TOOMANY) toomany++;
            else if(e == HUB_GIT_ERR_NOTFOUND) nf++;
            else other++;
        }
        if(g) hub_git_close(g);
        printf("scan: ok=%u toomany=%u notfound=%u other=%u\n", ok, toomany, nf, other);
        return 0;
    }
    if(argc >= 3 && !strcmp(argv[1], "merge")) { /* merge <repo> [small_limit] [stop_every] */
        uint32_t lim = argc >= 4 ? (uint32_t)atol(argv[3]) : 0;
        unsigned every = argc >= 5 ? (unsigned)atoi(argv[4]) : 0;
        static HubGitMergeCursor mc;
        unsigned prev_total = 1u << 30;
        for(int job = 1;; job++) {
            unsigned total, small;
            hub_git_pack_stats(NULL, argv[2], lim, &total, &small);
            if(total >= prev_total) {
                printf("  no progress: stop\n");
                return 0;
            }
            prev_total = total;
            printf("job %d: packs=%u small=%u\n", job, total, small);
            if(small < 2) return 0;
            unsigned calls = 0;
            memset(&mc, 0, sizeof mc);
            for(;;) {
                HubGitMergeResult r;
                peak = 0;
                polls = 0;
                stop_every = every;
                int e = hub_git_pack_merge(NULL, argv[2], &mc, lim, 0, merge_stop, NULL, &r);
                calls++;
                printf("  merge call: e=%d more=%d stopped=%d phase=%u src=%u/%u total=%u merged=%u objects=%u heap_peak=%zu leak=%zu\n",
                       e, r.more, r.stopped, mc.phase, mc.cur_src, mc.nsrc, mc.total, r.merged_packs, r.objects, peak, cur_heap());
                if(e) return 1;
                if(!r.more) break;
            }
            printf("  job took %u calls, merged %s\n", calls, "ok");
            if(argc >= 6) return 0; /* one job only */
        }
    }
    if(argc >= 3 && !strcmp(argv[1], "graph")) { /* graph <repo> [stop_every]: slices until done */
        unsigned every = argc >= 4 ? (unsigned)atoi(argv[3]) : 0;
        unsigned calls = 0, total = 0;
        for(;;) {
            HubGitGraphResult gr;
            polls = 0;
            stop_every = every;
            peak = 0;
            int e = hub_git_graph_update(NULL, argv[2], merge_stop, NULL, &gr);
            calls++;
            total += gr.added;
            printf("graph call: e=%d added=%u more=%d stopped=%d heap_peak=%zu leak=%zu\n", e, gr.added, gr.more, gr.stopped, peak, cur_heap());
            if(e) return 1;
            if(!gr.more || calls > 5000) break;
        }
        printf("graph: %u calls, %u records\n", calls, total);
        return 0;
    }
    fprintf(stderr, "usage\n");
    return 2;
}
