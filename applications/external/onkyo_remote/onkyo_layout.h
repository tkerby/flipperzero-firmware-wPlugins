#pragma once

#include <gui/canvas.h>

#include "onkyo_commands.h"

/**
 * The view port runs in portrait, so the canvas is 64x128. The button grid is
 * taller than that and scrolls; see docs/SPECIFICATION.md section 3.2 for the
 * geometry this module implements.
 */
#define ONKYO_SCREEN_WIDTH   64
#define ONKYO_SCREEN_HEIGHT  128
#define ONKYO_CONTENT_HEIGHT 214
#define ONKYO_SCROLL_MAX     (ONKYO_CONTENT_HEIGHT - ONKYO_SCREEN_HEIGHT)

/** Everything the draw callback needs. Guarded by the app's mutex. */
typedef struct {
    uint8_t row; /**< focused row */
    uint8_t col; /**< focused column within that row */
    uint8_t sticky_col; /**< column to return to after a single-column row */
    uint8_t scroll; /**< content-space y of the top of the screen */
    bool sending; /**< a transmission is in flight */
} OnkyoViewModel;

/** Focus the first button, scrolled to the top. */
void onkyo_layout_init(OnkyoViewModel* model);

/** Move focus by one row or one column, scrolling it back into view. */
void onkyo_layout_move(OnkyoViewModel* model, int8_t d_row, int8_t d_col);

/** The command the focused button sends. */
OnkyoCmdId onkyo_layout_focused_command(const OnkyoViewModel* model);

/** Draw the whole grid. Runs on the GUI thread. */
void onkyo_layout_draw(Canvas* canvas, const OnkyoViewModel* model);
