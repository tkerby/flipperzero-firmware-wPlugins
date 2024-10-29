#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MP_FLIPPER_LED_RED (1 << 0)
#define MP_FLIPPER_LED_GREEN (1 << 1)
#define MP_FLIPPER_LED_BLUE (1 << 2)
#define MP_FLIPPER_LED_BACKLIGHT (1 << 3)

#define MP_FLIPPER_COLOR_BLACK (1 << 0)
#define MP_FLIPPER_COLOR_WHITE (1 << 1)

#define MP_FLIPPER_INPUT_BUTTON_UP (1 << 0)
#define MP_FLIPPER_INPUT_BUTTON_DOWN (1 << 1)
#define MP_FLIPPER_INPUT_BUTTON_RIGHT (1 << 2)
#define MP_FLIPPER_INPUT_BUTTON_LEFT (1 << 3)
#define MP_FLIPPER_INPUT_BUTTON_OK (1 << 4)
#define MP_FLIPPER_INPUT_BUTTON_BACK (1 << 5)
#define MP_FLIPPER_INPUT_BUTTON ((1 << 6) - 1)

#define MP_FLIPPER_INPUT_TYPE_PRESS (1 << 6)
#define MP_FLIPPER_INPUT_TYPE_RELEASE (1 << 7)
#define MP_FLIPPER_INPUT_TYPE_SHORT (1 << 8)
#define MP_FLIPPER_INPUT_TYPE_LONG (1 << 9)
#define MP_FLIPPER_INPUT_TYPE_REPEAT (1 << 10)
#define MP_FLIPPER_INPUT_TYPE ((1 << 11) - 1 - MP_FLIPPER_INPUT_BUTTON)

#define MP_FLIPPER_ALIGN_BEGIN (1 << 0)
#define MP_FLIPPER_ALIGN_CENTER (1 << 1)
#define MP_FLIPPER_ALIGN_END (1 << 2)

#define MP_FLIPPER_FONT_PRIMARY (1 << 0)
#define MP_FLIPPER_FONT_SECONDARY (1 << 1)

void mp_flipper_light_set(uint8_t raw_light, uint8_t brightness);
void mp_flipper_light_blink_start(uint8_t raw_light, uint8_t brightness, uint16_t on_time, uint16_t period);
void mp_flipper_light_blink_set_color(uint8_t raw_light);
void mp_flipper_light_blink_stop();

void mp_flipper_vibro(bool state);

/*
Python script for notes generation

# coding: utf-8
# Python script for notes generation

from typing import List

note_names: List = ['C', 'CS', 'D', 'DS', 'E', 'F', 'FS', 'G', 'GS', 'A', 'AS', 'B']
base_note: float = 16.3515979
cf: float = 2 ** (1.0 / 12)

note: float = base_note
for octave in range(9):
    for name in note_names:
        print(f"#define MP_FLIPPER_SPEAKER_NOTE_{name}{octave} MICROPY_FLOAT_CONST({round(note, 2)})")
        note = note * cf
*/

