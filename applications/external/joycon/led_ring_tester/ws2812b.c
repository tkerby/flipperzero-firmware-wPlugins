#include "ws2812b.h"
#include <furi_hal.h>

#define WS2812B_T0H 350   // 0 bit high time (ns)
#define WS2812B_T1H 700   // 1 bit high time (ns)
#define WS2812B_T0L 800   // 0 bit low time (ns)
#define WS2812B_T1L 600   // 1 bit low time (ns)
#define WS2812B_RESET 50000 // Reset time (ns)

struct WS2812B {
    const GpioPin* pin;
    uint16_t led_count;
    RGB* buffer;
};

WS2812B* ws2812b_alloc(const GpioPin* pin, uint16_t led_count) {
    WS2812B* strip = malloc(sizeof(WS2812B));
    strip->pin = pin;
    strip->led_count = led_count;
    strip->buffer = malloc(sizeof(RGB) * led_count);

    // Initialize GPIO
    furi_hal_gpio_init(strip->pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(strip->pin, false);

    // Clear buffer
    for(uint16_t i = 0; i < led_count; i++) {
        strip->buffer[i].r = 0;
        strip->buffer[i].g = 0;
        strip->buffer[i].b = 0;
    }

    return strip;
}

void ws2812b_free(WS2812B* strip) {
    if(strip) {
        // Turn off all LEDs
        ws2812b_clear(strip);
        ws2812b_update(strip);

        // Reset GPIO
        furi_hal_gpio_init(strip->pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        free(strip->buffer);
        free(strip);
    }
}

void ws2812b_set_led_count(WS2812B* strip, uint16_t led_count) {
    if(strip->led_count != led_count) {
        free(strip->buffer);
        strip->led_count = led_count;
        strip->buffer = malloc(sizeof(RGB) * led_count);
        ws2812b_clear(strip);
    }
}

uint16_t ws2812b_get_led_count(WS2812B* strip) {
    return strip->led_count;
}

void ws2812b_set_led(WS2812B* strip, uint16_t index, RGB color) {
    if(index < strip->led_count) {
        strip->buffer[index] = color;
    }
}

void ws2812b_set_all(WS2812B* strip, RGB color) {
    for(uint16_t i = 0; i < strip->led_count; i++) {
        strip->buffer[i] = color;
    }
}

void ws2812b_clear(WS2812B* strip) {
    RGB black = {0, 0, 0};
    ws2812b_set_all(strip, black);
}

// Send a single bit using bit-banging
static inline void ws2812b_send_bit(const GpioPin* pin, bool bit) {
    if(bit) {
        // Send '1' bit: ~700ns high, ~600ns low
        furi_hal_gpio_write(pin, true);
        // Delay for T1H (approximately 700ns)
        asm volatile(
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        );
        furi_hal_gpio_write(pin, false);
        // Delay for T1L (approximately 600ns)
        asm volatile(
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        );
    } else {
        // Send '0' bit: ~350ns high, ~800ns low
        furi_hal_gpio_write(pin, true);
        // Delay for T0H (approximately 350ns)
        asm volatile(
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        );
        furi_hal_gpio_write(pin, false);
        // Delay for T0L (approximately 800ns)
        asm volatile(
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
            "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        );
    }
}

// Send a single byte
static void ws2812b_send_byte(const GpioPin* pin, uint8_t byte) {
    for(int8_t i = 7; i >= 0; i--) {
        ws2812b_send_bit(pin, (byte >> i) & 1);
    }
}

void ws2812b_update(WS2812B* strip) {
    // Disable interrupts for precise timing
    FURI_CRITICAL_ENTER();

    // Send data for each LED (GRB order for WS2812B)
    for(uint16_t i = 0; i < strip->led_count; i++) {
        ws2812b_send_byte(strip->pin, strip->buffer[i].g);
        ws2812b_send_byte(strip->pin, strip->buffer[i].r);
        ws2812b_send_byte(strip->pin, strip->buffer[i].b);
    }

    // Re-enable interrupts
    FURI_CRITICAL_EXIT();

    // Send reset signal (>50us low)
    furi_delay_us(60);
}

RGB rgb_create(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    RGB color;
    color.r = (r * brightness) / 255;
    color.g = (g * brightness) / 255;
    color.b = (b * brightness) / 255;
    return color;
}

RGB rgb_from_hsv(uint16_t hue, uint8_t sat, uint8_t val) {
    RGB color;

    // Normalize hue to 0-360
    hue = hue % 360;

    uint8_t region = hue / 60;
    uint8_t remainder = (hue - (region * 60)) * 6;

    uint8_t p = (val * (255 - sat)) / 255;
    uint8_t q = (val * (255 - ((sat * remainder) / 255))) / 255;
    uint8_t t = (val * (255 - ((sat * (255 - remainder)) / 255))) / 255;

    switch(region) {
        case 0:
            color.r = val; color.g = t; color.b = p;
            break;
        case 1:
            color.r = q; color.g = val; color.b = p;
            break;
        case 2:
            color.r = p; color.g = val; color.b = t;
            break;
        case 3:
            color.r = p; color.g = q; color.b = val;
            break;
        case 4:
            color.r = t; color.g = p; color.b = val;
            break;
        default:
            color.r = val; color.g = p; color.b = q;
            break;
    }

    return color;
}
