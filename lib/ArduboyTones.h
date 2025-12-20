#pragma once
#include <stdint.h>

#include "Platform.h"

#ifndef TONES_END
#define TONES_END 0x8000
#endif

// Должно совпадать с kToneTickHz в твоём main.cpp
#ifndef ARDUBOY_TONES_TICK_HZ
#define ARDUBOY_TONES_TICK_HZ 850u
#endif

class ArduboyTones {
public:
    // Arduboy-совместимый конструктор (в играх часто так делают)
    explicit ArduboyTones(bool enabled) {
        Platform::SetAudioEnabled(enabled);
    }

    ArduboyTones() = default;

    void begin() {
        // На Arduboy это обычно “инициализация звука”
        Platform::SetAudioEnabled(true);
    }

    void tones(const uint16_t* pattern) {
        if(!pattern) return;
        if(!Platform::IsAudioEnabled()) Platform::SetAudioEnabled(true);
        Platform::PlaySound(pattern);
    }

    // В большинстве Arduboy-игр duration в миллисекундах
    void tone(uint16_t frequency, uint16_t duration_ms) {
        if(!Platform::IsAudioEnabled()) Platform::SetAudioEnabled(true);

        const uint16_t ticks = ms_to_ticks(duration_ms);

        const uint8_t i = s_inline_idx;
        s_inline_idx = (uint8_t)((s_inline_idx + 1) % kInlineSlots);

        s_inline[i][0] = frequency; // 0 = пауза
        s_inline[i][1] = ticks;
        s_inline[i][2] = TONES_END;

        Platform::PlaySound(s_inline[i]);
    }

    void noTone() {
        // стопнуть текущий тон: короткая пауза
        tone(0, 1);
    }

private:
    static inline uint16_t ms_to_ticks(uint32_t ms) {
        uint32_t ticks = (ms * ARDUBOY_TONES_TICK_HZ + 500u) / 1000u; // round
        if(ticks == 0) ticks = 1;
        if(ticks > 0xFFFFu) ticks = 0xFFFFu;
        return (uint16_t)ticks;
    }

    static constexpr uint8_t kInlineSlots = 8;

    // C++17 inline variables: один экземпляр на линковку, не ломает множественные include
    static inline uint16_t s_inline[kInlineSlots][3] = {};
    static inline uint8_t s_inline_idx = 0;
};