#define MP_FLIPPER_SPEAKER_NOTE_C0 MICROPY_FLOAT_CONST(16.35)
#define MP_FLIPPER_SPEAKER_NOTE_CS0 MICROPY_FLOAT_CONST(17.32)
#define MP_FLIPPER_SPEAKER_NOTE_D0 MICROPY_FLOAT_CONST(18.35)
#define MP_FLIPPER_SPEAKER_NOTE_DS0 MICROPY_FLOAT_CONST(19.45)
#define MP_FLIPPER_SPEAKER_NOTE_E0 MICROPY_FLOAT_CONST(20.6)
#define MP_FLIPPER_SPEAKER_NOTE_F0 MICROPY_FLOAT_CONST(21.83)
#define MP_FLIPPER_SPEAKER_NOTE_FS0 MICROPY_FLOAT_CONST(23.12)
#define MP_FLIPPER_SPEAKER_NOTE_G0 MICROPY_FLOAT_CONST(24.5)
#define MP_FLIPPER_SPEAKER_NOTE_GS0 MICROPY_FLOAT_CONST(25.96)
#define MP_FLIPPER_SPEAKER_NOTE_A0 MICROPY_FLOAT_CONST(27.5)
#define MP_FLIPPER_SPEAKER_NOTE_AS0 MICROPY_FLOAT_CONST(29.14)
#define MP_FLIPPER_SPEAKER_NOTE_B0 MICROPY_FLOAT_CONST(30.87)
#define MP_FLIPPER_SPEAKER_NOTE_C1 MICROPY_FLOAT_CONST(32.7)
#define MP_FLIPPER_SPEAKER_NOTE_CS1 MICROPY_FLOAT_CONST(34.65)
#define MP_FLIPPER_SPEAKER_NOTE_D1 MICROPY_FLOAT_CONST(36.71)
#define MP_FLIPPER_SPEAKER_NOTE_DS1 MICROPY_FLOAT_CONST(38.89)
#define MP_FLIPPER_SPEAKER_NOTE_E1 MICROPY_FLOAT_CONST(41.2)
#define MP_FLIPPER_SPEAKER_NOTE_F1 MICROPY_FLOAT_CONST(43.65)
#define MP_FLIPPER_SPEAKER_NOTE_FS1 MICROPY_FLOAT_CONST(46.25)
#define MP_FLIPPER_SPEAKER_NOTE_G1 MICROPY_FLOAT_CONST(49.0)
#define MP_FLIPPER_SPEAKER_NOTE_GS1 MICROPY_FLOAT_CONST(51.91)
#define MP_FLIPPER_SPEAKER_NOTE_A1 MICROPY_FLOAT_CONST(55.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS1 MICROPY_FLOAT_CONST(58.27)
#define MP_FLIPPER_SPEAKER_NOTE_B1 MICROPY_FLOAT_CONST(61.74)
#define MP_FLIPPER_SPEAKER_NOTE_C2 MICROPY_FLOAT_CONST(65.41)
#define MP_FLIPPER_SPEAKER_NOTE_CS2 MICROPY_FLOAT_CONST(69.3)
#define MP_FLIPPER_SPEAKER_NOTE_D2 MICROPY_FLOAT_CONST(73.42)
#define MP_FLIPPER_SPEAKER_NOTE_DS2 MICROPY_FLOAT_CONST(77.78)
#define MP_FLIPPER_SPEAKER_NOTE_E2 MICROPY_FLOAT_CONST(82.41)
#define MP_FLIPPER_SPEAKER_NOTE_F2 MICROPY_FLOAT_CONST(87.31)
#define MP_FLIPPER_SPEAKER_NOTE_FS2 MICROPY_FLOAT_CONST(92.5)
#define MP_FLIPPER_SPEAKER_NOTE_G2 MICROPY_FLOAT_CONST(98.0)
#define MP_FLIPPER_SPEAKER_NOTE_GS2 MICROPY_FLOAT_CONST(103.83)
#define MP_FLIPPER_SPEAKER_NOTE_A2 MICROPY_FLOAT_CONST(110.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS2 MICROPY_FLOAT_CONST(116.54)
#define MP_FLIPPER_SPEAKER_NOTE_B2 MICROPY_FLOAT_CONST(123.47)
#define MP_FLIPPER_SPEAKER_NOTE_C3 MICROPY_FLOAT_CONST(130.81)
#define MP_FLIPPER_SPEAKER_NOTE_CS3 MICROPY_FLOAT_CONST(138.59)
#define MP_FLIPPER_SPEAKER_NOTE_D3 MICROPY_FLOAT_CONST(146.83)
#define MP_FLIPPER_SPEAKER_NOTE_DS3 MICROPY_FLOAT_CONST(155.56)
#define MP_FLIPPER_SPEAKER_NOTE_E3 MICROPY_FLOAT_CONST(164.81)
#define MP_FLIPPER_SPEAKER_NOTE_F3 MICROPY_FLOAT_CONST(174.61)
#define MP_FLIPPER_SPEAKER_NOTE_FS3 MICROPY_FLOAT_CONST(185.0)
#define MP_FLIPPER_SPEAKER_NOTE_G3 MICROPY_FLOAT_CONST(196.0)
#define MP_FLIPPER_SPEAKER_NOTE_GS3 MICROPY_FLOAT_CONST(207.65)
#define MP_FLIPPER_SPEAKER_NOTE_A3 MICROPY_FLOAT_CONST(220.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS3 MICROPY_FLOAT_CONST(233.08)
#define MP_FLIPPER_SPEAKER_NOTE_B3 MICROPY_FLOAT_CONST(246.94)
#define MP_FLIPPER_SPEAKER_NOTE_C4 MICROPY_FLOAT_CONST(261.63)
#define MP_FLIPPER_SPEAKER_NOTE_CS4 MICROPY_FLOAT_CONST(277.18)
#define MP_FLIPPER_SPEAKER_NOTE_D4 MICROPY_FLOAT_CONST(293.66)
#define MP_FLIPPER_SPEAKER_NOTE_DS4 MICROPY_FLOAT_CONST(311.13)
#define MP_FLIPPER_SPEAKER_NOTE_E4 MICROPY_FLOAT_CONST(329.63)
#define MP_FLIPPER_SPEAKER_NOTE_F4 MICROPY_FLOAT_CONST(349.23)
#define MP_FLIPPER_SPEAKER_NOTE_FS4 MICROPY_FLOAT_CONST(369.99)
#define MP_FLIPPER_SPEAKER_NOTE_G4 MICROPY_FLOAT_CONST(392.0)
#define MP_FLIPPER_SPEAKER_NOTE_GS4 MICROPY_FLOAT_CONST(415.3)
#define MP_FLIPPER_SPEAKER_NOTE_A4 MICROPY_FLOAT_CONST(440.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS4 MICROPY_FLOAT_CONST(466.16)
#define MP_FLIPPER_SPEAKER_NOTE_B4 MICROPY_FLOAT_CONST(493.88)
#define MP_FLIPPER_SPEAKER_NOTE_C5 MICROPY_FLOAT_CONST(523.25)
#define MP_FLIPPER_SPEAKER_NOTE_CS5 MICROPY_FLOAT_CONST(554.37)
#define MP_FLIPPER_SPEAKER_NOTE_D5 MICROPY_FLOAT_CONST(587.33)
#define MP_FLIPPER_SPEAKER_NOTE_DS5 MICROPY_FLOAT_CONST(622.25)
#define MP_FLIPPER_SPEAKER_NOTE_E5 MICROPY_FLOAT_CONST(659.26)
#define MP_FLIPPER_SPEAKER_NOTE_F5 MICROPY_FLOAT_CONST(698.46)
#define MP_FLIPPER_SPEAKER_NOTE_FS5 MICROPY_FLOAT_CONST(739.99)
#define MP_FLIPPER_SPEAKER_NOTE_G5 MICROPY_FLOAT_CONST(783.99)
#define MP_FLIPPER_SPEAKER_NOTE_GS5 MICROPY_FLOAT_CONST(830.61)
#define MP_FLIPPER_SPEAKER_NOTE_A5 MICROPY_FLOAT_CONST(880.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS5 MICROPY_FLOAT_CONST(932.33)
#define MP_FLIPPER_SPEAKER_NOTE_B5 MICROPY_FLOAT_CONST(987.77)
#define MP_FLIPPER_SPEAKER_NOTE_C6 MICROPY_FLOAT_CONST(1046.5)
#define MP_FLIPPER_SPEAKER_NOTE_CS6 MICROPY_FLOAT_CONST(1108.73)
#define MP_FLIPPER_SPEAKER_NOTE_D6 MICROPY_FLOAT_CONST(1174.66)
#define MP_FLIPPER_SPEAKER_NOTE_DS6 MICROPY_FLOAT_CONST(1244.51)
#define MP_FLIPPER_SPEAKER_NOTE_E6 MICROPY_FLOAT_CONST(1318.51)
#define MP_FLIPPER_SPEAKER_NOTE_F6 MICROPY_FLOAT_CONST(1396.91)
#define MP_FLIPPER_SPEAKER_NOTE_FS6 MICROPY_FLOAT_CONST(1479.98)
#define MP_FLIPPER_SPEAKER_NOTE_G6 MICROPY_FLOAT_CONST(1567.98)
#define MP_FLIPPER_SPEAKER_NOTE_GS6 MICROPY_FLOAT_CONST(1661.22)
#define MP_FLIPPER_SPEAKER_NOTE_A6 MICROPY_FLOAT_CONST(1760.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS6 MICROPY_FLOAT_CONST(1864.66)
#define MP_FLIPPER_SPEAKER_NOTE_B6 MICROPY_FLOAT_CONST(1975.53)
#define MP_FLIPPER_SPEAKER_NOTE_C7 MICROPY_FLOAT_CONST(2093.0)
#define MP_FLIPPER_SPEAKER_NOTE_CS7 MICROPY_FLOAT_CONST(2217.46)
#define MP_FLIPPER_SPEAKER_NOTE_D7 MICROPY_FLOAT_CONST(2349.32)
#define MP_FLIPPER_SPEAKER_NOTE_DS7 MICROPY_FLOAT_CONST(2489.02)
#define MP_FLIPPER_SPEAKER_NOTE_E7 MICROPY_FLOAT_CONST(2637.02)
#define MP_FLIPPER_SPEAKER_NOTE_F7 MICROPY_FLOAT_CONST(2793.83)
#define MP_FLIPPER_SPEAKER_NOTE_FS7 MICROPY_FLOAT_CONST(2959.96)
#define MP_FLIPPER_SPEAKER_NOTE_G7 MICROPY_FLOAT_CONST(3135.96)
#define MP_FLIPPER_SPEAKER_NOTE_GS7 MICROPY_FLOAT_CONST(3322.44)
#define MP_FLIPPER_SPEAKER_NOTE_A7 MICROPY_FLOAT_CONST(3520.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS7 MICROPY_FLOAT_CONST(3729.31)
#define MP_FLIPPER_SPEAKER_NOTE_B7 MICROPY_FLOAT_CONST(3951.07)
#define MP_FLIPPER_SPEAKER_NOTE_C8 MICROPY_FLOAT_CONST(4186.01)
#define MP_FLIPPER_SPEAKER_NOTE_CS8 MICROPY_FLOAT_CONST(4434.92)
#define MP_FLIPPER_SPEAKER_NOTE_D8 MICROPY_FLOAT_CONST(4698.64)
#define MP_FLIPPER_SPEAKER_NOTE_DS8 MICROPY_FLOAT_CONST(4978.03)
#define MP_FLIPPER_SPEAKER_NOTE_E8 MICROPY_FLOAT_CONST(5274.04)
#define MP_FLIPPER_SPEAKER_NOTE_F8 MICROPY_FLOAT_CONST(5587.65)
#define MP_FLIPPER_SPEAKER_NOTE_FS8 MICROPY_FLOAT_CONST(5919.91)
#define MP_FLIPPER_SPEAKER_NOTE_G8 MICROPY_FLOAT_CONST(6271.93)
#define MP_FLIPPER_SPEAKER_NOTE_GS8 MICROPY_FLOAT_CONST(6644.88)
#define MP_FLIPPER_SPEAKER_NOTE_A8 MICROPY_FLOAT_CONST(7040.0)
#define MP_FLIPPER_SPEAKER_NOTE_AS8 MICROPY_FLOAT_CONST(7458.62)
#define MP_FLIPPER_SPEAKER_NOTE_B8 MICROPY_FLOAT_CONST(7902.13)

