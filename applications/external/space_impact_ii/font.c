#include <string.h>
#include "graphics.h"

Uint8 SpaceFont[128][40], /* ASCII tábla a telefon betûtípusában, 5*8-as méretben */
    CompressedFont[94][5] = {
        /* A betűtípus tömörített verziója, 34-gyel kevesebb bejegyzéssel, mivel az első 32 és a [127] nem karakter, a [32] pedig szóköz */
        {33, 8, 66, 0, 128},
        /*  33: ! */ {82, 148},
        /*  34: " */ {82, 190, 175, 169, 64}, /*  35: # */
        {35, 232, 226, 248, 128},
        /*  36: $ */ {198, 68, 68, 76, 96},
        /*  37: % */ {100, 168, 138, 201, 160}, /*  38: & */
        {33},
        /*  39: ' */ {17, 16, 132, 16, 64},
        /*  40: ( */ {65, 4, 33, 17}, /*  41: ) */
        {1, 42, 234, 144},
        /*  42: * */ {1, 9, 242, 16},
        /*  43: + */ {0, 0, 6, 17}, /*  44: , */
        {0, 1, 240},
        /*  45: - */ {0, 0, 0, 49, 128},
        /*  46: . */ {0, 68, 68, 64}, /*  47: / */
        {116, 103, 92, 197, 192},
        /*  48: 0 */ {35, 8, 66, 17, 192},
        /*  49: 1 */ {116, 66, 34, 35, 224}, /*  50: 2 */
        {248, 136, 32, 197, 192},
        /*  51: 3 */ {17, 149, 47, 136, 64},
        /*  52: 4 */ {252, 60, 16, 197, 192}, /*  53: 5 */
        {50, 33, 232, 197, 192},
        /*  54: 6 */ {252, 68, 68, 33},
        /*  55: 7 */ {116, 98, 232, 197, 192}, /*  56: 8 */
        {116, 98, 240, 137, 128},
        /*  57: 9 */ {3, 24, 6, 48},
        /*  58: : */ {3, 24, 6, 17}, /*  59: ; */
        {17, 17, 4, 16, 64},
        /*  60: < */ {0, 62, 15, 128},
        /*  61: = */ {65, 4, 17, 17}, /*  62: > */
        {116, 66, 34, 0, 128},
        /*  63: ? */ {116, 66, 218, 213, 192},
        /*  64: @ */ {116, 99, 31, 198, 32}, /*  65: A */
        {244, 99, 232, 199, 192},
        /*  66: B */ {116, 97, 8, 69, 192},
        /*  67: C */ {244, 99, 24, 199, 192}, /*  68: D */
        {252, 33, 232, 67, 224},
        /*  69: E */ {252, 33, 232, 66},
        /*  70: F */ {116, 97, 56, 197, 224}, /*  71: G */
        {140, 99, 248, 198, 32},
        /*  72: H */ {113, 8, 66, 17, 192},
        /*  73: I */ {56, 132, 33, 73, 128}, /*  74: J */
        {140, 169, 138, 74, 32},
        /*  75: K */ {132, 33, 8, 67, 224},
        /*  76: L */ {142, 235, 88, 198, 32}, /*  77: M */
        {140, 115, 89, 198, 32},
        /*  78: N */ {116, 99, 24, 197, 192},
        /*  79: O */ {244, 99, 232, 66, 0}, /*  80: P */
        {116, 99, 26, 201, 160},
        /*  81: Q */ {244, 99, 234, 74, 32},
        /*  82: R */ {124, 32, 224, 135, 192}, /*  83: S */
        {249, 8, 66, 16, 128},
        /*  84: T */ {140, 99, 24, 197, 192},
        /*  85: U */ {140, 99, 24, 168, 128}, /*  86: V */
        {140, 99, 90, 213, 64},
        /*  87: W */ {140, 84, 69, 70, 32},
        /*  88: X */ {140, 98, 162, 16, 128}, /*  89: Y */
        {248, 68, 68, 67, 224},
        /*  90: Z */ {114, 16, 132, 33, 192},
        /*  91: [ */ {4, 16, 65, 4}, /*  92: \ */
        {112, 132, 33, 9, 192},
        /*  93: ] */ {34, 162},
        /*  94: ^ */ {0, 0, 0, 3, 224}, /*  95: _ */
        {65, 4},
        /*  96: ` */ {0, 28, 23, 197, 224},
        /*  97: a */ {132, 45, 152, 199, 192}, /*  98: b */
        {0, 29, 8, 69, 192},
        /*  99: c */ {8, 91, 56, 197, 224},
        /* 100: d */ {0, 29, 31, 193, 192}, /* 101: e */
        {50, 81, 196, 33},
        /* 102: f */ {0, 31, 24, 188, 46},
        /* 103: g */ {132, 45, 152, 198, 32}, /* 104: h */
        {32, 8, 194, 17, 192},
        /* 105: i */ {16, 12, 33, 8, 68},
        /* 106: j */ {132, 37, 76, 82, 64}, /* 107: k */
        {97, 8, 66, 17, 192},
        /* 108: l */ {0, 53, 90, 214, 160},
        /* 109: m */ {0, 45, 152, 198, 32}, /* 110: n */
        {0, 29, 24, 197, 192},
        /* 111: o */ {0, 61, 24, 250, 16},
        /* 112: p */ {0, 27, 56, 188, 33}, /* 113: q */
        {0, 45, 152, 66},
        /* 114: r */ {0, 29, 7, 7, 192},
        /* 115: s */ {66, 56, 132, 36, 192}, /* 116: t */
        {0, 35, 24, 205, 160},
        /* 117: u */ {0, 35, 24, 168, 128},
        /* 118: v */ {0, 35, 26, 213, 64}, /* 119: w */
        {0, 34, 162, 42, 32},
        /* 120: x */ {0, 35, 24, 188, 46},
        /* 121: y */ {0, 62, 34, 35, 224}, /* 122: z */
        {17, 8, 130, 16, 64},
        /* 123: { */ {33, 8, 66, 16, 128},
        /* 124: | */ {65, 8, 34, 17}, /* 125: } */
        {0, 0, 217}, /* 126: ~ */
};

/** Betűtípus kibontása **/
void UncompressFont() {
    Uint8 i;
    for(i = 33; i < 127; ++i) {
        memcpy(
            SpaceFont[i],
            CompressedFont[i - 33],
            5); /* Átmásolás a megfelelő tömörített helyről */
        UncompressPixelMap(SpaceFont[i], 40, 5); /* Helyben kibontás */
    }
}
