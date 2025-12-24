#pragma once

#include "Arduino.h"
#include "ArduboyTones.h"
#include "EEPROM.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <furi.h>
#include <input/input.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#ifdef __cplusplus
} // extern "C"
#endif

#ifndef WIDTH
#define WIDTH 128
#endif
#ifndef HEIGHT
#define HEIGHT 64
#endif

#ifndef UP_BUTTON
#define UP_BUTTON    0x01
#define DOWN_BUTTON  0x02
#define LEFT_BUTTON  0x04
#define RIGHT_BUTTON 0x08
#define A_BUTTON     0x10
#define B_BUTTON     0x20
#endif

#ifndef INPUT_UP
#define INPUT_UP UP_BUTTON
#endif
#ifndef INPUT_DOWN
#define INPUT_DOWN DOWN_BUTTON
#endif
#ifndef INPUT_LEFT
#define INPUT_LEFT LEFT_BUTTON
#endif
#ifndef INPUT_RIGHT
#define INPUT_RIGHT RIGHT_BUTTON
#endif
#ifndef INPUT_A
#define INPUT_A A_BUTTON
#endif
#ifndef INPUT_B
#define INPUT_B B_BUTTON
#endif

#ifndef EEPROM_STORAGE_SPACE_START
#define EEPROM_STORAGE_SPACE_START 0
#endif

class Arduboy2Base {
public:
    ArduboyAudio audio;

    struct InputContext {
        volatile uint8_t* input_state = nullptr;
    };

    // Совместимость с твоим main.cpp: begin(buf, &input_state, mutex)
    void begin(uint8_t* screen_buffer, volatile uint8_t* input_state, FuriMutex* game_mutex) {
        sBuffer_ = screen_buffer;
        input_state_ = input_state;
        input_ctx_.input_state = input_state_;
        game_mutex_ = game_mutex;

        // На Flipper тики уже приходят таймером из main.cpp.
        // Поэтому Arduboy nextFrame не должен повторно "душить" кадры временем.
        external_timing_ = true;
    }

    InputContext* inputContext() {
        return &input_ctx_;
    }