#define MP_FLIPPER_SPEAKER_VOLUME_MIN MICROPY_FLOAT_CONST(0.0)
#define MP_FLIPPER_SPEAKER_VOLUME_MAX MICROPY_FLOAT_CONST(1.0)

bool mp_flipper_speaker_start(float frequency, float volume);
bool mp_flipper_speaker_set_volume(float volume);
bool mp_flipper_speaker_stop();

uint8_t mp_flipper_canvas_width();
uint8_t mp_flipper_canvas_height();
uint8_t mp_flipper_canvas_text_width(const char* text);
uint8_t mp_flipper_canvas_text_height();
void mp_flipper_canvas_draw_dot(uint8_t x, uint8_t y);
void mp_flipper_canvas_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void mp_flipper_canvas_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void mp_flipper_canvas_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void mp_flipper_canvas_draw_circle(uint8_t x, uint8_t y, uint8_t r);
void mp_flipper_canvas_draw_disc(uint8_t x, uint8_t y, uint8_t r);
void mp_flipper_canvas_set_font(uint8_t font);
void mp_flipper_canvas_set_color(uint8_t color);
void mp_flipper_canvas_set_text(uint8_t x, uint8_t y, const char* text);
void mp_flipper_canvas_set_text_align(uint8_t x, uint8_t y);
void mp_flipper_canvas_update();
void mp_flipper_canvas_clear();

