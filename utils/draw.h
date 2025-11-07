#pragma once

#include <stdint.h>
#include <gui/canvas.h>

// Draws a line in the image buffer. The last two args are the image bounds
void ie_draw_line(uint8_t* buffer, int x1, int y1, int x2, int y2, int bw, int bh);

// Draws a circle in the image buffer. The last two args are the image bounds
// pt0 is center, pt1 is anywhere on the edge
void ie_draw_circle(uint8_t* buffer, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, int bw, int bh);

typedef enum {
    FontMicro,
    Font5x7
} IEFont;
int ie_draw_str(Canvas* canvas, int x, int y, Align h, Align v, IEFont font, const char* str);
uint16_t ie_draw_get_str_width(Canvas* canvas, IEFont font, const char* str);

void ie_draw_modal_panel_frame(Canvas* canvas, int x, int y, int w, int h);
