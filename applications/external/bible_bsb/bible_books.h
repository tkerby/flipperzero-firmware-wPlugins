#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Bible metadata: 66-book canon
 *
 * All arrays are indexable 0-65.
 * ============================================================================ */

#define BIBLE_BOOK_COUNT 66
#define OT_COUNT         39 /* Malachi is index 38, Matthew is index 39 */

/* --------------------------------------------------------------------------
 * 22 filter options: "All" + 21 initial letters
 * -------------------------------------------------------------------------- */
#define BIBLE_FILTER_COUNT 22

extern const char* BIBLE_FILTER_LABELS[BIBLE_FILTER_COUNT];

/* --------------------------------------------------------------------------
 * Static const arrays — one entry per book
 * -------------------------------------------------------------------------- */
extern const char* OSIS_BOOK_CODES[BIBLE_BOOK_COUNT];
extern const char* OSIS_BOOK_NAMES[BIBLE_BOOK_COUNT];
extern const uint8_t BOOK_CHAPTER_COUNTS[BIBLE_BOOK_COUNT];

/* --------------------------------------------------------------------------
 * Max-verse tables (hardcoded for Genesis, Psalms, John)
 * -------------------------------------------------------------------------- */

/* Genesis chapters 1–10 (fallback 30) */
extern const uint8_t GENESIS_MAX_VERSES[10];

/* Psalms — all 150 chapters */
extern const uint8_t PSALMS_MAX_VERSES[150];

/* John chapters 1–21 (fallback 20) */
extern const uint8_t JOHN_MAX_VERSES[21];

/* --------------------------------------------------------------------------
 * Lookup helpers
 * -------------------------------------------------------------------------- */

/**
 * Return the maximum verse count for a given book and chapter.
 * Falls back to 30 for Genesis (unknown ch), 10 for Psalms (unknown ch),
 * 20 for John (unknown ch), and 40 for all other books.
 */
uint16_t max_verse_for_chapter(uint8_t book_idx, uint16_t chapter);

/**
 * Fill `out_indices` with book indices matching the given filter.
 *
 * @param filter_idx  0–21 index into BIBLE_FILTER_LABELS
 * @param out_indices caller-allocated array of at least BIBLE_BOOK_COUNT uint8_t entries
 * @return number of matching books written into out_indices
 */
uint8_t get_filtered_books(uint8_t filter_idx, uint8_t* out_indices);
