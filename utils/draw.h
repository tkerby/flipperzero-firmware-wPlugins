#pragma once

#include <stdint.h>
#include <gui/canvas.h>

void ie_draw_line(uint8_t* buffer, int x1, int y1, int x2, int y2);
// pt0 is center, pt1 is on the edge
void ie_draw_circle(uint8_t* buffer, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

typedef enum {
    FontMicro,
    Font5x7
} IEFont;
void ie_draw_str(Canvas* canvas, int x, int y, Align h, Align v, IEFont font, const char* str);
