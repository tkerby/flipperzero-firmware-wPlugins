#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "bible_app.h"

/* ============================================================================
 * ActionMenu overlay view — 3-item menu over Reader
 * ============================================================================ */

/**
 * Draw the ActionMenu overlay: header, 3 items with inverted highlight.
 */
void bible_bsb_view_action_menu_draw(Canvas* canvas, void* ctx);

/**
 * Handle ActionMenu input: Up/Down=move selection, OK=execute, Back=return.
 */
bool bible_bsb_view_action_menu_input(InputEvent* event, void* ctx);
