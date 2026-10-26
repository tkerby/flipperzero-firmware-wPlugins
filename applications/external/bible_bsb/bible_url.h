#pragma once

#include "bible_state.h"

/* ============================================================================
 * route.bible URL builder
 *
 * Full implementation will be provided by the route-url-builder feature.
 * This header defines the contract so ActionMenu can call
 * bible_build_route_url() today.
 * ============================================================================ */

/**
 * Build a canonical route.bible URL for the given passage.
 *
 * Single verse:
 *   https://route.bible/{book_lower}.{chapter}.{verse}?v=BSB&src=flipper_bible_bsb
 *
 * Verse range:
 *   https://route.bible/{book_lower}.{chapter}.{start}-{book_lower}.{chapter}.{end}?v=BSB&src=flipper_bible_bsb
 *
 * @param passage  the passage to build a URL for
 * @param out      output buffer (must be at least 128 bytes)
 * @param out_len  capacity of out buffer
 * @return true if URL was written successfully
 */
bool bible_build_route_url(const BiblePassage* passage, char* out, size_t out_len);