    static void FlipperInputCallback(InputEvent* event, void* ctx_ptr) {
        if(!event || !ctx_ptr) return;
        InputContext* ctx = (InputContext*)ctx_ptr;
        if(!ctx->input_state) return;

        auto map_key_to_bit = [](InputKey key) -> uint8_t {
            switch(key) {
            case InputKeyUp:
                return INPUT_UP;
            case InputKeyDown:
                return INPUT_DOWN;
            case InputKeyLeft:
                return INPUT_LEFT;
            case InputKeyRight:
                return INPUT_RIGHT;
            case InputKeyOk:
                return INPUT_B; // OK = B
            case InputKeyBack:
                return INPUT_A; // BACK = A
            default:
                return 0;
            }
        };

        const uint8_t bit = map_key_to_bit(event->key);
        if(!bit) return;

        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            *ctx->input_state = (uint8_t)(*ctx->input_state | bit);
        } else if(event->type == InputTypeRelease) {
            *ctx->input_state = (uint8_t)(*ctx->input_state & (uint8_t)~bit);
        }
    }

    void boot() {
    }
    void bootLogo() {
    }
    void bootLogoSpritesSelfMasked() {
    }

    void setFrameRate(uint8_t fps) {
        if(fps == 0) fps = 60;
        frame_duration_ms_ = 1000u / fps;
        if(frame_duration_ms_ == 0) frame_duration_ms_ = 1;
    }

    bool nextFrame() {
        if(external_timing_) {
            frame_count_++;
            return true;
        }

        const uint32_t now = millis_flipper_();
        if(now - last_frame_ms_ < frame_duration_ms_) return false;
        last_frame_ms_ = now;
        frame_count_++;
        return true;
    }

    bool everyXFrames(uint8_t n) const {
        if(n == 0) return false;
        return (frame_count_ % n) == 0;
    }

    void pollButtons() {
        prev_buttons_ = cur_buttons_;
        cur_buttons_ = mapInputToArduboyMask_(input_state_ ? *input_state_ : 0);
    }

    bool pressed(uint8_t mask) const {
        return (cur_buttons_ & mask) != 0;
    }
    bool notPressed(uint8_t mask) const {
        return (cur_buttons_ & mask) == 0;
    }
    bool justPressed(uint8_t mask) const {
        return ((cur_buttons_ & mask) != 0) && ((prev_buttons_ & mask) == 0);
    }
    bool justReleased(uint8_t mask) const {
        return ((cur_buttons_ & mask) == 0) && ((prev_buttons_ & mask) != 0);
    }

    uint8_t* getBuffer() {
        return sBuffer_;
    }
    const uint8_t* getBuffer() const {
        return sBuffer_;
    }

    void clear() {
        fillScreen(0);
    }

    void fillScreen(uint8_t color) {
        if(!sBuffer_) return;
        memset(sBuffer_, color ? 0xFF : 0x00, (WIDTH * HEIGHT) / 8);
    }

    void display() {
    }

    // -------------------------
    // FAST RENDER CORE (BYTE BLIT)
    // -------------------------

    // настоящий putPixel нужен редко, но оставим
    void drawPixel(int16_t x, int16_t y, uint8_t color) {
        if(!sBuffer_) return;
        if(x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;

        const uint16_t idx = (uint16_t)x + (uint16_t)((y >> 3) * WIDTH);
        const uint8_t mask = (uint8_t)(1u << (y & 7));

        if(color)
            sBuffer_[idx] |= mask;
        else
            sBuffer_[idx] &= (uint8_t)~mask;
    }

    // Рисовать КАДР из спрайта (формат: [w,h] + frames)
    void drawBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        if(!bmp || !sBuffer_) return;
        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
        blitSelfMasked_(x, y, data, w, h);
    }

    void drawSolidBitmapFrame(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        if(!bmp || !sBuffer_) return;
        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
        blitOverwrite_(x, y, data, w, h);
    }

    // PlusMask: header [w,h], затем пары (spriteByte, maskByte)
    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame) {
        if(!plusmask || !sBuffer_) return;

        const int16_t w = (int16_t)pgm_read_byte(plusmask + 0);
        const int16_t h = (int16_t)pgm_read_byte(plusmask + 1);

        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;
        const uint8_t* data = plusmask + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;

        blitPlusMask_(x, y, data, w, h);
    }

        // Bresenham circle (outline). color: 0/1
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
        if(r <= 0) return;

        int16_t x = r;
        int16_t y = 0;
        int16_t err = 1 - x;

        while(x >= y) {
            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 + y, y0 + x, color);
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 - x, y0 + y, color);
            drawPixel(x0 - x, y0 - y, color);
            drawPixel(x0 - y, y0 - x, color);
            drawPixel(x0 + y, y0 - x, color);
            drawPixel(x0 + x, y0 - y, color);

            y++;
            if(err < 0) {
                err += 2 * y + 1;
            } else {
                x--;
                err += 2 * (y - x) + 1;
            }
        }
    }


    // drawSprite: если mask != nullptr, то mask чисто "маска байтами"
    // если mask == nullptr — трактуем bmp как plusmask (совместимость со старым портом)
    void drawSprite(
        int16_t x,
        int16_t y,
        const uint8_t* bmp,
        const uint8_t* mask,
        uint8_t frame,
        uint8_t mask_frame) {
        if(!bmp || !sBuffer_) return;

        const int16_t w = (int16_t)pgm_read_byte(bmp + 0);
        const int16_t h = (int16_t)pgm_read_byte(bmp + 1);
        const int16_t pages = (h + 7) >> 3;
        const uint16_t frame_size = (uint16_t)w * (uint16_t)pages;

        if(mask) {
            const uint8_t* sprite_data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size;
            const uint8_t* mask_data = mask + (uint32_t)mask_frame * (uint32_t)frame_size;
            blitExternalMask_(x, y, sprite_data, mask_data, w, h);
        } else {
            const uint8_t* data = bmp + 2 + (uint32_t)frame * (uint32_t)frame_size * 2u;
            blitPlusMask_(x, y, data, w, h);
        }
    }

    void setRGBled(uint8_t r, uint8_t g, uint8_t b) {
        NotificationApp* n = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
        if(!n) return;

        if(r)
            notification_message(n, &sequence_set_red_255);
        else
            notification_message(n, &sequence_reset_red);

        if(g)
            notification_message(n, &sequence_set_green_255);
        else
            notification_message(n, &sequence_reset_green);

        if(b)
            notification_message(n, &sequence_set_blue_255);
        else
            notification_message(n, &sequence_reset_blue);

        furi_record_close(RECORD_NOTIFICATION);
    }

    void expectLoadDelay() {
        last_frame_ms_ = millis_flipper_();
    }
    uint32_t frameCount() const {
        return frame_count_;
    }

private:
   // ---- helpers ----
static inline uint8_t page_mask_(int16_t p, int16_t pages, int16_t h) {
    // mask of valid bits in the source byte for page p
    if(p != pages - 1) return 0xFF;
    int16_t rem = (h & 7);
    if(rem == 0) return 0xFF;
    return (uint8_t)((1u << rem) - 1u);
}

