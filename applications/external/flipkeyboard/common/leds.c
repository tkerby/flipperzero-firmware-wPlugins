#include "leds_i.h"

/**
 * @brief Allocates a FlipboardLeds struct.
 * @details This method allocates a FlipboardLeds struct.  This is used to
 * control the addressable LEDs on the flipboard.
 * @param resources The resources struct to use for hardware access.
 * @return The allocated FlipboardLeds struct.
*/
FlipboardLeds* flipboard_leds_alloc(Resources* resources) {
    FlipboardLeds* leds = malloc(sizeof(FlipboardLeds));

#ifdef USE_LED_DRIVER
    leds->resources = resources;
    leds->led_driver = led_driver_alloc(LED_COUNT, pin_ws2812_leds);
#else
    // If our bit-bang code is interrupted, the LED colors could become 0xFFFFFF and drain too much current?
    FURI_LOG_E(TAG, "WARNING: Using Bit-bang LEDs. Colors may be wrong. Only use for a few LEDs!");
    furi_assert(
        LED_COUNT < 16, "Bit-bang LEDs only supports up to 16 LEDs for MAX CURRENT safety.");
    furi_hal_gpio_init_simple(pin_ws2812_leds, GpioModeOutputPushPull);
    leds->led_driver = NULL;
#endif

    furi_hal_gpio_init_simple(pin_status_led, GpioModeOutputPushPull);
    flipboard_leds_reset(leds);
    return leds;
}

/**
 * @brief Frees a FlipboardLeds struct.
 * @param leds The FlipboardLeds struct to free.
*/
void flipboard_leds_free(FlipboardLeds* leds) {
    for(int i = 0; i < (1 << LED_COUNT); i++) {
        flipboard_leds_set(leds, i, 0);
    }
    flipboard_leds_update(leds);
    if(leds->led_driver) {
        led_driver_free(leds->led_driver);
    }

    furi_hal_gpio_init_simple(pin_ws2812_leds, GpioModeAnalog);
    furi_hal_gpio_init_simple(pin_status_led, GpioModeAnalog);
}

/**
 * @brief Resets the LEDs to their default color pattern (off).
 * @details This method resets the LEDs data to their default color pattern (off).
 * You must still call flipboard_leds_update to update the LEDs.
 * @param leds The FlipboardLeds struct to reset.
*/
void flipboard_leds_reset(FlipboardLeds* leds) {
    leds->color[0] = 0x000000;
    leds->color[1] = 0x000000;
    leds->color[2] = 0x000000;
    leds->color[3] = 0x000000;
}

/**
 * @brief Sets the color of the LEDs.
 * @details This method sets the color of the LEDs.
 * @param leds The FlipboardLeds struct to set the color of.
 * @param led The LED to set the color of.
 * @param color The color to set the LED to (Hex value: RRGGBB).
*/
void flipboard_leds_set(FlipboardLeds* leds, LedIds led, uint32_t color) {
    if(led & LedId1) {
        leds->color[0] = color;
    }
    if(led & LedId2) {
        leds->color[1] = color;
    }
    if(led & LedId3) {
        leds->color[2] = color;
    }
    if(led & LedId4) {
        leds->color[3] = color;
    }
}

/**
 * @brief Updates the LEDs.
 * @details This method changes the LEDs to the colors set by flipboard_leds_set.
 * @param leds The FlipboardLeds struct to update.
*/
void flipboard_leds_update(FlipboardLeds* leds) {
#ifdef USE_LED_DRIVER
    resources_acquire(leds->resources, ResourceIdLedDriver, FuriWaitForever);
    for(int i = 0; i < LED_COUNT; i++) {
        led_driver_set_led(leds->led_driver, i, leds->color[i]);
    }
    led_driver_transmit(leds->led_driver);
    resources_release(leds->resources, ResourceIdLedDriver);
#else
    furi_hal_gpio_write(pin_ws2812_leds, false);
    uint8_t bits[4 * 8 * 3 * 2];
    for(int led = 0, i = 0; led < 4; led++) {
        uint32_t data = leds->color[led];
        UNUSED(data);
        uint32_t mask_red = 0x008000U;
        for(int j = 0; j < 8; j++) {
            bits[i++] = (data & mask_red) ? 38 : 14;
            bits[i++] = (data & mask_red) ? 2 : 20;
            mask_red >>= 1;
        }
        uint32_t mask_green = 0x800000U;
        for(int j = 0; j < 8; j++) {
            bits[i++] = (data & mask_green) ? 38 : 14;
            bits[i++] = (data & mask_green) ? 2 : 20;
            mask_green >>= 1;
        }
        uint32_t mask_blue = 0x000080U;
        for(int j = 0; j < 8; j++) {
            bits[i++] = (data & mask_blue) ? 38 : 14;
            bits[i++] = (data & mask_blue) ? 2 : 20;
            mask_blue >>= 1;
        }
    }
    furi_delay_us(50);

    furi_kernel_lock();
    size_t i = 0;
    while(i < COUNT_OF(bits)) {
        uint32_t delay = bits[i++];
        furi_hal_gpio_write(pin_ws2812_leds, true);
        uint32_t start = DWT_ACCESS->COUNT;
        while((DWT_ACCESS->COUNT - start) < delay) {
        }

        furi_hal_gpio_write(pin_ws2812_leds, false);
        delay = bits[i++];
        start = DWT_ACCESS->COUNT;
        while((DWT_ACCESS->COUNT - start) < delay) {
        }
    }
    furi_kernel_unlock();

    furi_delay_ms(50);
#endif
}

/**
 * @brief Sets the status LED.
 * @details This method sets the status LED to the specified state.
 * @param leds The FlipboardLeds struct to set the status LED of.
 * @param glow True to turn the status LED on, false to turn it off.
*/
void flipboard_status_led(FlipboardLeds* leds, bool glow) {
    UNUSED(leds);
    furi_hal_gpio_write(pin_status_led, glow);
}

/**
 * @brief Adjusts the brightness of a color.
 * @details This method adjusts the brightness of a color.
 * @param color The color to adjust.
 * @param brightness The brightness to adjust the color to (0-255).
 * @return The adjusted color.
*/
uint32_t adjust_color_brightness(uint32_t color, uint8_t brightness) {
    uint32_t red = (color & 0xFF0000) >> 16;
    uint32_t green = (color & 0x00FF00) >> 8;
    uint32_t blue = (color & 0x0000FF);

    red = (red * brightness) / 255;
    green = (green * brightness) / 255;
    blue = (blue * brightness) / 255;

    return (red << 16) | (green << 8) | blue;
}