#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define MM_INPUT_CAP 1025
#define MM_OUTPUT_CAP 513
#define MM_TAPE_SIZE 256
#define MM_STEP_LIMIT 100000

typedef enum { DecodeAuto, DecodeHex, DecodeBase64, DecodeBrainfuck, DecodeROT13, DecodeCount } DecodeMode;
typedef struct {
    uint8_t bytes[MM_OUTPUT_CAP];
    size_t length;
    bool success;
    uint8_t layers;
    char error[64];
} DecodeResult;
void mm_decode(DecodeMode mode, const char* input, DecodeResult* result);
uint32_t mm_meal_hash(DecodeMode mode, const char* input);