void mp_flipper_on_input(uint16_t button, uint16_t type);

void mp_flipper_dialog_message_set_text(const char* text, uint8_t x, uint8_t y, uint8_t h, uint8_t v);
void mp_flipper_dialog_message_set_header(const char* text, uint8_t x, uint8_t y, uint8_t h, uint8_t v);
void mp_flipper_dialog_message_set_button(const char* text, uint8_t button);
uint8_t mp_flipper_dialog_message_show();
void mp_flipper_dialog_message_clear();

#define MP_FLIPPER_GPIO_PIN_PC0 (0)
#define MP_FLIPPER_GPIO_PIN_PC1 (1)
#define MP_FLIPPER_GPIO_PIN_PC3 (2)
#define MP_FLIPPER_GPIO_PIN_PB2 (3)
#define MP_FLIPPER_GPIO_PIN_PB3 (4)
#define MP_FLIPPER_GPIO_PIN_PA4 (5)
#define MP_FLIPPER_GPIO_PIN_PA6 (6)
#define MP_FLIPPER_GPIO_PIN_PA7 (7)

#define MP_FLIPPER_GPIO_PINS (8)

#define MP_FLIPPER_GPIO_MODE_INPUT (1 << 0)
#define MP_FLIPPER_GPIO_MODE_OUTPUT_PUSH_PULL (1 << 1)
#define MP_FLIPPER_GPIO_MODE_OUTPUT_OPEN_DRAIN (1 << 2)
#define MP_FLIPPER_GPIO_MODE_ANALOG (1 << 3)
#define MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE (1 << 4)
#define MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL (1 << 5)

