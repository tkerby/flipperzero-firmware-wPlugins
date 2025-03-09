#pragma once
#include <PicoDVI.h>                  // Core display & graphics library
#include <Fonts/FreeSansBold18pt7b.h> // A custom font
#include "vector.h"
#include "image.h"

// https://doc-tft-espi.readthedocs.io/tft_espi/colors/
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif

#ifndef TFT_RED
#define TFT_RED 0xF800
#endif

#ifndef TFT_WFITE
#define TFT_WHITE 0xFFFF
#endif

namespace VGMGameEngine
{
    class DisplayAdapter
    {
    public:
        DisplayAdapter(bool use_8bit = false);
        ~DisplayAdapter();
        bool begin();                                                                                       // Initializes the display.
        void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color); // Draws a bitmap on the display at the specified position with the specified color.
        void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);                                 // Draws a circle on the display at the specified position with the specified radius and color.
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h);       // Draws a grayscale bitmap on the display at the specified position.
        void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);                      // Draws a line on the display from (x0, y0) to (x1, y1) with the specified color.
        void drawPixel(int16_t x, int16_t y, uint16_t color);                                               // Draws a pixel on the display at the specified position.
        void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);                          // Draws a rectangle on the display at the specified position.
        void drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h);             // Draws a bitmap on the display at the specified position.
        void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);          // Draws a rounded rectangle on the display at the specified position.
        void fillScreen(uint16_t color);                                                                    // Fills the entire screen with the specified color.
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);                          // Fills a rectangle on the display with the specified color.
        uint16_t *getPalette();                                                                             // Returns the color palette.
        void print(const char *text);                                                                       // Prints text on the display.
        void setPalette(const uint16_t *palette);                                                           // Sets the color palette for the 8-bit display.
        void setColor(uint8_t idx, uint16_t color);                                                         // Sets the color for drawing.
        void setCursor(int16_t x, int16_t y);                                                               // Sets the cursor position for text rendering.
        void setFont(const GFXfont *font = NULL);                                                           // Sets the font for text rendering.
        void setTextSize(uint8_t size = 1);                                                                 // Sets the text size for rendering.
        void setTextColor(uint16_t color = TFT_WHITE);                                                      // Sets the text color for rendering.
        void swap(bool copy_framebuffer = false, bool copy_palette = false);                                // Swaps the display buffer.
        //
        DVIGFX16 *display16; // Invoke custom library
        DVIGFX8 *display8;   // Invoke custom library
    };

    // The Draw class is used to draw images and text on the display.
    class Draw
    {
    public:
        Draw(bool use_8bit = false);
        ~Draw();
        void background(uint16_t color);                                                   // Sets the background color of the display.
        void clear(Vector position, Vector size, uint16_t color);                          // Clears the display at the specified position and size with the specified color.
        void color(uint16_t color);                                                        // Sets the color for drawing.
        void image(Vector position, Image *image);                                         // Draws an image on the display at the specified position.
        void image(Vector position, const uint8_t *bitmap, Vector size);                   // Draws a bitmap on the display at the specified position.
        void text(Vector position, const char *text);                                      // Draws text on the display at the specified position.
        void text(Vector position, const char *text, uint16_t color);                      // Draws text on the display at the specified position with the specified font.
        void text(Vector position, const char *text, uint16_t color, const GFXfont *font); // Draws text on the display at the specified position with the specified font and color.
        Vector size;                                                                       // The size of the display.
        DisplayAdapter *display;                                                           // Invoke custom library
    private:
        bool is_8bit; // Flag to indicate if the display is 8-bit or not
    };
}