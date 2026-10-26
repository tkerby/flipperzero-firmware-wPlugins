#include "status_led.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stream_hub.h"
#include "uart_link.h"

// Official Flipper WiFi devboard (ESP32-S2-WROVER): three discrete LEDs, not a
// NeoPixel. Pins and polarity match FlipperHTTP's BOARD_WIFI_DEV mapping, which
// is the firmware these boards ship running.
#define LED_BLUE  GPIO_NUM_4
#define LED_GREEN GPIO_NUM_5
#define LED_RED   GPIO_NUM_6

// Wired so the GPIO sinks the LED: driving the pin low lights it.
#define LED_ON  0
#define LED_OFF 1

#define LED_TICK_MS 50

// Full blink cycles, in ticks. 20 -> 1 Hz (calm), 4 -> 5 Hz (urgent).
#define BLINK_CALM   20
#define BLINK_URGENT 4

// How long one event keeps its colour on screen. Audio arrives in 512-byte
// frames ~32 times a second, so without a hold the indicator would flicker
// between states instead of reading as "streaming".
#define AUDIO_HOLD_US (1000 * 1000)
#define ERROR_HOLD_US (2000 * 1000)

static void led_only(gpio_num_t pin, bool lit) {
    gpio_set_level(LED_RED, LED_OFF);
    gpio_set_level(LED_GREEN, LED_OFF);
    gpio_set_level(LED_BLUE, LED_OFF);
    if(lit) gpio_set_level(pin, LED_ON);
}

static void status_led_task(void* arg) {
    (void)arg;

    UartLinkStats s;
    uart_link_get_stats(&s);
    uint64_t last_audio = s.audio_bytes;
    uint32_t last_errors = s.crc_errors + s.truncated + s.oversize_errors;

    int64_t audio_at = 0;
    int64_t error_at = 0;
    uint32_t tick = 0;

    for(;;) {
        uart_link_get_stats(&s);
        const int64_t now = esp_timer_get_time();

        // The LED deliberately starts before the rest of the system, so the hub
        // it wants to ask about listeners may not exist yet.
        const bool hub_up = stream_hub_ready();

        const uint32_t errors = s.crc_errors + s.truncated + s.oversize_errors;
        if(errors != last_errors) {
            last_errors = errors;
            error_at = now;
        }
        if(s.audio_bytes != last_audio) {
            last_audio = s.audio_bytes;
            audio_at = now;
        }

        // Most specific condition wins, so the LED always shows the most
        // informative thing happening rather than an average of everything.
        gpio_num_t pin;
        uint16_t cycle;
        if(error_at && (now - error_at) < ERROR_HOLD_US) {
            pin = LED_RED; // the link is corrupting frames
            cycle = BLINK_URGENT;
        } else if(hub_up && stream_hub_listener_count() > 0) {
            pin = LED_BLUE; // somebody is actually listening
            // Blue used to swallow the audio indicator entirely: a listener
            // connected to a silent stream looked exactly like one hearing
            // music, which is the one distinction worth having when it goes
            // quiet. Fast blue means audio is still arriving from the Flipper.
            cycle = (audio_at && (now - audio_at) < AUDIO_HOLD_US) ? BLINK_URGENT : BLINK_CALM;
        } else if(audio_at && (now - audio_at) < AUDIO_HOLD_US) {
            pin = LED_GREEN; // audio flowing in from the Flipper
            cycle = BLINK_URGENT;
        } else {
            pin = LED_GREEN; // alive and idle — the heartbeat
            cycle = BLINK_CALM;
        }

        led_only(pin, (tick % cycle) < (cycle / 2));
        tick++;
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));
    }
}

void status_led_start(void) {
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_RED) | (1ULL << LED_GREEN) | (1ULL << LED_BLUE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    led_only(LED_RED, false); // all off

    // Sweep all three at boot. This costs 180 ms and proves the LED hardware
    // works, so a later dark LED means "firmware problem", not "dead board".
    const gpio_num_t sweep[] = {LED_RED, LED_GREEN, LED_BLUE};
    for(int i = 0; i < 3; i++) {
        led_only(sweep[i], true);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    led_only(LED_RED, false);

    xTaskCreate(status_led_task, "status_led", 2560, NULL, 2, NULL);
}
