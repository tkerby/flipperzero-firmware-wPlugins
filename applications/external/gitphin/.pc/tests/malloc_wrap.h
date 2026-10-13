/* Force-included (gcc -include) into hub_inflate.c and hub_gitobj.c only, so
 * the test harness can measure the heap use of the library code. */
#ifndef MALLOC_WRAP_H
#define MALLOC_WRAP_H
#include <stdlib.h>
void* t_malloc(size_t n);
void* t_calloc(size_t a, size_t b);
void* t_realloc(void* p, size_t n);
void t_free(void* p);
#define malloc(n)     t_malloc(n)
#define calloc(a, b)  t_calloc(a, b)
#define realloc(p, n) t_realloc(p, n)
#define free(p)       t_free(p)
#endif
