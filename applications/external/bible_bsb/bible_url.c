#include "bible_url.h"
#include "bible_books.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * route.bible URL builder
 *
 * Canonical URL format:
 *
 *   All verses:     https://route.bible/{book_lower}.{chapter}?v=BSB&src=flipper_bible_bsb
 *   Single verse:   https://route.bible/{book_lower}.{chapter}.{verse}?v=BSB&src=flipper_bible_bsb
 *   Verse range:    https://route.bible/{book_lower}.{chapter}.{start}-{book_lower}.{chapter}.{end}?v=BSB&src=flipper_bible_bsb
 *
 * Book names use lowercased OSIS codes (e.g., "gen", "jhn", "rev").
 * Used by ActionMenu NFC share (bible_view_action_menu.c) and
 * Collection NFC bulk export (bible_view_collection.c).
 * ============================================================================ */

bool bible_build_route_url(const BiblePassage* passage, char* out, size_t out_len) {
    furi_check(passage);
    furi_check(out);
    furi_check(out_len > 0);

    if(passage->verse_count == 0 || passage->book_index >= BIBLE_BOOK_COUNT) {
        strlcpy(out, "", out_len);
        return false;
    }

    const char* osis = OSIS_BOOK_CODES[passage->book_index];
    char book_lower[8];
    for(uint8_t i = 0; i < sizeof(book_lower) && osis[i] != '\0'; i++) {
        char c = osis[i];
        if(c >= 'A' && c <= 'Z') {
            book_lower[i] = c + ('a' - 'A');
        } else {
            book_lower[i] = c;
        }
        book_lower[i + 1] = '\0';
    }

    if(passage->start_verse > 0 && passage->end_verse > passage->start_verse) {
        /* Verse range */
        snprintf(
            out,
            out_len,
            "https://route.bible/%s.%u.%u-%s.%u.%u?v=BSB&src=flipper_bible_bsb",
            book_lower,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse,
            book_lower,
            (unsigned int)passage->chapter,
            (unsigned int)passage->end_verse);
    } else if(passage->start_verse > 0) {
        /* Single verse */
        snprintf(
            out,
            out_len,
            "https://route.bible/%s.%u.%u?v=BSB&src=flipper_bible_bsb",
            book_lower,
            (unsigned int)passage->chapter,
            (unsigned int)passage->start_verse);
    } else {
        /* All verses: no verse number appended */
        snprintf(
            out,
            out_len,
            "https://route.bible/%s.%u?v=BSB&src=flipper_bible_bsb",
            book_lower,
            (unsigned int)passage->chapter);
    }

    return true;
}
