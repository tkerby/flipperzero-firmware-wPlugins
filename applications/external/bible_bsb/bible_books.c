#include "bible_books.h"

#include <string.h>

/* ============================================================================
 * 22 filter options: "All" + 21 initial letters
 * ============================================================================ */
const char* BIBLE_FILTER_LABELS[BIBLE_FILTER_COUNT] = {
    "All", "A", "C", "D", "E", "G", "H", "I", "J", "K", "L",
    "M",   "N", "O", "P", "R", "S", "T", "Z", "1", "2", "3",
};

/* ============================================================================
 * OSIS book codes — 66 entries, index 0–65
 * ============================================================================ */
const char* OSIS_BOOK_CODES[BIBLE_BOOK_COUNT] = {
    "GEN", "EXO", "LEV", "NUM", "DEU", "JOS", "JDG", "RUT", "1SA", "2SA", "1KI",
    "2KI", "1CH", "2CH", "EZR", "NEH", "EST", "JOB", "PSA", "PRO", "ECC", "SNG",
    "ISA", "JER", "LAM", "EZK", "DAN", "HOS", "JOL", "AMO", "OBA", "JON", "MIC",
    "NAM", "HAB", "ZEP", "HAG", "ZEC", "MAL", "MAT", "MRK", "LUK", "JHN", "ACT",
    "ROM", "1CO", "2CO", "GAL", "EPH", "PHP", "COL", "1TH", "2TH", "1TI", "2TI",
    "TIT", "PHM", "HEB", "JAS", "1PE", "2PE", "1JN", "2JN", "3JN", "JUD", "REV",
};

/* ============================================================================
 * Book display names — 66 entries, index 0–65
 * ============================================================================ */
const char* OSIS_BOOK_NAMES[BIBLE_BOOK_COUNT] = {
    "Genesis",
    "Exodus",
    "Leviticus",
    "Numbers",
    "Deuteronomy",
    "Joshua",
    "Judges",
    "Ruth",
    "1 Samuel",
    "2 Samuel",
    "1 Kings",
    "2 Kings",
    "1 Chronicles",
    "2 Chronicles",
    "Ezra",
    "Nehemiah",
    "Esther",
    "Job",
    "Psalms",
    "Proverbs",
    "Ecclesiastes",
    "Song of Solomon",
    "Isaiah",
    "Jeremiah",
    "Lamentations",
    "Ezekiel",
    "Daniel",
    "Hosea",
    "Joel",
    "Amos",
    "Obadiah",
    "Jonah",
    "Micah",
    "Nahum",
    "Habakkuk",
    "Zephaniah",
    "Haggai",
    "Zechariah",
    "Malachi",
    "Matthew",
    "Mark",
    "Luke",
    "John",
    "Acts",
    "Romans",
    "1 Corinthians",
    "2 Corinthians",
    "Galatians",
    "Ephesians",
    "Philippians",
    "Colossians",
    "1 Thessalonians",
    "2 Thessalonians",
    "1 Timothy",
    "2 Timothy",
    "Titus",
    "Philemon",
    "Hebrews",
    "James",
    "1 Peter",
    "2 Peter",
    "1 John",
    "2 John",
    "3 John",
    "Jude",
    "Revelation",
};

/* ============================================================================
 * Chapter counts per book — 66 entries, index 0–65
 * ============================================================================ */
const uint8_t BOOK_CHAPTER_COUNTS[BIBLE_BOOK_COUNT] = {
    50, 40, 27, 36, 34, 24, 21, 4, 31, 24, 22, 25, 29, 36, 10, 13, 10, 42, 150, 31, 12, 8,
    66, 52, 5,  48, 12, 14, 3,  9, 1,  4,  7,  3,  3,  3,  2,  14, 4,  28, 16,  24, 21, 28,
    16, 16, 13, 6,  6,  4,  4,  5, 3,  6,  4,  3,  1,  13, 5,  5,  3,  5,  1,   1,  1,  22,
};

/* ============================================================================
 * Genesis max-verse table — chapters 1–10
 * ============================================================================ */
const uint8_t GENESIS_MAX_VERSES[10] = {
    31,
    25,
    24,
    26,
    32,
    22,
    24,
    22,
    29,
    32,
};

/* ============================================================================
 * Psalms max-verse table — all 150 chapters
 * ============================================================================ */
const uint8_t PSALMS_MAX_VERSES[150] = {
    6,  12, 8,  8,  12, 10, 17, 9,  20,  18, 7,  8,  6,  7,  5,  11, 15, 50, 14, 9,  13, 31,
    6,  10, 22, 12, 14, 9,  11, 12, 24,  11, 22, 22, 28, 12, 40, 22, 13, 17, 13, 11, 5,  26,
    17, 11, 9,  14, 20, 23, 19, 9,  6,   7,  23, 13, 11, 11, 17, 12, 8,  12, 11, 10, 13, 20,
    7,  35, 36, 5,  24, 20, 28, 23, 10,  12, 20, 72, 13, 19, 16, 8,  18, 12, 13, 17, 7,  18,
    52, 17, 16, 15, 5,  23, 11, 13, 12,  9,  9,  5,  8,  28, 22, 35, 45, 48, 43, 13, 31, 7,
    10, 10, 9,  8,  18, 19, 2,  29, 176, 7,  8,  9,  4,  8,  5,  6,  5,  6,  8,  8,  3,  18,
    3,  3,  21, 26, 9,  8,  24, 13, 10,  7,  12, 15, 21, 10, 20, 14, 9,  6,
};

/* ============================================================================
 * John max-verse table — all 21 chapters
 * ============================================================================ */
const uint8_t JOHN_MAX_VERSES[21] = {
    51, 25, 36, 54, 47, 71, 53, 59, 41, 42, 57, 50, 38, 31, 27, 33, 26, 40, 42, 31, 25,
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

uint16_t max_verse_for_chapter(uint8_t book_idx, uint16_t chapter) {
    if(chapter == 0) {
        return 1;
    }

    if(book_idx == 0) {
        /* Genesis */
        if(chapter <= 10) {
            return GENESIS_MAX_VERSES[chapter - 1];
        }
        return 30;
    }

    if(book_idx == 18) {
        /* Psalms */
        if(chapter <= 150) {
            return PSALMS_MAX_VERSES[chapter - 1];
        }
        return 10;
    }

    if(book_idx == 42) {
        /* John */
        if(chapter <= 21) {
            return JOHN_MAX_VERSES[chapter - 1];
        }
        return 20;
    }

    /* Default fallback for all other books */
    return 40;
}

uint8_t get_filtered_books(uint8_t filter_idx, uint8_t* out_indices) {
    if(filter_idx >= BIBLE_FILTER_COUNT) {
        return 0;
    }

    /* "All" filter — return every book */
    if(filter_idx == 0) {
        for(uint8_t i = 0; i < BIBLE_BOOK_COUNT; i++) {
            out_indices[i] = i;
        }
        return BIBLE_BOOK_COUNT;
    }

    const char* filter_label = BIBLE_FILTER_LABELS[filter_idx];
    char filter_char = filter_label[0];

    uint8_t count = 0;
    for(uint8_t i = 0; i < BIBLE_BOOK_COUNT; i++) {
        const char* name = OSIS_BOOK_NAMES[i];
        char first_char = name[0];

        /* Numeric filters (1, 2, 3) match books starting with a digit */
        if(filter_char >= '1' && filter_char <= '3') {
            if(first_char >= '1' && first_char <= '3') {
                out_indices[count++] = i;
            }
        } else {
            if(first_char == filter_char) {
                out_indices[count++] = i;
            }
        }
    }

    return count;
}
