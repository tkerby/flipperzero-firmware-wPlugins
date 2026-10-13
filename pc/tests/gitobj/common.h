/* gitobj_test part: includes, heap accounting, check helpers. Included by ../gitobj_test.c (one translation unit). */
#define _GNU_SOURCE
#include "../../../flipper_app/src/obj/hub_gitobj.h"
#include "../../../flipper_app/src/util/hub_inflate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ---------- heap accounting (library code only) ---------- */

static size_t heap_cur, heap_peak, heap_base, heap_fail_after = (size_t)-1, heap_calls;

typedef struct {
    size_t size;
    size_t pad;
} MHdr;

void* t_malloc(size_t n) {
    if(++heap_calls > heap_fail_after) return NULL;
    MHdr* h = malloc(sizeof(MHdr) + n);
    if(!h) return NULL;
    h->size = n;
    heap_cur += n;
    if(heap_cur > heap_peak) heap_peak = heap_cur;
    return h + 1;
}
void* t_calloc(size_t a, size_t b) {
    void* p = t_malloc(a * b);
    if(p) memset(p, 0, a * b);
    return p;
}
void t_free(void* p) {
    if(!p) return;
    MHdr* h = (MHdr*)p - 1;
    heap_cur -= h->size;
    free(h);
}
void* t_realloc(void* p, size_t n) {
    if(!p) return t_malloc(n);
    MHdr* h = (MHdr*)p - 1;
    void* q = t_malloc(n);
    if(!q) return NULL;
    memcpy(q, p, h->size < n ? h->size : n);
    t_free(p);
    return q;
}
static void heap_mark(void) {
    heap_base = heap_cur;
    heap_peak = heap_cur;
}
static size_t heap_used(void) {
    return heap_peak - heap_base;
}

/* ---------- check helpers ---------- */

static unsigned n_checks, n_fail;
static void fail(const char* fmt, ...) {
    n_fail++;
    if(n_fail <= 25) {
        va_list ap;
        va_start(ap, fmt);
        fprintf(stderr, "FAIL: ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        va_end(ap);
    }
}
#define CHECK(cond, ...) \
    do { \
        n_checks++; \
        if(!(cond)) fail(__VA_ARGS__); \
    } while(0)

static const char* errname(int e) {
    switch(e) {
    case HUB_GIT_OK: return "OK";
    case HUB_GIT_ERR_NOTFOUND: return "NOTFOUND";
    case HUB_GIT_ERR_TOOLARGE: return "TOOLARGE";
    case HUB_GIT_ERR_CORRUPT: return "CORRUPT";
    case HUB_GIT_ERR_NOMEM: return "NOMEM";
    case HUB_GIT_ERR_IO: return "IO";
    case HUB_GIT_ERR_BADARG: return "BADARG";
    case HUB_GIT_ERR_WRITE: return "WRITE";
    }
    return "?";
}

