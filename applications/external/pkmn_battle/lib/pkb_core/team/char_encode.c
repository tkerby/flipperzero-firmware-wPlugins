/* ASCII <-> Game Boy charset, using tables generated from constants/charmap.asm. */
#include <pkb/charset.h>
#include "pb_tables.h"

#define GB_QUESTION 0xE6

void pkb_gb_encode(const char* ascii, uint8_t* out, size_t len) {
    size_t i = 0;
    if(len == 0) return;
    for(; ascii && ascii[i] && i < len - 1; i++) {
        unsigned char c = (unsigned char)ascii[i];
        uint8_t gb = c < 128 ? pb_ascii_to_gb[c] : 0;
        out[i] = gb ? gb : GB_QUESTION;
    }
    for(; i < len; i++) out[i] = PKB_GB_TERMINATOR;
}

void pkb_gb_decode(const uint8_t* gb, size_t len, char* out, size_t out_size) {
    /* 'd 'l 'm 'r 's 't 'v ligatures occupy $D0-$D6 */
    static const char ligature[] = "dlmrstv";
    size_t o = 0;
    if(out_size == 0) return;
    for(size_t i = 0; i < len && gb[i] != PKB_GB_TERMINATOR; i++) {
        char c = pb_gb_to_ascii[gb[i]];
        if(o + 1 >= out_size) break;
        out[o++] = c;
        if(gb[i] >= 0xD0 && gb[i] <= 0xD6 && o + 1 < out_size) {
            out[o++] = ligature[gb[i] - 0xD0];
        }
    }
    out[o] = '\0';
}
