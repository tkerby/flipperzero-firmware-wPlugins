// Force the app's own allocations through the shim's budgeted heap.
//
// Included with -include so it lands before rf_survey.c's first line. stdlib is pulled in FIRST
// and only then are the names redefined: otherwise the macro would rewrite malloc's own
// prototype into nonsense.
//
// Without this the simulator under-reported by the size of the App struct (11.7 KB), which is
// the single largest allocation the app makes -- an OOM sweep that ignores it is measuring the
// wrong thing.
#pragma once
#include <stdlib.h>
#include <stddef.h>

void* sim_alloc(size_t n);
void sim_free(void* p);
void* sim_calloc(size_t n, size_t size);
void* sim_realloc(void* p, size_t n);

#define malloc(n) sim_alloc(n)
#define free(p) sim_free(p)
#define calloc(n, s) sim_calloc((n), (s))
#define realloc(p, n) sim_realloc((p), (n))
