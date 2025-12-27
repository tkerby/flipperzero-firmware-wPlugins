// ArduboyTonesFX.cpp
#include "game/ArduboyTonesFX.h"

static inline uint8_t clamp_u8_len_(int v) {
    if(v <= 0) return 0;
    if(v > 255) return 255;
    return (uint8_t)v;
}

bool ArduboyTonesFX::enabled_() const {
    if(outEn_) return outEn_();
    return enabled_value_;
}

ArduboyTonesFX::ArduboyTonesFX(bool (*outEn)())
    : tones_() {
    outEn_ = outEn;
    enabled_value_ = true;

    ring_ = internal_ring_;
    ring_len_ = (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;

    linear_ = internal_linear_;
    linear_cap_ = (uint16_t)(ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1);

    tonesPlaying_ = false;
    toneMode_ = TONES_MODE_NORMAL;

    head_ = 0;
    tail_ = -1;
}

ArduboyTonesFX::ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, uint8_t tonesArrayLen)
    : tones_() {
    outEn_ = outEn;
    enabled_value_ = true;

    ring_ = tonesArray ? tonesArray : internal_ring_;
    ring_len_ = tonesArrayLen ? tonesArrayLen : (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;

    linear_ = internal_linear_;
    linear_cap_ = (uint16_t)(ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1);

    if(ring_len_ > ARDUBOY_TONES_FX_DEFAULT_BUF_LEN) {
        ring_ = internal_ring_;
        ring_len_ = (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;
    }

    tonesPlaying_ = false;
    toneMode_ = TONES_MODE_NORMAL;

    head_ = 0;
    tail_ = -1;
}

ArduboyTonesFX::ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, int tonesArrayLen)
    : ArduboyTonesFX(outEn, tonesArray, clamp_u8_len_(tonesArrayLen)) {}

ArduboyTonesFX::ArduboyTonesFX(bool enabled)
    : tones_() {
    outEn_ = nullptr;
    enabled_value_ = enabled;

    ring_ = internal_ring_;
    ring_len_ = (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;

    linear_ = internal_linear_;
    linear_cap_ = (uint16_t)(ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1);

    tonesPlaying_ = false;
    toneMode_ = TONES_MODE_NORMAL;

    head_ = 0;
    tail_ = -1;
}

ArduboyTonesFX::ArduboyTonesFX(bool enabled, uint16_t* tonesArray, uint8_t tonesArrayLen)
    : tones_() {
    outEn_ = nullptr;
    enabled_value_ = enabled;

    ring_ = tonesArray ? tonesArray : internal_ring_;
    ring_len_ = tonesArrayLen ? tonesArrayLen : (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;

    linear_ = internal_linear_;
    linear_cap_ = (uint16_t)(ARDUBOY_TONES_FX_DEFAULT_BUF_LEN + 1);

    if(ring_len_ > ARDUBOY_TONES_FX_DEFAULT_BUF_LEN) {
        ring_ = internal_ring_;
        ring_len_ = (uint8_t)ARDUBOY_TONES_FX_DEFAULT_BUF_LEN;
    }

    tonesPlaying_ = false;
    toneMode_ = TONES_MODE_NORMAL;

    head_ = 0;
    tail_ = -1;
}

ArduboyTonesFX::ArduboyTonesFX(bool enabled, uint16_t* tonesArray, int tonesArrayLen)
    : ArduboyTonesFX(enabled, tonesArray, clamp_u8_len_(tonesArrayLen)) {}

void ArduboyTonesFX::volumeMode(uint8_t mode) {
    ArduboyTones::volumeMode(mode);
}

bool ArduboyTonesFX::playing() {
    return ArduboyTones::playing();
}

void ArduboyTonesFX::noTone() {
    tones_.noTone();
    tonesPlaying_ = false;
    toneMode_ = TONES_MODE_NORMAL;
    tail_ = -1;
    head_ = 0;
}

void ArduboyTonesFX::tone(uint16_t freq, uint16_t dur) {
    toneMode_ = TONES_MODE_NORMAL;
    tonesPlaying_ = false;
    if(!enabled_()) return;
    tones_.tone(freq, dur);
}

void ArduboyTonesFX::tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2) {
    toneMode_ = TONES_MODE_NORMAL;
    tonesPlaying_ = false;
    if(!enabled_()) return;
    tones_.tone(f1, d1, f2, d2);
}

void ArduboyTonesFX::tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2, uint16_t f3, uint16_t d3) {
    toneMode_ = TONES_MODE_NORMAL;
    tonesPlaying_ = false;
    if(!enabled_()) return;
    tones_.tone(f1, d1, f2, d2, f3, d3);
}

void ArduboyTonesFX::tones(const uint16_t* pattern) {
    toneMode_ = TONES_MODE_NORMAL;
    tonesPlaying_ = false;
    if(!enabled_()) return;
    tones_.tones(pattern);
}

void ArduboyTonesFX::tonesInRAM(uint16_t* pattern) {
    toneMode_ = TONES_MODE_NORMAL;
    tonesPlaying_ = false;
    if(!enabled_()) return;
    tones_.tones((const uint16_t*)pattern);
}

void ArduboyTonesFX::tonesFromFX(uint24_t tones_address) {
    if(!enabled_()) {
        tonesPlaying_ = false;
        return;
    }

    toneMode_ = TONES_MODE_FX;
    tonesFX_Start_ = tones_address;
    tonesFX_Curr_ = tones_address;

    head_ = 0;
    tail_ = 0;
    tonesPlaying_ = true;

    fillBufferFromFX_();

    if(!buildAndPlayFromRing_()) {
        noTone();
        return;
    }
}

void ArduboyTonesFX::fillBufferFromFX_() {
    if(!tonesPlaying_) return;
    if(ring_len_ < 4) return;
    if(tail_ < 0) tail_ = 0;

    uint8_t head = head_;
    const uint8_t len = ring_len_;
    const uint8_t tail = (uint8_t)tail_;

    FX::seekData(tonesFX_Curr_);

    for(;;) {
        uint8_t next = (uint8_t)(head + 1u);
        if(next >= len) next = 0;
        if(next == tail) break;

        uint16_t t = readU16LEFromFX_();
        ring_[head] = t;
        head = next;
        tonesFX_Curr_ = (uint24_t)(tonesFX_Curr_ + 2u);

        if(t == TONES_REPEAT) {
            tonesFX_Curr_ = tonesFX_Start_;
            FX::seekData(tonesFX_Curr_);
        }

        if(t == TONES_END) break;
    }

    head_ = head;
}

bool ArduboyTonesFX::buildAndPlayFromRing_() {
    if(!tonesPlaying_) return false;
    if(tail_ < 0) return false;

    fillBufferFromFX_();

    uint16_t out_i = 0;
    const uint16_t out_cap = linear_cap_;

    uint8_t t = (uint8_t)tail_;
    const uint8_t head = head_;
    const uint8_t len = ring_len_;

    if(t == head) return false;

    while(t != head && (uint16_t)(out_i + 1u) < out_cap) {
        uint16_t w = ring_[t];
        uint8_t next = (uint8_t)(t + 1u);
        if(next >= len) next = 0;
        t = next;

        linear_[out_i++] = w;

        if(w == TONES_END) break;
        if(w == TONES_REPEAT) break;
    }

    if(out_i == 0) return false;

    if(linear_[out_i - 1u] != TONES_END) {
        if(out_i < out_cap) linear_[out_i++] = TONES_END;
        else linear_[out_cap - 1u] = TONES_END;
    }

    tail_ = (int16_t)t;

    if(linear_[0] == TONES_END) {
        tonesPlaying_ = false;
        return false;
    }

    tones_.tones(linear_);
    return true;
}

void ArduboyTonesFX::fillBufferFromFX() {
    if(toneMode_ != TONES_MODE_FX) return;

    fillBufferFromFX_();

    if(tonesPlaying_ && !ArduboyTones::playing()) {
        if(!buildAndPlayFromRing_()) {
            noTone();
        }
    }
}
