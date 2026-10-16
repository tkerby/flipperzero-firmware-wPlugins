#pragma once

#include <stdbool.h>
#include <stdlib.h>

#ifdef FAP_VERSION
#include <furi/core/memmgr.h>
#include <furi/core/memmgr_heap.h>
#endif

/* Firmware malloc can abort instead of returning NULL. This conservative
 * preflight leaves space for subsequent work and rejects fragmented heaps.
 * It does not reserve memory against allocations made by other threads. */
static inline bool taiko_memory_available(size_t size, size_t reserve) {
    if(!size) return false;
#ifdef FAP_VERSION
    const size_t overhead = 32;
    const size_t available = memmgr_get_free_heap();
    const size_t largest = memmgr_heap_get_max_free_block();
    return size <= available && reserve <= available - size && largest >= overhead &&
           size <= largest - overhead;
#else
    (void)reserve;
    return true;
#endif
}

static inline void* taiko_alloc(size_t size, size_t reserve) {
    return taiko_memory_available(size, reserve) ? malloc(size) : NULL;
}
