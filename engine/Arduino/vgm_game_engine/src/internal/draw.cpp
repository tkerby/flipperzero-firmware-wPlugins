#include "draw.h"
namespace VGMGameEngine
{
    DisplayAdapter::DisplayAdapter(bool use_8bit)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (use_8bit)
        {
            this->display8 = new DVIGFX8(DVI_RES_320x240p60, true, PICO_GAME_ENGINE_BOARD_DVI_CONFIG);
            this->display16 = nullptr;
        }
        else
        {
            this->display16 = new DVIGFX16(DVI_RES_320x240p60, PICO_GAME_ENGINE_BOARD_DVI_CONFIG);
            this->display8 = nullptr;
        }
#else
        this->display = new TFT_eSPI();
#endif
    }

    DisplayAdapter::~DisplayAdapter()
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            delete this->display8;
        }
        else
        {
            delete this->display16;
        }
#else
        delete this->display;
#endif
    }

    bool DisplayAdapter::begin()
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            return this->display8->begin();
        }
        else
        {
            return this->display16->begin();
        }
#else
        this->display->init();
        this->display->setRotation(3);
        return true;
#endif
    }

    void DisplayAdapter::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawBitmap(x, y, bitmap, w, h, color);
        }
        else
        {
            this->display16->drawBitmap(x, y, bitmap, w, h, color);
        }
#else
        {
            this->display->drawBitmap(x, y, bitmap, w, h, color);
        }
#endif
    }

    void DisplayAdapter::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawCircle(x0, y0, r, color);
        }
        else
        {
            this->display16->drawCircle(x0, y0, r, color);
        }
#else
        this->display->drawCircle(x0, y0, r, color);
#endif
    }

    void DisplayAdapter::drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawGrayscaleBitmap(x, y, bitmap, w, h);
        }
        else
        {
            this->display16->drawGrayscaleBitmap(x, y, bitmap, w, h);
        }
#else
        this->display->drawBitmap(x, y, bitmap, w, h);
#endif
    }

    void DisplayAdapter::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawLine(x0, y0, x1, y1, color);
        }
        else
        {
            this->display16->drawLine(x0, y0, x1, y1, color);
        }
#else
        this->display->drawLine(x0, y0, x1, y1, color);
#endif
    }

    void DisplayAdapter::drawPixel(int16_t x, int16_t y, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawPixel(x, y, color);
        }
        else
        {
            this->display16->drawPixel(x, y, color);
        }
#else
        this->display->drawPixel(x, y, color);
#endif
    }

    void DisplayAdapter::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawRect(x, y, w, h, color);
        }
        else
        {
            this->display16->drawRect(x, y, w, h, color);
        }
#else
        this->display->drawRect(x, y, w, h, color);
#endif
    }

    void DisplayAdapter::drawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawRGBBitmap(x, y, bitmap, w, h);
        }
        else
        {
            this->display16->drawRGBBitmap(x, y, bitmap, w, h);
        }
#else
        this->display->pushImage((int32_t)x, (int32_t)y, (int32_t)w, (int32_t)h, bitmap);
#endif
    }

    void DisplayAdapter::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->drawRoundRect(x, y, w, h, r, color);
        }
        else
        {
            this->display16->drawRoundRect(x, y, w, h, r, color);
        }
#else
        this->display->drawRoundRect(x, y, w, h, r, color);
#endif
    }

    void DisplayAdapter::fillScreen(uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->fillScreen(color);
        }
        else
        {
            this->display16->fillScreen(color);
        }
#else
        this->display->fillScreen(color);
#endif
    }

    void DisplayAdapter::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->fillRect(x, y, w, h, color);
        }
        else
        {
            this->display16->fillRect(x, y, w, h, color);
        }
#else
        this->display->fillRect(x, y, w, h, color);
#endif
    }

    uint16_t *DisplayAdapter::getPalette()
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            return this->display8->getPalette();
        }
        else
        {
            // does not exist
            return nullptr;
        }
#else
        {
            // does not exist
            return nullptr;
        }
#endif
    }

    void DisplayAdapter::print(const char *text)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->print(text);
        }
        else
        {
            this->display16->print(text);
        }
#else
        this->display->print(text);
#endif
    }

    void DisplayAdapter::setPalette(const uint16_t *palette)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            memcpy(this->display8->getPalette(), palette, sizeof palette);
        }
        else
        {
            // does not exist
            return;
        }
#else
        // does not exist
        return;
#endif
    }

    void DisplayAdapter::setColor(uint8_t idx, uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->setColor(idx, color);
        }
        else
        {
            // does not exist
            return;
        }
#else
        this->display->setTextColor(color);
#endif
    }

    void DisplayAdapter::setCursor(int16_t x, int16_t y)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->setCursor(x, y);
        }
        else
        {
            this->display16->setCursor(x, y);
        }
#else
        this->display->setCursor(x, y);
#endif
    }

#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
    void DisplayAdapter::setFont(const GFXfont *font)
    {
        if (this->display8 != nullptr)
        {
            this->display8->setFont(font);
        }
        else
        {
            this->display16->setFont(font);
        }
    }
#else
    void DisplayAdapter::setFont(int font)
    {
        this->display->setTextFont(font);
    }
#endif

    void DisplayAdapter::setTextSize(uint8_t size)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->setTextSize(size);
        }
        else
        {
            this->display16->setTextSize(size);
        }
#else
        this->display->setTextSize(size);
#endif
    }

    void DisplayAdapter::setTextColor(uint16_t color)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->setTextColor(color);
        }
        else
        {
            this->display16->setTextColor(color);
        }
#else
        this->display->setTextColor(color);
#endif
    }

    void DisplayAdapter::swap(bool copy_framebuffer, bool copy_palette)
    {
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
        if (this->display8 != nullptr)
        {
            this->display8->swap(copy_framebuffer, copy_palette);
        }
        else
        {
            // does not exist
            return;
        }
#else
        // does not exist
        return;
#endif
    }

    Draw::Draw(bool use_8bit) : is_8bit(use_8bit)
    {
        this->size = Vector(320, 240);
        this->display = new DisplayAdapter(use_8bit);
        if (this->display == nullptr)
            return;

        if (!this->display->begin())
            return;

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
            this->display->drawRGBBitmap(position.x, position.y, image->buffer, image->size.x, image->size.y);
        }
    }

    void Draw::image(Vector position, const uint8_t *bitmap, Vector size)
    {
        if (bitmap != nullptr &&
            position.x < this->size.x &&
            position.y < this->size.y &&
            size.x > 0 &&
            size.y > 0)
        {
            if (this->is_8bit)
            {
                this->display->drawGrayscaleBitmap(position.x, position.y, bitmap, size.x, size.y);
            }
            else
            {
                this->display->drawRGBBitmap(position.x, position.y, (const uint16_t *)bitmap, size.x, size.y);
            }
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
#if PICO_GAME_ENGINE_BOARD_TYPE == PICO_GAME_ENGINE_BOARD_TYPE_FLIPPER_VGM
    void Draw::text(Vector position, const char *text, uint16_t color, const GFXfont *font)
#else
    void Draw::text(Vector position, const char *text, uint16_t color, int font)
#endif
    {
        this->display->setCursor(position.x, position.y);
        this->display->setFont(font);
        this->display->setTextColor(color);
        this->display->print(text);
    }
}