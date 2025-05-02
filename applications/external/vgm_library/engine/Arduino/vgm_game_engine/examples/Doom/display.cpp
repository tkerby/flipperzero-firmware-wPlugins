#include "display.h"

void drawGun(
    int16_t x,
    int16_t y,
    const uint8_t *bitmap,
    int16_t w,
    int16_t h,
    uint16_t color,
    Draw *const canvas)
{
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byte = 0;
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                byte <<= 1;
            else
                byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
            if (byte & 0x80)
                drawPixel(x + i, y, color, false, canvas);
        }
    }
}

void drawVLine(int16_t x, int16_t start_y, int16_t end_y, uint16_t intensity, Draw *const canvas)
{
    int16_t dots = end_y - start_y;
    for (int16_t i = 0; i < dots; i++)
    {
        canvas_draw_dot(canvas, x, start_y + i, TFT_BROWN);
    }
}

void drawText(int16_t x, int16_t y, uint16_t num, Draw *const canvas)
{
    char buf[4];
    snprintf(buf, 4, "%d", num);
    canvas->text(Vector(x, y), buf);
}

void drawTextSpace(int16_t x, int16_t y, const char *txt, uint16_t space, Draw *const canvas)
{
    int16_t pos = x;
    int16_t i = 0;
    char ch;
    while ((ch = txt[i]) != '\0')
    {
        drawChar(pos, y, ch, canvas);
        i++;
        pos += UICHAR_WIDTH + space;
        if (pos > SCREEN_WIDTH)
            return;
    }
}

void drawSprite(
    int16_t x,
    int16_t y,
    const uint8_t *bitmap,
    const uint8_t *bitmap_mask,
    int16_t w,
    int16_t h,
    uint8_t sprite,
    double distance,
    Draw *const canvas)
{
    uint16_t tw = (double)w / distance;
    uint16_t th = (double)h / distance;
    uint16_t byte_width = w / 8;
    uint16_t pixel_size = fmax(1, (double)1.0 / (double)distance);
    uint16_t sprite_offset = byte_width * h * sprite;

    bool pixel;
    bool maskPixel;

    if (zbuffer[(int)(fmin(fmax(x, 0), ZBUFFER_SIZE - 1) / Z_RES_DIVIDER)] <
        distance * DISTANCE_MULTIPLIER)
    {
        return;
    }

    for (uint16_t ty = 0; ty < th; ty += pixel_size)
    {
        if (y + ty < 0 || y + ty >= RENDER_HEIGHT)
        {
            continue;
        }
        uint16_t sy = ty * distance;
        for (uint16_t tx = 0; tx < tw; tx += pixel_size)
        {
            uint16_t sx = tx * distance;
            uint16_t byte_offset = sprite_offset + sy * byte_width + sx / 8;
            if (x + tx < 0 || x + tx >= SCREEN_WIDTH)
            {
                continue;
            }
            maskPixel = read_bit(pgm_read_byte(bitmap_mask + byte_offset), sx % 8);
            if (maskPixel)
            {
                pixel = read_bit(pgm_read_byte(bitmap + byte_offset), sx % 8);
                for (uint16_t ox = 0; ox < pixel_size; ox++)
                {
                    for (uint16_t oy = 0; oy < pixel_size; oy++)
                    {
                        if (bitmap == imp_inv)
                            drawPixel(x + tx + ox, y + ty + oy, 1, true, canvas);
                        else
                            drawPixel(x + tx + ox, y + ty + oy, pixel, true, canvas);
                    }
                }
            }
        }
    }
}

void drawPixel(int16_t x, int16_t y, bool color, bool raycasterViewport, Draw *const canvas)
{
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 ||
        y >= (raycasterViewport ? RENDER_HEIGHT : SCREEN_HEIGHT))
    {
        return;
    }
    if (color)
        canvas_draw_dot(canvas, x, y, TFT_RED);
    else
        canvas_draw_dot(canvas, x, y, TFT_BLACK);
}

void drawBitmap(
    int16_t x,
    int16_t y,
    const uint8_t *i,
    int16_t w,
    int16_t h,
    uint16_t color,
    Draw *const canvas)
{
#ifdef PicoEngine
    canvas->tft.drawBitmap(x, y, i, w, h, color);
#else
    canvas->display->drawBitmap(x, y, i, w, h, color);
#endif
}

void drawChar(int16_t x, int16_t y, char ch, Draw *const canvas)
{
    uint8_t lsb;
    uint8_t c = 0;
    while (CHAR_MAP[c] != ch && CHAR_MAP[c] != '\0')
        c++;
    for (uint8_t i = 0; i < 6; i++)
    {
        lsb = reverse_bits(char_arr[c][i]);
        for (uint8_t n = 0; n < 4; n++)
        {
            if (CHECK_BIT(lsb, n))
            {
                drawPixel(x + n, y + i, true, false, canvas);
            }
        }
    }
}

void clearRect(int16_t x, int16_t y, int16_t w, int16_t h, Draw *const canvas)
{
    canvas->clear(Vector(x, y), Vector(w, h), TFT_WHITE);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, Draw *const canvas)
{
    for (int16_t i = 0; i < w; i++)
    {
        for (int16_t j = 0; j < h; j++)
        {
            canvas_draw_dot(canvas, x + i, y + j);
        }
    }
}

bool getGradientPixel(int16_t x, int16_t y, uint8_t i)
{
    if (i == 0)
        return 0;
    if (i >= GRADIENT_COUNT - 1)
        return 1;

    uint8_t index =
        fmax(0, fmin(GRADIENT_COUNT - 1, i)) * GRADIENT_WIDTH * GRADIENT_HEIGHT +
        y * GRADIENT_WIDTH % (GRADIENT_WIDTH * GRADIENT_HEIGHT) +
        x / GRADIENT_HEIGHT % GRADIENT_WIDTH;
    return read_bit(pgm_read_byte(gradient + index), x % 8);
}

void fadeScreen(uint16_t intensity, bool color, Draw *const canvas)
{
    for (int16_t x = 0; x < SCREEN_WIDTH; x++)
    {
        for (int16_t y = 0; y < SCREEN_HEIGHT; y++)
        {
            if (getGradientPixel(x, y, intensity))
                drawPixel(x, y, color, false, canvas);
        }
    }
}

void fps()
{
    while (furi_get_tick() - lastFrameTime < FRAME_TIME)
        ;
    delta = (double)(furi_get_tick() - lastFrameTime) / (double)FRAME_TIME;
    lastFrameTime = furi_get_tick();
}

double getActualFps()
{
    return 1000 / ((double)FRAME_TIME * (double)delta);
}

uint8_t reverse_bits(uint16_t num)
{
    unsigned int NO_OF_BITS = sizeof(num) * 8;
    uint8_t reverse_num = 0;
    for (uint8_t i = 0; i < NO_OF_BITS; i++)
    {
        if ((num & (1 << i)))
            reverse_num |= 1 << ((NO_OF_BITS - 1) - i);
    }
    return reverse_num;
}

double delta = 1.0;
uint32_t lastFrameTime = 0;
uint8_t zbuffer[320];
