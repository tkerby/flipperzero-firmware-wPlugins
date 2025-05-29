#include <furi.h>
#include <math.h>
#include "draw.h"

void ie_draw_pixel(uint8_t* buffer, int8_t x, int8_t y) {
    if(x < 0 || x >= 10) return;
    if(y < 0 || y >= 10) return;
    buffer[y * 10 + x] = 1;
}

void ie_draw_line(uint8_t* buffer, int x1, int y1, int x2, int y2) {
    uint8_t tmp;
    uint8_t x, y;
    uint8_t dx, dy;
    int8_t err;
    int8_t ystep;

    uint8_t swapxy = 0;

    /* no intersection check at the moment, should be added... */
    if(x1 > x2)
        dx = x1 - x2;
    else
        dx = x2 - x1;
    if(y1 > y2)
        dy = y1 - y2;
    else
        dy = y2 - y1;

    if(dy > dx) {
        swapxy = 1;
        tmp = dx;
        dx = dy;
        dy = tmp;
        tmp = x1;
        x1 = y1;
        y1 = tmp;
        tmp = x2;
        x2 = y2;
        y2 = tmp;
    }
    if(x1 > x2) {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    err = dx >> 1;
    if(y2 > y1)
        ystep = 1;
    else
        ystep = -1;
    y = y1;

#ifndef U8G2_16BIT
    if(x2 == 255) x2--;
#else
    if(x2 == 0xffff) x2--;
#endif

    for(x = x1; x <= x2; x++) {
        if(swapxy == 0) {
            ie_draw_pixel(buffer, x, y);
        } else {
            ie_draw_pixel(buffer, y, x);
        }
        err -= (uint8_t)dy;
        if(err < 0) {
            y += (uint8_t)ystep;
            err += (uint8_t)dx;
        }
    }
}

#define DRAW_UPPER_RIGHT 0x01
#define DRAW_UPPER_LEFT  0x02
#define DRAW_LOWER_LEFT  0x04
#define DRAW_LOWER_RIGHT 0x08
#define DRAW_ALL         (DRAW_UPPER_RIGHT | DRAW_UPPER_LEFT | DRAW_LOWER_LEFT | DRAW_LOWER_RIGHT)

void ie_draw_circle_section(
    uint8_t* buffer,
    uint8_t x,
    uint8_t y,
    uint8_t x0,
    uint8_t y0,
    uint8_t option) {
    /* upper right */
    if(option & DRAW_UPPER_RIGHT) {
        ie_draw_pixel(buffer, x0 + x, y0 - y);
        ie_draw_pixel(buffer, x0 + y, y0 - x);
    }

    /* upper left */
    if(option & DRAW_UPPER_LEFT) {
        ie_draw_pixel(buffer, x0 - x, y0 - y);
        ie_draw_pixel(buffer, x0 - y, y0 - x);
    }

    /* lower right */
    if(option & DRAW_LOWER_RIGHT) {
        ie_draw_pixel(buffer, x0 + x, y0 + y);
        ie_draw_pixel(buffer, x0 + y, y0 + x);
    }

    /* lower left */
    if(option & DRAW_LOWER_LEFT) {
        ie_draw_pixel(buffer, x0 - x, y0 + y);
        ie_draw_pixel(buffer, x0 - y, y0 + x);
    }
}

void ie_draw_circle(uint8_t* buffer, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    int8_t f;
    int8_t ddF_x;
    int8_t ddF_y;
    uint8_t x;
    uint8_t y;

    int8_t dx = x0 - x1;
    int8_t dy = y0 - y1;
    uint8_t rad = (uint8_t)sqrt(dx * dx + dy * dy);

    f = 1;
    f -= rad;
    ddF_x = 1;
    ddF_y = 0;
    ddF_y -= rad;
    ddF_y *= 2;
    x = 0;
    y = rad;
    ie_draw_circle_section(buffer, x, y, x0, y0, DRAW_ALL);

    while(x < y) {
        if(f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        ie_draw_circle_section(buffer, x, y, x0, y0, DRAW_ALL);
    }
}

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

/*                                                                                                                                                                                            
  Fontname: -Misc-Fixed-Medium-R-Normal--7-70-75-75-C-50-ISO10646-1                                                                                                                           
  Copyright: Public domain font.  Share and enjoy.                                                                                                                                            
  Glyphs: 95/1848                                                                                                                                                                             
  BBX Build Mode: 0                                                                                                                                                                           
*/
const uint8_t u8g2_font_5x7_tr[804] =
    "_\0\2\2\3\3\3\4\4\5\7\0\377\6\377\6\0\1\12\2\26\3\7 \5\0\275\1!\6\261\261"
    "\31)\42\7[\267IV\0#\12-\261\253\206\252\206\252\0$\11-\261[\365Ni\1%\10\64\261"
    "\311\261w\0&\11,\261\213)V\61\5'\5\231\267\31(\7r\261S\315\0)\10r\261\211\251R"
    "\0*\7k\261I\325j+\12-\261\315(\16\231Q\4,\7[\257S%\0-\6\14\265\31\1."
    "\6R\261\31\1/\7$\263\217m\0\60\10s\261\253\134\25\0\61\7s\261K\262\65\62\11\64\261S"
    "\61\307r\4\63\12\64\261\31\71i$\223\2\64\12\64\261\215\252\32\61'\0\65\12\64\261\31z#\231"
    "\24\0\66\12\64\261SyE\231\24\0\67\12\64\261\31\71\346\230#\0\70\12\64\261S\61\251(\223\2"
    "\71\12\64\261SQ\246\235\24\0:\7j\261\31q\4;\10\63\257\263\221*\1<\7k\261Mu\1"
    "=\10\34\263\31\31\215\0>\7k\261\311U\11\77\11s\261k\246\14\23\0@\11\64\261SQ\335H"
    "\1A\11\64\261SQ\216)\3B\12\64\261Yq\244(G\2C\11\64\261SQ\227I\1D\11\64"
    "\261Y\321\71\22\0E\11\64\261\31z\345<\2F\10\64\261\31z\345\32G\11\64\261SQ\247\231\6"
    "H\10\64\261\211rL\63I\7s\261Y\261\65J\10\64\261o\313\244\0K\12\64\261\211*I\231\312"
    "\0L\7\64\261\311\335#M\11\64\261\211\343\210f\0N\10\64\261\211k\251\63O\11\64\261S\321\231"
    "\24\0P\12\64\261YQ\216\224\63\0Q\12<\257S\321\134I\243\0R\11\64\261YQ\216\324\14S"
    "\12\64\261S\61eT&\5T\7s\261Y\261\13U\10\64\261\211\236I\1V\11\64\261\211\316$\25"
    "\0W\11\64\261\211\346\70b\0X\12\64\261\211\62I\25e\0Y\10s\261IVY\1Z\11\64\261"
    "\31\71\266G\0[\7s\261\31\261\71\134\11$\263\311(\243\214\2]\7s\261\231\315\21^\5S\271"
    "k_\6\14\261\31\1`\6R\271\211\1a\10$\261\33Q\251\2b\12\64\261\311yE\71\22\0c"
    "\6#\261\233Yd\10\64\261\257F\224ie\10$\261Sid\5f\11\64\261\255\312\231#\0g\11"
    ",\257\33\61\251\214\6h\10\64\261\311yE\63i\10s\261\313HV\3j\11{\257\315\260T\25\0"
    "k\11\64\261\311U\222\251\14l\7s\261\221]\3m\10$\261IiH\31n\7$\261Y\321\14o"
    "\10$\261SQ&\5p\11,\257YQ\216\224\1q\10,\257\33Q\246\35r\10$\261YQg\0"
    "s\10$\261\33\32\15\5t\11\64\261\313q\346\214\4u\7$\261\211f\32v\7c\261IV\5w"
    "\7$\261\211r\34x\10$\261\211I\252\30y\11,\257\211\62\225%\0z\10$\261\31\261\34\1{"
    "\10s\261MI\326\1|\5\261\261\71}\11s\261\311Q\305\24\1~\7\24\271K*\1\0\0\0\4"
    "\377\377\0";

void ie_draw_str(Canvas* canvas, int x, int y, Align h, Align v, IEFont font, const char* str) {
    if(font == FontMicro) {
        canvas_set_custom_u8g2_font(canvas, u8g2_font_micro_tn);
    } else if(font == Font5x7) {
        canvas_set_custom_u8g2_font(canvas, u8g2_font_5x7_tr);
    } else {
        furi_assert(false);
        FURI_LOG_E("IEDraw", "Invalid font!");
    }

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
