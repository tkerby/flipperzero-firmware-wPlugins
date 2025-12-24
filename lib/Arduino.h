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



// --- Common Arduino macros ---
#ifndef F
#define F(x) (x)
#endif

#ifndef min
#define min(a,b) (( (a) < (b) ) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (( (a) > (b) ) ? (a) : (b))
#endif

// #ifndef abs
// #define abs(x) (( (x) < 0 ) ? -(x) : (x))
// #endif

#ifndef _BV
#define _BV(bit) (1U << (bit))
#endif

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x1U)
#endif

#ifndef bitSet
#define bitSet(value, bit) ((value) |= (uint8_t)_BV(bit))
#endif

#ifndef bitClear
#define bitClear(value, bit) ((value) &= (uint8_t)~_BV(bit))
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif

static inline uint16_t pgm_read_word_safe(const void* addr) {
    uint16_t v;
    memcpy(&v, addr, sizeof(v));   // не требует выравнивания
    return v;
}
static inline uint32_t pgm_read_dword_safe(const void* addr) {
    uint32_t v;
    memcpy(&v, addr, sizeof(v));
    return v;
}

#ifndef pgm_read_word
#define pgm_read_word(addr) pgm_read_word_safe((const void*)(addr))
#endif

// если где-то используется pgm_read_dword:
#ifndef pgm_read_dword
#define pgm_read_dword(addr) pgm_read_dword_safe((const void*)(addr))
#endif

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
