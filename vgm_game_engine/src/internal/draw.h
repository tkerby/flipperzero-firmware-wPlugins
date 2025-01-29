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
    // The Draw class is used to draw images and text on the display.
    class Draw
    {
    public:
        Draw();
        ~Draw();
        void background(uint16_t color);                                                   // Sets the background color of the display.
        void clear(Vector position, Vector size, uint16_t color);                          // Clears the display at the specified position and size with the specified color.
        void color(uint16_t color);                                                        // Sets the color for drawing.
        void image(Vector position, Image *image);                                         // Draws an image on the display at the specified position.
        void text(Vector position, const char *text);                                      // Draws text on the display at the specified position.
        void text(Vector position, const char *text, uint16_t color);                      // Draws text on the display at the specified position with the specified font.
        void text(Vector position, const char *text, uint16_t color, const GFXfont *font); // Draws text on the display at the specified position with the specified font and color.
        Vector size;                                                                       // The size of the display.
        DVIGFX16 *display;                                                                 // Invoke custom library
    };
}