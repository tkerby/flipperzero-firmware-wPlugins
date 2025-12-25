#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef TONES_END
#define TONES_END 0x8000
#endif
#ifndef TONES_REPEAT
#define TONES_REPEAT 0x8001
#endif

class ArduboyAudio {
public:
    void begin() {}
    void on() {}
    void off() {}
    bool enabled() const { return false; }
    void saveOnOff() {}
};

class ArduboyTones {
public:
    explicit ArduboyTones(bool /*enabled*/) {}
    ArduboyTones() {}

    void attachAudio(ArduboyAudio* /*audio*/) {}
    void begin() {}

    static void volumeMode(uint8_t /*mode*/) {}
    static bool playing() { return false; }

    void tones(const uint16_t* /*pattern*/) {}
    void tonesInRAM(uint16_t* /*pattern*/) {}
    void noTone() {}

    void tone(uint16_t /*frequency*/, uint16_t /*duration_ms*/) {}
    void tone(uint16_t /*f1*/, uint16_t /*d1*/, uint16_t /*f2*/, uint16_t /*d2*/) {}
    void tone(uint16_t /*f1*/, uint16_t /*d1*/, uint16_t /*f2*/, uint16_t /*d2*/, uint16_t /*f3*/, uint16_t /*d3*/) {}

    static void nextTone() {}
};
