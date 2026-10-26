// Agrippa's seven planetary squares (Three Books of Occult Philosophy II.22)
// with the names of their intelligences and spirits as Hebrew letter values,
// and his Latin numeration (II.20), as the site's temple/talisman.js has them.
// Pure C.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hours.h"

typedef struct {
    const char* name;
    const uint16_t* values; // Hebrew letter values, in order
    uint8_t count;
} TalismanName;

// The square, row-major, and its side (3 for Saturn to 9 for the Moon)
const uint8_t* talisman_square(Planet planet, uint8_t* side);
uint16_t talisman_row_sum(Planet planet);
uint16_t talisman_total(Planet planet);
const char* talisman_metal(Planet planet);

// NULL for the Moon, whose intelligence of intelligences has a name of five words
const TalismanName* talisman_intelligence(Planet planet);
const TalismanName* talisman_spirit(Planet planet);

// Agrippa's Latin numeration of a name. Returns how many letters counted.
size_t talisman_latin(const char* name, uint16_t* out, size_t cap);
uint16_t talisman_sum(const uint16_t* values, size_t count);

// A number's cell: a value past the square is read with its last figure dropped until it fits.
bool talisman_cell(Planet planet, uint16_t value, uint8_t* col, uint8_t* row);
