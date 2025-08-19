/* Mock implementation for Furi framework */
#ifndef FURI_MOCK_H
#define FURI_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Mock FURI_LOG macros
#define FURI_LOG_I(tag, fmt, ...) (void)(tag)
#define FURI_LOG_W(tag, fmt, ...) (void)(tag)
#define FURI_LOG_E(tag, fmt, ...) (void)(tag)
#define FURI_LOG_D(tag, fmt, ...) (void)(tag)

// Mock furi_assert
#define furi_assert(condition)                         \
    do {                                               \
        if(!(condition)) {                             \
            printf("ASSERT FAILED: %s\n", #condition); \
            abort();                                   \
        }                                              \
    } while(0)

// Mock memory functions
#define furi_alloc malloc
#define furi_free  free

// Mock tick functions
uint32_t furi_get_tick(void);
void furi_delay_ms(uint32_t ms);

// Mock string functions
char* furi_string_get_cstr(const char* str);

#endif /* FURI_MOCK_H */
