#include "graphics.h"

#define SCALE 10

/*
  Fontname: micro
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 18/128
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_micro_tn[148] =
    "\22\0\2\3\2\3\1\4\4\3\5\0\0\5\0\5\0\0\0\0\0\0w \4`\63*\10\67\62Q"
    "j\312\0+\7or\321\24\1,\5*r\3-\5\247\62\3.\5*\62\4/\10\67\262\251\60\12"
    "\1\60\10\67r)U\12\0\61\6\66rS\6\62\7\67\62r\224\34\63\7\67\62r$\22\64\7\67"
    "\62\221\212\14\65\7\67\62\244<\1\66\6\67r#E\67\10\67\62c*\214\0\70\6\67\62TE\71"
    "\7\67\62\24\71\1:\6\66\62$\1\0\0\0\4\377\377\0";

// TODO: allow points to be located outside the canvas. currently, the canvas_* methods
// choke on this in some cases, resulting in large vertical/horizontal lines
void gfx_draw_line(Canvas* canvas, float x1, float y1, float x2, float y2) {
    canvas_draw_line(
        canvas, roundf(x1 / SCALE), roundf(y1 / SCALE), roundf(x2 / SCALE), roundf(y2 / SCALE));
}

void gfx_draw_line(Canvas* canvas, const Vec2& p1, const Vec2& p2) {
    gfx_draw_line(canvas, p1.x, p1.y, p2.x, p2.y);
}

void gfx_draw_disc(Canvas* canvas, float x, float y, float r) {
    canvas_draw_disc(canvas, roundf(x / SCALE), roundf(y / SCALE), roundf(r / SCALE));
}
void gfx_draw_disc(Canvas* canvas, const Vec2& p, float r) {
    gfx_draw_disc(canvas, p.x, p.y, r);
}

void gfx_draw_circle(Canvas* canvas, float x, float y, float r) {
    canvas_draw_circle(canvas, roundf(x / SCALE), roundf(y / SCALE), roundf(r / SCALE));
}
void gfx_draw_circle(Canvas* canvas, const Vec2& p, float r) {
    gfx_draw_circle(canvas, p.x, p.y, r);
}

void gfx_draw_dot(Canvas* canvas, float x, float y) {
    canvas_draw_dot(canvas, roundf(x / SCALE), roundf(y / SCALE));
}
void gfx_draw_dot(Canvas* canvas, const Vec2& p) {
    gfx_draw_dot(canvas, p.x, p.y);
}

void gfx_draw_arc(Canvas* canvas, const Vec2& p, float r, float start, float end) {
    float adj_end = end;
    if(end < start) {
        adj_end += (float)M_PI * 2;
    }
    // initialize to start of arc
    float sx = p.x + r * cosf(start);
    float sy = p.y - r * sinf(start);
    size_t segments = r / 8;
    for(size_t i = 1; i <= segments; i++) { // for now, use r to determin number of segments
        float nx = p.x + r * cosf(start + i / (segments / (adj_end - start)));
        float ny = p.y - r * sinf(start + i / (segments / (adj_end - start)));
        gfx_draw_line(canvas, sx, sy, nx, ny);
        sx = nx;
        sy = ny;
    }
}

void gfx_draw_str(Canvas* canvas, int x, int y, Align h, Align v, const char* str) {
    canvas_set_custom_u8g2_font(canvas, u8g2_font_micro_tn);

    canvas_set_color(canvas, ColorWhite);
    int w = canvas_string_width(canvas, str);
    if(h == AlignRight) {
        canvas_draw_box(canvas, x - 1 - w, y, w + 2, 6);
    } else if(h == AlignLeft) {
        canvas_draw_box(canvas, x - 1, y, w + 2, 6);
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, x, y, h, v, str);

    canvas_set_font(canvas, FontSecondary); // reset?
}
