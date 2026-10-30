#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKB_GB_TERMINATOR 0x50 /* "@" */

/*
 * Encode ASCII into the Game Boy (English) charset, '@'-terminated and padded
 * with terminators to `len` bytes. Unmappable characters become '?'. At most
 * len - 1 characters are encoded so the result is always terminated.
 */
void pkb_gb_encode(const char* ascii, uint8_t* out, size_t len);

/*
 * Decode up to `len` Game Boy charset bytes (stopping at '@') into a
 * NUL-terminated ASCII string of at most out_size - 1 characters. Gender
 * symbols become 'M'/'F'; unknown glyphs become '?'.
 */
void pkb_gb_decode(const uint8_t* gb, size_t len, char* out, size_t out_size);

#ifdef __cplusplus
}
#endif
