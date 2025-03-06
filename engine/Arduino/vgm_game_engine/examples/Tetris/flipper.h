#pragma once
#include <Arduino.h>
#include <stdint.h>

// #define PicoEngine
#define VGMEngine

#ifdef PicoEngine
#include <PicoGameEngine.h>
#else
#include <VGMGameEngine.h>
#endif

#define furi_assert(x) assert(x)
#define furi_delay_ms(x) delay(x)
#define furi_get_tick() (millis() / 10)
#define furi_kernel_get_tick_frequency() 200
#define furi_hal_random_get() random(256)
void furi_hal_random_fill_buf(void *buffer, size_t len);

typedef enum
{
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} FlipperAlign;

typedef enum
{
    ColorWhite = 0x00,
    ColorBlack = 0x01,
    ColorXOR = 0x02,
} FlipperColor;

typedef enum
{
    FontPrimary,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers,
} FlipperFont;

typedef enum
{
    InputKeyRight = BUTTON_RIGHT,
    InputKeyLeft = BUTTON_LEFT,
    InputKeyUp = BUTTON_UP,
    InputKeyDown = BUTTON_DOWN,
    InputKeyOk = BUTTON_CENTER,
    InputKeyBack = BUTTON_BACK
} FlipperInputKey;

// https://doc-tft-espi.readthedocs.io/tft_espi/colors/
#ifndef TFT_DARKCYAN
#define TFT_DARKCYAN 0x03EF
#endif
#ifndef TFT_DARKGREEN
#define TFT_DARKGREEN 0x03E0
#endif
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_BLUE
#define TFT_BLUE 0x001F
#endif
#ifndef TFT_RED
#define TFT_RED 0xF800
#endif
#ifndef TFT_SKYBLUE
#define TFT_SKYBLUE 0x867D
#endif
#ifndef TFT_VIOLET
#define TFT_VIOLET 0x915C
#endif
#ifndef TFT_BROWN
#define TFT_BROWN 0x9A60
#endif
#ifndef TFT_TRANSPARENT
#define TFT_TRANSPARENT 0x0120
#endif
#ifndef TFT_YELLOW
#define TFT_YELLOW 0xFFE0
#endif
#ifndef TFT_ORANGE
#define TFT_ORANGE 0xFDA0
#endif
#ifndef TFT_PINK
#define TFT_PINK 0xFE19
#endif

#ifndef FLIPPER_SCREEN_WIDTH
#define FLIPPER_SCREEN_WIDTH 128
#endif
#ifndef FLIPPER_SCREEN_HEIGHT
#define FLIPPER_SCREEN_HEIGHT 64
#endif

#ifndef FLIPPER_SCREEN_SIZE
#define FLIPPER_SCREEN_SIZE Vector(FLIPPER_SCREEN_WIDTH, FLIPPER_SCREEN_HEIGHT)
#endif

typedef Draw Canvas;

void canvas_clear(Canvas *canvas, uint16_t color = TFT_WHITE);
size_t canvas_current_font_height(const Canvas *canvas);
void canvas_draw_box(Canvas *canvas, int32_t x, int32_t y, size_t width, size_t height, uint16_t color = TFT_BLACK);
void canvas_draw_dot(Canvas *canvas, int32_t x, int32_t y, uint16_t color = TFT_BLACK);
void canvas_draw_frame(Canvas *canvas, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color = TFT_BLACK);
void canvas_draw_icon(Canvas *canvas, int32_t x, int32_t y, const uint8_t *icon, int32_t w, int32_t h, uint16_t color = TFT_BLACK);
void canvas_draw_line(Canvas *canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t color = TFT_BLACK);
void canvas_draw_rframe(Canvas *canvas, int32_t x, int32_t y, size_t width, size_t height, size_t radius, uint16_t color = TFT_BLACK);
void canvas_draw_str(Canvas *canvas, int32_t x, int32_t y, const char *str, uint16_t color = TFT_BLACK);
void canvas_draw_str_aligned(Canvas *canvas, int32_t x, int32_t y, int32_t align_x, int32_t align_y, const char *str, uint16_t color = TFT_BLACK);
size_t canvas_height(const Canvas *canvas);
void canvas_set_bitmap_mode(Canvas *canvas, bool alpha);
void canvas_set_color(Canvas *canvas, FlipperColor color);
void canvas_set_font(Canvas *canvas, FlipperFont font);
uint16_t canvas_string_width(Canvas *canvas, const char *str);
size_t canvas_width(const Canvas *canvas);