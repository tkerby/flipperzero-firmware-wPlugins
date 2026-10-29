#pragma once

/* Byte trace of a link: one entry per completed transfer, timed by the
 * clock master. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t ticks; /* master's emulated time (8 MiHz ticks) at the last bit */
    uint8_t master; /* byte the clock master shifted out */
    uint8_t slave; /* byte the other side shifted out */
    uint8_t by; /* which side clocked it (twin traces: GB 0 or 1) */
} TraceEntry;

typedef struct {
    TraceEntry* e;
    size_t n;
    size_t cap;
} Trace;

void trace_add(Trace* t, uint64_t ticks, uint8_t master, uint8_t slave, uint8_t by);
void trace_free(Trace* t);
/* Text file: "frame.fraction master slave by" per line (frames of 1/59.73 s). */
bool trace_write(const Trace* t, const char* path);
bool trace_read(Trace* t, const char* path);
/* FNV-1a over the byte pairs (not the times). */
uint64_t trace_hash(const Trace* t);