void blitSelfMasked_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
    if(w <= 0 || h <= 0) return;
    const int16_t yOffset = (y & 7);
    const int16_t sRow = y >> 3;
    const int16_t pages = (h + 7) >> 3;

    for(int16_t i = 0; i < w; i++) {
        int16_t sx = x + i;
        if(sx < 0 || sx >= WIDTH) continue;

        for(int16_t p = 0; p < pages; p++) {
            uint8_t b = pgm_read_byte(src + i + p * w);
            b &= page_mask_(p, pages, h);

            int16_t row = sRow + p;

            if(yOffset == 0) {
                if(row >= 0 && row < (HEIGHT >> 3)) {
                    sBuffer_[row * WIDTH + sx] |= b;
                }
            } else {
                if(row >= 0 && row < (HEIGHT >> 3)) {
                    sBuffer_[row * WIDTH + sx] |= (uint8_t)(b << yOffset);
                }
                if((row + 1) >= 0 && (row + 1) < (HEIGHT >> 3)) {
                    sBuffer_[(row + 1) * WIDTH + sx] |= (uint8_t)(b >> (8 - yOffset));
                }
            }
        }
    }
}

void blitOverwrite_(int16_t x, int16_t y, const uint8_t* src, int16_t w, int16_t h) {
    if(w <= 0 || h <= 0) return;
    const int16_t yOffset = (y & 7);
    const int16_t sRow = y >> 3;
    const int16_t pages = (h + 7) >> 3;

    for(int16_t i = 0; i < w; i++) {
        int16_t sx = x + i;
        if(sx < 0 || sx >= WIDTH) continue;

        for(int16_t p = 0; p < pages; p++) {
            uint8_t b = pgm_read_byte(src + i + p * w);
            uint8_t srcMask = page_mask_(p, pages, h);
            b &= srcMask;

            int16_t row = sRow + p;

            if(yOffset == 0) {
                if(row >= 0 && row < (HEIGHT >> 3)) {
                    // true overwrite for this page
                    sBuffer_[row * WIDTH + sx] = b;
                }
            } else {
                // Dest masks are based on which bits of the source are valid,
                // shifted into their destination rows.
                uint8_t lo = (uint8_t)(b << yOffset);
                uint8_t hi = (uint8_t)(b >> (8 - yOffset));

                uint8_t maskLo = (uint8_t)(srcMask << yOffset);
                uint8_t maskHi = (uint8_t)(srcMask >> (8 - yOffset));

                if(row >= 0 && row < (HEIGHT >> 3)) {
                    uint8_t* dst = &sBuffer_[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~maskLo) | (lo & maskLo));
                }
                if((row + 1) >= 0 && (row + 1) < (HEIGHT >> 3)) {
                    uint8_t* dst2 = &sBuffer_[(row + 1) * WIDTH + sx];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~maskHi) | (hi & maskHi));
                }
            }
        }
    }
}