#define MP_FLIPPER_GPIO_PULL_NO (0)
#define MP_FLIPPER_GPIO_PULL_UP (1)
#define MP_FLIPPER_GPIO_PULL_DOWN (2)

#define MP_FLIPPER_GPIO_SPEED_LOW (0)
#define MP_FLIPPER_GPIO_SPEED_MEDIUM (1)
#define MP_FLIPPER_GPIO_SPEED_HIGH (2)
#define MP_FLIPPER_GPIO_SPEED_VERY_HIGH (3)

bool mp_flipper_gpio_init_pin(uint8_t raw_pin, uint8_t raw_mode, uint8_t raw_pull, uint8_t raw_speed);
void mp_flipper_gpio_deinit_pin(uint8_t raw_pin);
void mp_flipper_gpio_set_pin(uint8_t raw_pin, bool state);
bool mp_flipper_gpio_get_pin(uint8_t raw_pin);
void mp_flipper_on_gpio(void* ctx);

uint16_t mp_flipper_adc_read_pin(uint8_t raw_pin);
float mp_flipper_adc_convert_to_voltage(uint16_t value);

bool mp_flipper_pwm_start(uint8_t raw_pin, uint32_t frequency, uint8_t duty);
void mp_flipper_pwm_stop(uint8_t raw_pin);
bool mp_flipper_pwm_is_running(uint8_t raw_pin);

#define MP_FLIPPER_INFRARED_RX_DEFAULT_TIMEOUT (1000000)
#define MP_FLIPPER_INFRARED_TX_DEFAULT_FREQUENCY (38000)
#define MP_FLIPPER_INFRARED_TX_DEFAULT_DUTY_CYCLE (0.33)

typedef uint32_t (*mp_flipper_infrared_signal_tx_provider)(void* signal, const size_t index);

uint32_t* mp_flipper_infrared_receive(uint32_t timeout, size_t* length);
bool mp_flipper_infrared_transmit(
    void* signal,
    size_t length,
    mp_flipper_infrared_signal_tx_provider callback,
    uint32_t repeat,
    uint32_t frequency,
    float duty,
    bool use_external_pin);
bool mp_flipper_infrared_is_busy();

#define MP_FLIPPER_UART_MODE_USART (0)
#define MP_FLIPPER_UART_MODE_LPUART (1)

void* mp_flipper_uart_open(uint8_t raw_mode, uint32_t baud_rate);
bool mp_flipper_uart_close(void* handle);
bool mp_flipper_uart_sync(void* handle);
size_t mp_flipper_uart_read(void* handle, void* buffer, size_t size, int* errcode);
size_t mp_flipper_uart_write(void* handle, const void* buffer, size_t size, int* errcode);
