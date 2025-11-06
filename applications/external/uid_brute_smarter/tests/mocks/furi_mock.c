/* Mock implementation for Furi framework */
#include "furi_mock.h"
#include <time.h>

static uint32_t mock_tick = 1000;

uint32_t furi_get_tick(void) {
    return mock_tick++;
}

void furi_delay_ms(uint32_t ms) {
    (void)ms; // Mock - no actual delay
    mock_tick += ms;
}

char* furi_string_get_cstr(const char* str) {
    return (char*)str;
}

// Additional mock functions for testing
void furi_mock_reset_tick(void) {
    mock_tick = 0;
}

uint32_t furi_mock_get_current_tick(void) {
    return mock_tick;
}
