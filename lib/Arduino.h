#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Arduino basic types ---
typedef uint8_t  byte;
typedef bool     boolean;
typedef uint16_t word;

#ifndef NULL
#define NULL 0
#endif

// --- PROGMEM / pgm_read_* compatibility ---
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
static inline uint8_t pgm_read_byte(const void* p) {
    return *(const uint8_t*)p;
}
#endif

#ifndef pgm_read_word
static inline uint16_t pgm_read_word(const void* p) {
    return *(const uint16_t*)p;
}
#endif

#ifndef pgm_read_dword
static inline uint32_t pgm_read_dword(const void* p) {
    return *(const uint32_t*)p;
}
#endif


// --- Common Arduino macros ---
#ifndef F
#define F(x) (x)
#endif

// #ifndef min
// #define min(a,b) (( (a) < (b) ) ? (a) : (b))
// #endif

// #ifndef max
// #define max(a,b) (( (a) > (b) ) ? (a) : (b))
// #endif

// #ifndef abs
// #define abs(x) (( (x) < 0 ) ? -(x) : (x))
// #endif

uint32_t millis(void);
uint32_t micros(void);
void delay(uint32_t ms);
void randomSeed(uint32_t seed);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

static inline long random(long max) {
    if(max <= 0) return 0;
    static long xs = 1;
    xs ^= xs << 7;
	xs ^= xs >> 9;
	xs ^= xs << 8;
    return (long)(xs % max);
}

static inline long random(long min, long max) {
    if(max <= min) return min;
    return min + random(max - min);
}

#endif
