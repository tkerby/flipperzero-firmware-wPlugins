#include "draw.h"
namespace VGMGameEngine
{
    uint16_t frameBuffer[320 * 240];
    Draw::Draw()
    {
        this->size = Vector(320, 240);
        this->display = new DVIGFX16(DVI_RES_320x240p60, picodvi_dvi_cfg);

        if (!this->display->begin())
            return;

        // this->display->fillScreen(0); // Start by clearing the screen; color 0 = black
        this->display->setFont();
        this->display->setTextSize(1);
    }

    Draw::~Draw()
    {
        delete this->display;
    }

    void Draw::background(uint16_t color)
    {
        this->display->fillScreen(color);
    }

    void Draw::clear(Vector position, Vector size, uint16_t color)
    {
        // Calculate the clipping boundaries
        int x = position.x;
        int y = position.y;
        int width = size.x;
        int height = size.y;

        // Adjust for left and top boundaries
        if (x < 0)
        {
            width += x; // Reduce width by the negative offset
            x = 0;
        }
        if (y < 0)
        {
            height += y; // Reduce height by the negative offset
            y = 0;
        }

        // Adjust for right and bottom boundaries
        if (x + width > this->size.x)
        {
            width = this->size.x - x;
        }
        if (y + height > this->size.y)
        {
            height = this->size.y - y;
        }

        // Ensure width and height are positive before drawing
        if (width > 0 && height > 0)
        {
            this->display->fillRect(x, y, width, height, color);
        }
    }

    void Draw::color(uint16_t color)
    {
        this->display->setTextColor(color);
    }

    void Draw::image(Vector position, Image *image)
    {
        if (image->buffer != nullptr &&
            position.x < this->size.x &&
            position.y < this->size.y &&
            image->size.x > 0 &&
            image->size.y > 0)
        {
            // image does not have a color property so we should use another method
            this->display->drawRGBBitmap(position.x, position.y, image->buffer, image->size.x, image->size.y);
        }
    }

    void Draw::text(Vector position, const char *text)
    {
        this->display->setCursor(position.x, position.y);
        this->display->print(text);
    }

    void Draw::text(Vector position, const char *text, uint16_t color)
    {
        this->display->setCursor(position.x, position.y);
        this->display->setTextColor(color);
        this->display->print(text);
    }

    void Draw::text(Vector position, const char *text, uint16_t color, const GFXfont *font)
    {
        this->display->setCursor(position.x, position.y);
        this->display->setFont(font);
        this->display->setTextColor(color);
        this->display->print(text);
    }
}