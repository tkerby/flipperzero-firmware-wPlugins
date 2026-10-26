#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <gui/canvas.h>

#include "bible_state.h"

/* ============================================================================
 * Canvas layout constants
 * ============================================================================ */
#define BIBLE_CHAR_WIDTH    6
#define BIBLE_CHAR_HEIGHT   8
#define BIBLE_LINE_HEIGHT   12
#define BIBLE_SCREEN_WIDTH  128
#define BIBLE_SCREEN_HEIGHT 64
#define BIBLE_MARGIN_X      2
#define BIBLE_MARGIN_Y      2

/* Max characters that fit on one line */
#define BIBLE_MAX_CHARS_PER_LINE ((BIBLE_SCREEN_WIDTH - BIBLE_MARGIN_X * 2) / BIBLE_CHAR_WIDTH)

/* ============================================================================
 * Word wrapping
 * ============================================================================ */

/**
 * Wrap a single verse's text into display lines.
 *
 * Splits on whitespace. The first line is prefixed with "{verse_number} "
 * and marked is_verse_number=true. All produced lines get verse_number set.
 *
 * @param text          verse text to wrap
 * @param verse_number  verse number to prefix and tag lines with
 * @param max_chars     max chars per line (typically BIBLE_MAX_CHARS_PER_LINE)
 * @param out_lines     output array (caller-allocated)
 * @param out_count     [in/out] current line count; incremented on success
 * @param max_lines     capacity of out_lines array
 */
void bible_word_wrap(
    const char* text,
    uint16_t verse_number,
    uint8_t max_chars,
    BibleLine* out_lines,
    uint16_t* out_count,
    uint16_t max_lines);

/* ============================================================================
 * Scroll indicator
 * ============================================================================ */

/**
 * Draw a 2px-wide scroll thumb on the right edge (x=126).
 *
 * @param canvas        canvas handle
 * @param scroll        current scroll offset (first visible item index)
 * @param total         total number of items
 * @param visible       number of items visible at once
 * @param header_height pixel height of header area (thumb drawn below this)
 */
void bible_draw_scroll_indicator(
    Canvas* canvas,
    uint16_t scroll,
    uint16_t total,
    uint16_t visible,
    uint8_t header_height);

/* ============================================================================
 * Toast overlay
 * ============================================================================ */

/**
 * Draw a toast as a centered black box with white text.
 *
 * Only draws if toast->timer > 0.
 *
 * @param canvas  canvas handle
 * @param toast   toast state (message + timer)
 */
void bible_draw_toast(Canvas* canvas, const BibleToast* toast);

/* ============================================================================
 * Page rendering and pagination
 * ============================================================================ */

/**
 * Render a page of lines starting at scroll_offset.
 *
 * @param canvas         canvas handle
 * @param lines          array of BibleLine
 * @param line_count     number of lines
 * @param scroll_offset  first visible line index
 * @param y_offset       top pixel where text begins (e.g. 16, below header)
 */
void bible_render_page(
    Canvas* canvas,
    const BibleLine* lines,
    uint16_t line_count,
    uint16_t scroll_offset,
    int16_t y_offset);

/**
 * Max visible lines for a given y_offset.
 */
uint16_t bible_max_visible_lines(int16_t y_offset);

/**
 * Calculate total pages given lines and visible lines count.
 *
 * @param lines       array of BibleLine
 * @param line_count  number of lines
 * @param y_offset    top pixel where text begins
 * @return total page count (at least 1)
 */
uint16_t bible_total_pages(const BibleLine* lines, uint16_t line_count, int16_t y_offset);

/* ============================================================================
 * Inverted highlight
 * ============================================================================ */

/**
 * Draw an inverted highlight: black filled box with white text.
 *
 * Restores canvas color to black before returning.
 *
 * @param canvas  canvas handle
 * @param x       box x position
 * @param y       box top y position
 * @param w       box width
 * @param h       box height
 * @param text    text to draw (null-terminated)
 * @param text_x  text x position
 * @param text_y  text baseline y position
 */
/**
 * Full-screen notice shown when the bundled chapter pack is not installed.
 */
void bible_draw_missing_data(Canvas* canvas);

void bible_draw_inverted_highlight(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    const char* text,
    uint8_t text_x,
    uint8_t text_y);
