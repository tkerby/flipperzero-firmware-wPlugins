#pragma once

#include <furi.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

// RGB color structure
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

// WS2812B LED strip structure
typedef struct WS2812B WS2812B;

// Allocate and initialize LED strip
WS2812B* ws2812b_alloc(const GpioPin* pin, uint16_t led_count);

// Free LED strip
void ws2812b_free(WS2812B* strip);

// Set LED count
void ws2812b_set_led_count(WS2812B* strip, uint16_t led_count);

// Get LED count
uint16_t ws2812b_get_led_count(WS2812B* strip);

// Set a single LED color
void ws2812b_set_led(WS2812B* strip, uint16_t index, RGB color);

// Set all LEDs to the same color
void ws2812b_set_all(WS2812B* strip, RGB color);

// Clear all LEDs (turn off)
void ws2812b_clear(WS2812B* strip);

// Update the LED strip (send data)
void ws2812b_update(WS2812B* strip);

// Helper function to create RGB color with brightness
RGB rgb_create(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

// Helper function to create HSV color (hue 0-360, sat 0-100, val 0-100)
RGB rgb_from_hsv(uint16_t hue, uint8_t sat, uint8_t val);

#ifdef __cplusplus
}
#endif
