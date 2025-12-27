// ArduboyTonesFX.h
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "lib/ArduboyFX.h"
#include "lib/ArduboyTones.h"

#ifndef ARDUBOY_TONES_FX_DEFAULT_BUF_LEN
#define ARDUBOY_TONES_FX_DEFAULT_BUF_LEN 64
#endif

class ArduboyTonesFX : public ArduboyTones {
public:
    ArduboyTonesFX(bool (*outEn)());
    ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, uint8_t tonesArrayLen);
    ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, int tonesArrayLen);

    ArduboyTonesFX(bool enabled = true);
    ArduboyTonesFX(bool enabled, uint16_t* tonesArray, uint8_t tonesArrayLen);
    ArduboyTonesFX(bool enabled, uint16_t* tonesArray, int tonesArrayLen);

    void volumeMode(uint8_t mode);
    bool playing();

    void noTone();

    void tone(uint16_t freq, uint16_t dur = 0);
    void tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2);
    void tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2, uint16_t f3, uint16_t d3);

    void tones(const uint16_t* pattern);
    void tonesInRAM(uint16_t* pattern);

    void tonesFromFX(uint24_t tones_address);
    void fillBufferFromFX();

private:
    enum : uint8_t { TONES_MODE_NORMAL = 0, TONES_MODE_FX = 1 };

    bool enabled_() const;

    static inline uint16_t readU16LEFromFX_() {
        uint16_t lo = FX::readPendingUInt8();
        uint16_t hi = FX::readPendingUInt8();
        return (hi << 8) | lo;
    }

    void fillBufferFromFX_();
    bool buildAndPlayFromRing_();

private:
    ArduboyTones tones_;

    bool (*outEn_)() = nullptr;
    bool enabled_value_ = true;

    uint16_t internal_ring_[ARDUBOY_TONES_FX_DEFAULT_BUF_LEN];
    uint16_t* ring_ = nullptr;
    uint8_t ring_len_ = 0;

    uint16_t internal_linear_[ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1];
    uint16_t* linear_ = nullptr;
    uint16_t linear_cap_ = 0;

    volatile uint8_t head_ = 0;
    volatile int16_t tail_ = -1;

    bool tonesPlaying_ = false;
    uint8_t toneMode_ = TONES_MODE_NORMAL;

    uint24_t tonesFX_Start_ = 0;
    uint24_t tonesFX_Curr_ = 0;
};
