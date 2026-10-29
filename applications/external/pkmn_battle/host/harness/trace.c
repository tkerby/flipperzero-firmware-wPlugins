#include "trace.h"

#include <stdio.h>
#include <stdlib.h>

#include "gbh.h"

#define TICKS_PER_FRAME ((double)GBH_TICKS_PER_FRAME)

void trace_add(Trace* t, uint64_t ticks, uint8_t master, uint8_t slave, uint8_t by) {
    if(t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 4096;
        t->e = realloc(t->e, t->cap * sizeof(TraceEntry));
    }
    t->e[t->n++] = (TraceEntry){ticks, master, slave, by};
}

void trace_free(Trace* t) {
    free(t->e);
    t->e = NULL;
    t->n = t->cap = 0;
}

bool trace_write(const Trace* t, const char* path) {
    FILE* f = fopen(path, "w");
    if(!f) return false;
    for(size_t i = 0; i < t->n; i++) {
        fprintf(
            f,
            "%.3f %02X %02X %u\n",
            (double)t->e[i].ticks / TICKS_PER_FRAME,
            t->e[i].master,
            t->e[i].slave,
            t->e[i].by);
    }
    fclose(f);
    return true;
}

bool trace_read(Trace* t, const char* path) {
    FILE* f = fopen(path, "r");
    double frame;
    unsigned m, s, by;
    if(!f) return false;
    while(fscanf(f, "%lf %x %x %u", &frame, &m, &s, &by) == 4) {
        trace_add(t, (uint64_t)(frame * TICKS_PER_FRAME), (uint8_t)m, (uint8_t)s, (uint8_t)by);
    }
    fclose(f);
    return true;
}

uint64_t trace_hash(const Trace* t) {
    uint64_t h = 0xcbf29ce484222325ull;
    for(size_t i = 0; i < t->n; i++) {
        h = (h ^ t->e[i].master) * 0x100000001b3ull;
        h = (h ^ t->e[i].slave) * 0x100000001b3ull;
    }
    return h;
}
