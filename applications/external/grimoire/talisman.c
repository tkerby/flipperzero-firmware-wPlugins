#include "talisman.h"
#include <ctype.h>

static const uint8_t saturn[] = {4, 9, 2, 3, 5, 7, 8, 1, 6};
static const uint8_t jupiter[] = {4, 14, 15, 1, 9, 7, 6, 12, 5, 11, 10, 8, 16, 2, 3, 13};
static const uint8_t mars[] = {11, 24, 7,  20, 3, 4,  12, 25, 8, 16, 17, 5, 13,
                               21, 9,  10, 18, 1, 14, 22, 23, 6, 19, 2,  15};
static const uint8_t sun[] = {6,  32, 3,  34, 35, 1,  7,  11, 27, 28, 8,  30,
                              19, 14, 16, 15, 23, 24, 18, 20, 22, 21, 17, 13,
                              25, 29, 10, 9,  26, 12, 36, 5,  33, 4,  2,  31};
static const uint8_t venus[] = {22, 47, 16, 41, 10, 35, 4,  5,  23, 48, 17, 42, 11, 29, 30, 6,  24,
                                49, 18, 36, 12, 13, 31, 7,  25, 43, 19, 37, 38, 14, 32, 1,  26, 44,
                                20, 21, 39, 8,  33, 2,  27, 45, 46, 15, 40, 9,  34, 3,  28};
static const uint8_t mercury[] = {8,  58, 59, 5,  4,  62, 63, 1,  49, 15, 14, 52, 53, 11, 10, 56,
                                  41, 23, 22, 44, 45, 19, 18, 48, 32, 34, 35, 29, 28, 38, 39, 25,
                                  40, 26, 27, 37, 36, 30, 31, 33, 17, 47, 46, 20, 21, 43, 42, 24,
                                  9,  55, 54, 12, 13, 51, 50, 16, 64, 2,  3,  61, 60, 6,  7,  57};
static const uint8_t moon[] = {37, 78, 29, 70, 21, 62, 13, 54, 5,  6,  38, 79, 30, 71, 22, 63, 14,
                               46, 47, 7,  39, 80, 31, 72, 23, 55, 15, 16, 48, 8,  40, 81, 32, 64,
                               24, 56, 57, 17, 49, 9,  41, 73, 33, 65, 25, 26, 58, 18, 50, 1,  42,
                               74, 34, 66, 67, 27, 59, 10, 51, 2,  43, 75, 35, 36, 68, 19, 60, 11,
                               52, 3,  44, 76, 77, 28, 69, 20, 61, 12, 53, 4,  45};

static const uint8_t* const squares[PlanetCount] =
    {saturn, jupiter, mars, sun, venus, mercury, moon};
static const uint8_t sides[PlanetCount] = {3, 4, 5, 6, 7, 8, 9};
static const char* const metals[PlanetCount] =
    {"lead", "tin", "iron", "gold", "copper", "quicksilver", "silver"};

static const uint16_t v_agiel[] = {1, 3, 10, 1, 30};
static const uint16_t v_zazel[] = {7, 1, 7, 30};
static const uint16_t v_yophiel[] = {10, 5, 80, 10, 1, 30};
static const uint16_t v_hismael[] = {5, 60, 40, 1, 30};
static const uint16_t v_graphiel[] = {3, 200, 1, 80, 10, 1, 30};
static const uint16_t v_bartzabel[] = {2, 200, 90, 2, 1, 30};
static const uint16_t v_nakhiel[] = {50, 20, 10, 1, 30};
static const uint16_t v_sorath[] = {60, 6, 200, 400};
static const uint16_t v_hagiel[] = {5, 3, 10, 1, 30};
static const uint16_t v_kedemel[] = {100, 4, 40, 1, 30};
static const uint16_t v_tiriel[] = {9, 10, 200, 10, 1, 30};
static const uint16_t v_taph[] = {400, 80, 400, 200, 400, 200, 400};
static const uint16_t v_chasmodai[] = {8, 300, 40, 6, 4, 1, 10};

#define NAME(n, v) {n, v, sizeof(v) / sizeof(uint16_t)}
static const TalismanName intelligences[PlanetCount] = {
    NAME("Agiel", v_agiel),
    NAME("Yophiel", v_yophiel),
    NAME("Graphiel", v_graphiel),
    NAME("Nakhiel", v_nakhiel),
    NAME("Hagiel", v_hagiel),
    NAME("Tiriel", v_tiriel),
    {0, 0, 0},
};
static const TalismanName spirits[PlanetCount] = {
    NAME("Zazel", v_zazel),
    NAME("Hismael", v_hismael),
    NAME("Bartzabel", v_bartzabel),
    NAME("Sorath", v_sorath),
    NAME("Kedemel", v_kedemel),
    NAME("Taphthartharath", v_taph),
    NAME("Chasmodai", v_chasmodai),
};

// A..Z; the consonant I and V counted apart, W taken as the consonant V
static const uint16_t latin[26] = {1,  2,  3,  4,  5,  6,  7,   8,   9,   600, 10,  20,  30,
                                   40, 50, 60, 70, 80, 90, 100, 200, 200, 700, 300, 400, 500};

const uint8_t* talisman_square(Planet planet, uint8_t* side) {
    *side = sides[planet];
    return squares[planet];
}

uint16_t talisman_row_sum(Planet planet) {
    uint16_t s = 0;
    for(uint8_t i = 0; i < sides[planet]; i++)
        s += squares[planet][i];
    return s;
}

uint16_t talisman_total(Planet planet) {
    uint16_t n = sides[planet] * sides[planet];
    return n * (n + 1) / 2;
}

const char* talisman_metal(Planet planet) {
    return metals[planet];
}

const TalismanName* talisman_intelligence(Planet planet) {
    return intelligences[planet].name ? &intelligences[planet] : NULL;
}

const TalismanName* talisman_spirit(Planet planet) {
    return &spirits[planet];
}

size_t talisman_latin(const char* name, uint16_t* out, size_t cap) {
    size_t n = 0;
    for(const char* p = name; *p && n < cap; p++) {
        char c = toupper((unsigned char)*p);
        if(c < 'A' || c > 'Z') continue;
        out[n++] = latin[c - 'A'];
    }
    return n;
}

uint16_t talisman_sum(const uint16_t* values, size_t count) {
    uint16_t s = 0;
    for(size_t i = 0; i < count; i++)
        s += values[i];
    return s;
}

bool talisman_cell(Planet planet, uint16_t value, uint8_t* col, uint8_t* row) {
    uint8_t n = sides[planet];
    while(value > n * n)
        value /= 10;
    if(value < 1) return false;
    const uint8_t* sq = squares[planet];
    for(uint8_t i = 0; i < n * n; i++) {
        if(sq[i] == value) {
            *col = i % n;
            *row = i / n;
            return true;
        }
    }
    return false;
}