void blitPlusMask_(int16_t x, int16_t y, const uint8_t* srcPairs, int16_t w, int16_t h) {
    if(w <= 0 || h <= 0) return;
    const int16_t yOffset = (y & 7);
    const int16_t sRow = y >> 3;
    const int16_t pages = (h + 7) >> 3;

    for(int16_t i = 0; i < w; i++) {
        int16_t sx = x + i;
        if(sx < 0 || sx >= WIDTH) continue;

        for(int16_t p = 0; p < pages; p++) {
            const uint16_t idx = (uint16_t)(i + p * w) * 2u;
            uint8_t s = pgm_read_byte(srcPairs + idx + 0);
            uint8_t m = pgm_read_byte(srcPairs + idx + 1);

            uint8_t srcMask = page_mask_(p, pages, h);
            s &= srcMask;
            m &= srcMask;

            int16_t row = sRow + p;

            if(yOffset == 0) {
                if(row >= 0 && row < (HEIGHT >> 3)) {
                    uint8_t* dst = &sBuffer_[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
                }
            } else {
                uint8_t slo = (uint8_t)(s << yOffset);
                uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                uint8_t mlo = (uint8_t)(m << yOffset);
                uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                if(row >= 0 && row < (HEIGHT >> 3)) {
                    uint8_t* dst = &sBuffer_[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                }
                if((row + 1) >= 0 && (row + 1) < (HEIGHT >> 3)) {
                    uint8_t* dst2 = &sBuffer_[(row + 1) * WIDTH + sx];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                }
            }
        }
    }
}

void blitExternalMask_(int16_t x, int16_t y, const uint8_t* sprite, const uint8_t* mask, int16_t w, int16_t h) {
    if(w <= 0 || h <= 0) return;
    const int16_t yOffset = (y & 7);
    const int16_t sRow = y >> 3;
    const int16_t pages = (h + 7) >> 3;

    for(int16_t i = 0; i < w; i++) {
        int16_t sx = x + i;
        if(sx < 0 || sx >= WIDTH) continue;

        for(int16_t p = 0; p < pages; p++) {
            uint8_t s = pgm_read_byte(sprite + i + p * w);
            uint8_t m = pgm_read_byte(mask + i + p * w);

            uint8_t srcMask = page_mask_(p, pages, h);
            s &= srcMask;
            m &= srcMask;

            int16_t row = sRow + p;

            if(yOffset == 0) {
                if(row >= 0 && row < (HEIGHT >> 3)) {
                    uint8_t* dst = &sBuffer_[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~m) | (s & m));
                }
            } else {
                uint8_t slo = (uint8_t)(s << yOffset);
                uint8_t shi = (uint8_t)(s >> (8 - yOffset));
                uint8_t mlo = (uint8_t)(m << yOffset);
                uint8_t mhi = (uint8_t)(m >> (8 - yOffset));

                if(row >= 0 && row < (HEIGHT >> 3)) {
                    uint8_t* dst = &sBuffer_[row * WIDTH + sx];
                    *dst = (uint8_t)((*dst & (uint8_t)~mlo) | (slo & mlo));
                }
                if((row + 1) >= 0 && (row + 1) < (HEIGHT >> 3)) {
                    uint8_t* dst2 = &sBuffer_[(row + 1) * WIDTH + sx];
                    *dst2 = (uint8_t)((*dst2 & (uint8_t)~mhi) | (shi & mhi));
                }
            }
        }
    }
}


    // ---------- misc ----------
    static uint32_t millis_flipper_() {
        const uint32_t tick = furi_get_tick();
        const uint32_t hz = furi_kernel_get_tick_frequency();
        if(hz == 0) return tick;
        return (tick * 1000u) / hz;
    }

    static uint8_t mapInputToArduboyMask_(uint8_t in) {
        uint8_t out = 0;
        if(in & INPUT_UP) out |= UP_BUTTON;
        if(in & INPUT_DOWN) out |= DOWN_BUTTON;
        if(in & INPUT_LEFT) out |= LEFT_BUTTON;
        if(in & INPUT_RIGHT) out |= RIGHT_BUTTON;
        if(in & INPUT_B) out |= B_BUTTON;
        if(in & INPUT_A) out |= A_BUTTON;
        return out;
    }

    FuriMutex* game_mutex_ = nullptr;
    bool external_timing_ = false;

    uint8_t* sBuffer_ = nullptr;
    volatile uint8_t* input_state_ = nullptr;
    InputContext input_ctx_{};

    uint32_t frame_duration_ms_ = 16;
    uint32_t last_frame_ms_ = 0;
    uint32_t frame_count_ = 0;

    uint8_t cur_buttons_ = 0;
    uint8_t prev_buttons_ = 0;
};

class Arduboy2 : public Arduboy2Base {};

class Sprites {
public:
    static void setArduboy(Arduboy2Base* a) {
        ab_ = a;
    }

    void drawOverwrite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;
        ab_->drawSolidBitmapFrame(x, y, bmp, frame);
    }

    void drawSelfMasked(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;
        ab_->drawBitmapFrame(x, y, bmp, frame);
    }

    // erase: чистим 1-биты (редко используется; если надо — добавлю байтовую версию)
    void drawErase(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        if(!ab_ || !bmp) return;
        // простой корректный вариант (медленный, но erase обычно редкий)
        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t* data = bmp + 2;
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint16_t frame_size = (uint16_t)w * pages;
        const uint8_t* f = data + (uint32_t)frame * frame_size;

        for(uint8_t page = 0; page < pages; page++) {
            for(uint8_t i = 0; i < w; i++) {
                uint8_t byte = pgm_read_byte(f + i + (uint16_t)page * w);
                for(uint8_t b = 0; b < 8; b++) {
                    uint8_t py = (uint8_t)(page * 8 + b);
                    if(py >= h) break;
                    if(byte & (1u << b)) ab_->drawPixel(x + i, y + py, 0);
                }
            }
        }
    }

    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame = 0) {
        if(!ab_) return;
        ab_->drawPlusMask(x, y, plusmask, frame);
    }

private:
    inline static Arduboy2Base* ab_ = nullptr;
};
