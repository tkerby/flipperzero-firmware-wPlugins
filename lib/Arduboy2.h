#pragma once

#include "Arduino.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// На Flipper это есть. Если ты собираешь не под Flipper — можно убрать.
#include <furi.h>

#ifdef __cplusplus
} // extern "C"
#endif

// --- твой Platform ---
#include "lib/Platform.h"

// ------------------------------------------------------------
// Экран Arduboy
// ------------------------------------------------------------
#ifndef WIDTH
#define WIDTH 128
#endif

#ifndef HEIGHT
#define HEIGHT 64
#endif

// ------------------------------------------------------------
// Кнопки Arduboy (маски)
// ------------------------------------------------------------
#ifndef UP_BUTTON
#define UP_BUTTON    0x01
#define DOWN_BUTTON  0x02
#define LEFT_BUTTON  0x04
#define RIGHT_BUTTON 0x08
#define A_BUTTON     0x10
#define B_BUTTON     0x20
#endif

// ------------------------------------------------------------
// Флаги ввода платформы (если нигде не объявлены)
// В твоём main.cpp они используются напрямую (INPUT_*).
// ------------------------------------------------------------
#ifndef INPUT_UP
#define INPUT_UP    UP_BUTTON
#endif
#ifndef INPUT_DOWN
#define INPUT_DOWN  DOWN_BUTTON
#endif
#ifndef INPUT_LEFT
#define INPUT_LEFT  LEFT_BUTTON
#endif
#ifndef INPUT_RIGHT
#define INPUT_RIGHT RIGHT_BUTTON
#endif
#ifndef INPUT_A
#define INPUT_A     A_BUTTON
#endif
#ifndef INPUT_B
#define INPUT_B     B_BUTTON
#endif


#ifndef EEPROM_STORAGE_SPACE_START
#define EEPROM_STORAGE_SPACE_START 0
#endif

struct EEPROMClass {
    // 512 байт обычно хватает для Arduboy игр
    static constexpr int kSize = 512;

    uint8_t read(int addr) {
        if(addr < 0 || addr >= kSize) return 0;
        return mem_[addr];
    }

    void write(int addr, uint8_t v) {
        if(addr < 0 || addr >= kSize) return;
        mem_[addr] = v;
    }

    template<typename T>
    void put(int addr, const T& value) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(&value);
        for(size_t i = 0; i < sizeof(T); i++) {
            write(addr + (int)i, src[i]);
        }
        commit();
    }

    template<typename T>
    void get(int addr, T& value) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(&value);
        for(size_t i = 0; i < sizeof(T); i++) {
            dst[i] = read(addr + (int)i);
        }
    }

    void commit() {
        // TODO: если нужно реальное сохранение на Flipper — допишем.
    }

private:
    uint8_t mem_[kSize] = {0};
};

static EEPROMClass EEPROM;

// ------------------------------------------------------------
// Audio как в Arduboy2
// ------------------------------------------------------------
struct ArduboyAudio {
    void begin() {}
    void on() { Platform::SetAudioEnabled(true); }
    void off() { Platform::SetAudioEnabled(false); }
    static bool enabled() { return Platform::IsAudioEnabled(); }
    void saveOnOff() { /* можно не делать */ }
};

// ------------------------------------------------------------
// Arduboy2Base
// ------------------------------------------------------------
class Arduboy2Base {
public:
    ArduboyAudio audio;

    void boot() {}
    void bootLogo() {}
    void bootLogoSpritesSelfMasked() {}

    void setFrameRate(uint8_t fps) {
        if(fps == 0) fps = 60;
        frame_duration_ms_ = 1000u / fps;
        if(frame_duration_ms_ == 0) frame_duration_ms_ = 1;
    }

