// The sigil press, after Austin Osman Spare (The Book of Pleasure, 1913), as
// the site's temple/sigil.js does it, step for step, so a wish gives the same
// sigil here as there. Pure C.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SIGIL_MAX_LETTERS 26
#define SIGIL_MAX_STROKES 96
#define SIGIL_MAX_POINTS  1024

typedef struct {
    float x;
    float y;
} SigilPoint;

typedef struct {
    uint16_t first; // index into points
    uint16_t count;
} SigilStroke;

typedef struct {
    SigilPoint points[SIGIL_MAX_POINTS];
    SigilStroke strokes[SIGIL_MAX_STROKES];
    uint16_t point_count;
    uint16_t stroke_count;
    char letters[SIGIL_MAX_LETTERS + 1];
} Sigil;

// Strike the vowels and every repeated letter. A wish of nothing but vowels keeps them.
// Returns the number of letters kept; out holds them, upper case, in order.
size_t sigil_reduce(const char* wish, char* out, size_t cap);

// Reduce and compose. False when the wish has no letters.
bool sigil_make(const char* wish, Sigil* sigil);
