#pragma once
/* Host stand-in for the parts of furi.h that the Gitphin route code uses (pc/tests/route_test). */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define UNUSED(x) (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#define EXT_PATH(p) "/ext/" p

typedef struct FuriMutex FuriMutex;

typedef struct {
    char* b;
    size_t n, cap;
} FuriString;

static inline void fs_grow(FuriString* s, size_t need) {
    if(need + 1 <= s->cap) return;
    size_t c = s->cap ? s->cap : 16;
    while(c < need + 1) c *= 2;
    s->b = realloc(s->b, c);
    s->cap = c;
}
static inline FuriString* furi_string_alloc(void) {
    FuriString* s = calloc(1, sizeof(*s));
    fs_grow(s, 0);
    s->b[0] = 0;
    return s;
}
static inline void furi_string_free(FuriString* s) {
    if(!s) return;
    free(s->b);
    free(s);
}
static inline const char* furi_string_get_cstr(const FuriString* s) { return s->b; }
static inline size_t furi_string_size(const FuriString* s) { return s->n; }
static inline void furi_string_reset(FuriString* s) { s->n = 0; s->b[0] = 0; }
static inline void furi_string_reserve(FuriString* s, size_t n) { fs_grow(s, n); }
static inline void furi_string_push_back(FuriString* s, char c) {
    fs_grow(s, s->n + 1);
    s->b[s->n++] = c;
    s->b[s->n] = 0;
}
static inline void furi_string_cat_str(FuriString* s, const char* t) {
    size_t l = strlen(t);
    fs_grow(s, s->n + l);
    memcpy(s->b + s->n, t, l + 1);
    s->n += l;
}
static inline void furi_string_set_str(FuriString* s, const char* t) {
    furi_string_reset(s);
    furi_string_cat_str(s, t);
}
static inline void furi_string_cat(FuriString* s, const FuriString* t) { furi_string_cat_str(s, t->b); }
static inline void furi_string_set(FuriString* s, const FuriString* t) { furi_string_set_str(s, t->b); }
static inline FuriString* furi_string_alloc_set(const char* t) {
    FuriString* s = furi_string_alloc();
    furi_string_cat_str(s, t);
    return s;
}
static inline void furi_string_vcat(FuriString* s, const char* f, va_list ap) {
    va_list c;
    va_copy(c, ap);
    int l = vsnprintf(NULL, 0, f, c);
    va_end(c);
    fs_grow(s, s->n + (size_t)l);
    vsnprintf(s->b + s->n, (size_t)l + 1, f, ap);
    s->n += (size_t)l;
}
static inline void furi_string_cat_printf(FuriString* s, const char* f, ...) {
    va_list ap;
    va_start(ap, f);
    furi_string_vcat(s, f, ap);
    va_end(ap);
}
static inline void furi_string_printf(FuriString* s, const char* f, ...) {
    furi_string_reset(s);
    va_list ap;
    va_start(ap, f);
    furi_string_vcat(s, f, ap);
    va_end(ap);
}
static inline FuriString* furi_string_alloc_printf(const char* f, ...) {
    FuriString* s = furi_string_alloc();
    va_list ap;
    va_start(ap, f);
    furi_string_vcat(s, f, ap);
    va_end(ap);
    return s;
}
static inline void furi_string_set_n(FuriString* s, const FuriString* t, size_t off, size_t n) {
    furi_string_reset(s);
    fs_grow(s, n);
    memcpy(s->b, t->b + off, n);
    s->n = n;
    s->b[n] = 0;
}
static inline size_t furi_string_search_char(const FuriString* s, char c, size_t start) {
    const char* p = strchr(s->b + start, c);
    return p ? (size_t)(p - s->b) : (size_t)-1;
}
static inline char furi_string_get_char(const FuriString* s, size_t i) { return s->b[i]; }

uint32_t furi_get_tick(void);
static inline uint32_t furi_ms_to_ticks(uint32_t ms) { return ms; }
static inline uint32_t furi_kernel_get_tick_frequency(void) { return 1000; }
size_t memmgr_get_free_heap(void);
size_t memmgr_heap_get_max_free_block(void);
