#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "bible_app.h"

/* ============================================================================
 * Reader view — paginated scripture display
 * ============================================================================ */

/**
 * Draw the Reader view: header with passage ref, divider, paginated text.
 */
void bible_bsb_view_reader_draw(Canvas* canvas, void* ctx);

/**
 * Handle Reader input: Up/Down=verse jump, Left/Right=scroll, OK=ActionMenu/quick-save, Back=return.
 */
bool bible_bsb_view_reader_input(InputEvent* event, void* ctx);
