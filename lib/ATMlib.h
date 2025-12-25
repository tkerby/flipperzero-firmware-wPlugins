#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


// Arduino-совместимые типы
typedef uint8_t byte;
typedef uint16_t word;

// PROGMEM заглушки
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*((const uint8_t*)(addr)))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*((const uint16_t*)(addr)))
#endif

// ---- Public globals (совместимость с оригиналом) ----
extern byte trackCount;
extern byte tickRate;
extern const word* trackList;
extern const byte* trackBase;

extern uint8_t pcm;

extern uint16_t cia;
extern uint16_t cia_count;

// Oscillator structure
typedef struct {
    uint8_t vol;
    uint16_t freq;
    uint16_t phase;
} osc_t;

typedef osc_t Oscillator;

extern osc_t osc[4];

// Playroutine API
void ATM_playroutine(void);

// ---- Compatibility C wrappers (т.к. у тебя в main.cpp так вызывается) ----
void atm_system_init(void);
void atm_system_deinit(void);

// ---- API как в оригинальном ATMlib ----
class ATMsynth {
public:
    ATMsynth() {}

    static void play(const byte* song);
    static void playPause();
    static void stop();
    static void muteChannel(byte ch);
    static void unMuteChannel(byte ch);

    static void systemInit();
    static void systemDeinit();
};

// Глобальный объект
extern ATMsynth ATM;

#ifdef __cplusplus
}
#endif
