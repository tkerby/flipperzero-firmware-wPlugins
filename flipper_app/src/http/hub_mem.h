#pragma once

/* Guarded allocator. Include it as the LAST include of every app .c file.
 * The firmware halts the device when malloc fails, so malloc/calloc/realloc
 * refuse (return NULL) unless ONE free block holds the request plus the
 * reserve for the system. Callers handle NULL. free() is not wrapped.
 * On the host (tests) this does nothing. */

#include <stddef.h>

#if defined(__arm__) && !defined(HUB_MEM_RAW)

void* hub_malloc(size_t n);
void* hub_calloc(size_t n, size_t m);
void* hub_realloc(void* p, size_t n);

#define malloc(n)     hub_malloc(n)
#define calloc(n, m)  hub_calloc(n, m)
#define realloc(p, n) hub_realloc(p, n)

#endif
