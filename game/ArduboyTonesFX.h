// ArduboyTonesFX.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <furi.h>
#include <furi_hal.h>
#include "lib/Arduboy2.h"
#include "lib/ArduboyFX.h"
#include "lib/ArduboyTones.h"
#include "lib/ArduboyTonesPitches.h"

#ifndef TONES_MODE_NORMAL
#define TONES_MODE_NORMAL 0
#endif
#ifndef TONES_MODE_FX
#define TONES_MODE_FX 1
#endif
#ifndef MAX_TONES
#define MAX_TONES 3
#endif
#ifndef ARDUBOY_TONES_FX_DEFAULT_BUF_LEN
#define ARDUBOY_TONES_FX_DEFAULT_BUF_LEN 128
#endif

class ArduboyTonesFX {
public:
    explicit ArduboyTonesFX(bool (*outEn)());
    ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, uint8_t tonesArrayLen);
    ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, int tonesArrayLen);

    explicit ArduboyTonesFX(bool enabled);
    ArduboyTonesFX(bool enabled, uint16_t* tonesArray, uint8_t tonesArrayLen);
    ArduboyTonesFX(bool enabled, uint16_t* tonesArray, int tonesArrayLen);

    void tone(uint16_t freq, uint16_t dur);
    void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2);
    void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2, uint16_t freq3, uint16_t dur3);

    void tones(const uint16_t* tones_progmem);
    void tonesInRAM(uint16_t* tones_ram);
    void tonesFromFX(uint24_t tones_address);

    void noTone();
    static void volumeMode(uint8_t mode);
    static bool playing();

    void fillBufferFromFX();

private:
    bool enabled_() const;

    void fillBufferFromFX_();
    bool buildAndPlayFromRing_();

    static inline uint16_t readU16LEFromFX_() {
        uint16_t lo = FX::readPendingUInt8();
        uint16_t hi = FX::readPendingUInt8();
        return (uint16_t)(lo | (hi << 8));
    }

private:
    ArduboyTones tones_;

    bool (*outEn_)() = nullptr;
    bool enabled_value_ = true;

    volatile bool tonesPlaying_ = false;
    volatile uint8_t toneMode_ = TONES_MODE_NORMAL;

    volatile uint24_t tonesFX_Start_ = 0;
    volatile uint24_t tonesFX_Curr_ = 0;

    uint16_t* ring_ = nullptr;
    uint8_t ring_len_ = 0;
    uint8_t head_ = 0;
    int16_t tail_ = -1;

    uint16_t* linear_ = nullptr;
    uint16_t linear_cap_ = 0;

    uint16_t internal_ring_[ARDUBOY_TONES_FX_DEFAULT_BUF_LEN];
    uint16_t internal_linear_[ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1];
};