    bool nextFrame() {
        // На Flipper таймер может быть внешним, но Arduboy-логика часто такая:
        // if(!nextFrame()) return;
        // Поэтому делаем реальную задержку по tick'ам.
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
        cur_buttons_ = mapInputToArduboyMask_(Platform::GetInput());
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

    void clear() {
        // Arduboy clear() = залить чёрным.
        // В твоём Platform цвета/буфер могут быть инвертированы в другом месте,
        // поэтому оставляем "0" как дефолт. Если у тебя фон стал белым — поменяешь на 1.
        Platform::FillScreen(0);
    }

    void display() {
        // На Flipper рендер/свап обычно делается снаружи (в твоём main.cpp)
        // Поэтому тут noop.
    }

    uint32_t frameCount() const { return frame_count_; }

private:
    static uint32_t millis_flipper_() {
        // furi_get_tick() в "тиках ядра", переводим в ms.
        const uint32_t tick = furi_get_tick();
        const uint32_t hz = furi_kernel_get_tick_frequency();
        if(hz == 0) return tick;
        return (tick * 1000u) / hz;
    }

    static uint8_t mapInputToArduboyMask_(uint8_t in) {
        // Твой Platform::GetInput() возвращает INPUT_*.
        // Маппим 1:1 на маски Arduboy.
        uint8_t out = 0;
        if(in & INPUT_UP) out |= UP_BUTTON;
        if(in & INPUT_DOWN) out |= DOWN_BUTTON;
        if(in & INPUT_LEFT) out |= LEFT_BUTTON;
        if(in & INPUT_RIGHT) out |= RIGHT_BUTTON;

        // В твоём main.cpp OK -> INPUT_B. Сделаем его B_BUTTON (часто это "A" в играх, но не критично).
        if(in & INPUT_B) out |= B_BUTTON;

        // Если у тебя появится вторая кнопка — добавишь INPUT_A -> A_BUTTON
        if(in & INPUT_A) out |= A_BUTTON;

        return out;
    }

    uint32_t frame_duration_ms_ = 16;
    uint32_t last_frame_ms_ = 0;
    uint32_t frame_count_ = 0;

    uint8_t cur_buttons_ = 0;
    uint8_t prev_buttons_ = 0;
};

// Arduboy2 часто просто typedef/наследник Arduboy2Base
class Arduboy2 : public Arduboy2Base {};

// ------------------------------------------------------------
// Sprites (минимум нужного)
// Поддержка Arduboy bitmap формата: [w,h,data...], кадры подряд.
// ------------------------------------------------------------
class Sprites {
public:
    void drawOverwrite(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        drawFrameSolid_(x, y, bmp, frame);
    }

    void drawSelfMasked(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        drawFrameWhiteOnly_(x, y, bmp, frame);
    }

    void drawErase(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame = 0) {
        drawFrameErase_(x, y, bmp, frame);
    }

    // Arduboy "plus mask" (interleaved sprite+mask). У тебя это уже реализовано внутри Platform::DrawSprite(x,y,bitmap,frame).
    void drawPlusMask(int16_t x, int16_t y, const uint8_t* plusmask, uint8_t frame = 0) {
        Platform::DrawSprite(x, y, plusmask, frame);
    }

private:
    static inline const uint8_t* framePtr_(const uint8_t* bmp, uint8_t frame) {
        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint16_t frame_size = (uint16_t)w * pages;
        return bmp + 2 + (uint32_t)frame * frame_size;
    }

    static void drawFrameWhiteOnly_(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint8_t* data = framePtr_(bmp, frame);

        for(uint8_t page = 0; page < pages; page++) {
            for(uint8_t i = 0; i < w; i++) {
                const uint8_t byte = pgm_read_byte(data + i + (uint16_t)page * w);
                for(uint8_t b = 0; b < 8; b++) {
                    const uint8_t py = (uint8_t)(page * 8 + b);
                    if(py >= h) break;
                    if(byte & (1u << b)) {
                        Platform::PutPixel((uint8_t)(x + i), (uint8_t)(y + py), 1);
                    }
                }
            }
        }
    }

    static void drawFrameSolid_(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint8_t* data = framePtr_(bmp, frame);

        for(uint8_t page = 0; page < pages; page++) {
            for(uint8_t i = 0; i < w; i++) {
                const uint8_t byte = pgm_read_byte(data + i + (uint16_t)page * w);
                for(uint8_t b = 0; b < 8; b++) {
                    const uint8_t py = (uint8_t)(page * 8 + b);
                    if(py >= h) break;
                    const uint8_t pix = (byte & (1u << b)) ? 1 : 0;
                    Platform::PutPixel((uint8_t)(x + i), (uint8_t)(y + py), pix);
                }
            }
        }
    }

    static void drawFrameErase_(int16_t x, int16_t y, const uint8_t* bmp, uint8_t frame) {
        const uint8_t w = pgm_read_byte(bmp + 0);
        const uint8_t h = pgm_read_byte(bmp + 1);
        const uint8_t pages = (uint8_t)((h + 7) >> 3);
        const uint8_t* data = framePtr_(bmp, frame);

        for(uint8_t page = 0; page < pages; page++) {
            for(uint8_t i = 0; i < w; i++) {
                const uint8_t byte = pgm_read_byte(data + i + (uint16_t)page * w);
                for(uint8_t b = 0; b < 8; b++) {
                    const uint8_t py = (uint8_t)(page * 8 + b);
                    if(py >= h) break;
                    if(byte & (1u << b)) {
                        Platform::PutPixel((uint8_t)(x + i), (uint8_t)(y + py), 0);
                    }
                }
            }
        }
    }
};
