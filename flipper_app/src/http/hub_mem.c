#define HUB_MEM_RAW
#include "hub_mem.h"

#include <furi.h>
#include <stdlib.h>

#define HUB_MEM_RESERVE (8 * 1024)

void* hub_malloc(size_t n) {
    if(memmgr_heap_get_max_free_block() < n + HUB_MEM_RESERVE) return NULL;
    return malloc(n);
}

void* hub_calloc(size_t n, size_t m) {
    if(memmgr_heap_get_max_free_block() < n * m + HUB_MEM_RESERVE) return NULL;
    return calloc(n, m);
}

void* hub_realloc(void* p, size_t n) {
    if(memmgr_heap_get_max_free_block() < n + HUB_MEM_RESERVE) return NULL;
    return realloc(p, n);
}
