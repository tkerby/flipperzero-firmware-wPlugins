#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Bundled chapter pack (assets/bsb.pack)
 *
 * Little-endian header (12 bytes):
 *   char magic[4]          "BSB1"
 *   uint16 count
 *   uint16 record_size     must be BIBLE_PACK_RECORD_SIZE
 *   uint8  window_sz2      heatshrink window, 4..BIBLE_PACK_MAX_WINDOW
 *   uint8  lookahead_sz2   heatshrink lookahead, less than window_sz2
 *   uint16 reserved
 *
 * Then `count` records of BIBLE_PACK_RECORD_SIZE bytes, sorted by
 * (book, chapter):
 *   uint8  book
 *   uint8  chapter
 *   uint16 comp_size
 *   uint16 raw_size
 *   uint32 data_offset     absolute offset of the heatshrink payload
 *
 * Payloads are raw heatshrink streams (no extra header). The decoder
 * window and lookahead come from the pack header.
 * ============================================================================ */

#define BIBLE_PACK_MAGIC_0     'B'
#define BIBLE_PACK_MAGIC_1     'S'
#define BIBLE_PACK_MAGIC_2     'B'
#define BIBLE_PACK_MAGIC_3     '1'
#define BIBLE_PACK_HEADER_SIZE 12
#define BIBLE_PACK_RECORD_SIZE 10
#define BIBLE_PACK_MAX_WINDOW  13
#define BIBLE_PACK_HS_INPUT    256

typedef struct {
    uint8_t book;
    uint8_t chapter;
    uint16_t comp_size;
    uint16_t raw_size;
    uint32_t data_offset;
} BiblePackEntry;

/**
 * Look up one chapter in a pack image.
 *
 * `pack_len` must cover the header and the record table. Payload bytes
 * past the table are ignored, so a caller may pass just the directory.
 *
 * @return true when the chapter is present and its sizes are non-zero
 */
bool bible_pack_find(
    const uint8_t* pack,
    size_t pack_len,
    uint8_t book,
    uint8_t chapter,
    BiblePackEntry* out);
