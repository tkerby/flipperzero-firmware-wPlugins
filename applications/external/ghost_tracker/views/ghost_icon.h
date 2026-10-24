#pragma once

#include <gui/canvas.h>

/** Shared decorative ghost icon (used by the menu and the splash screen).
 * (cx, top_y) is the top-center of the bounding box, w x h its size. */
void ghost_icon_draw(Canvas* canvas, int cx, int top_y, int w, int h);
