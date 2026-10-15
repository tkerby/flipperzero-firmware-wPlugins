#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t time_ms; /* relative to the beginning of the audio */
    uint8_t kind;
} TjaNote;

typedef struct {
    char title[128];
    char wave[256];
    char course[24];
    TjaNote* notes;
    size_t note_count;
    int32_t end_ms;
} TjaChart;

/* Return 1 for a line (including an empty one), 0 for EOF, -1 for an error.
 * The reader must reject lines that do not fit, and NUL-terminate each line. */
typedef int (*TjaReadLine)(void* context, char* line, size_t capacity);

/* Reads only the first #START ... #END chart. On failure, chart is still safe to free. */
bool tja_parse(TjaReadLine reader, void* context, TjaChart* chart, const char** error);
void tja_chart_free(TjaChart* chart);
