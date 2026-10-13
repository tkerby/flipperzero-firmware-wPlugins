/* Heap accounting for route_test: the other files are built with malloc_wrap.h, this one is not. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* heap accounting: every file of this test is built with malloc_wrap.h */
size_t cur_b, peak_b;
typedef struct { size_t n, pad; } Hd;
void* t_malloc(size_t n) {
    Hd* h = malloc(sizeof(Hd) + n);
    if(!h) return NULL;
    h->n = n;
    cur_b += n;
    if(cur_b > peak_b) peak_b = cur_b;
    return h + 1;
}
void* t_calloc(size_t a, size_t b) {
    void* p = t_malloc(a * b);
    if(p) memset(p, 0, a * b);
    return p;
}
void t_free(void* p) {
    if(!p) return;
    Hd* h = (Hd*)p - 1;
    cur_b -= h->n;
    free(h);
}
void* t_realloc(void* p, size_t n) {
    if(!p) return t_malloc(n);
    Hd* h = (Hd*)p - 1;
    size_t old = h->n;
    Hd* nh = realloc(h, sizeof(Hd) + n);
    if(!nh) return NULL;
    nh->n = n;
    cur_b = cur_b - old + n;
    if(cur_b > peak_b) peak_b = cur_b;
    return nh + 1;
}

