#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>

#define SENSOR_POLL_INTERVAL_MS 100
#define ADC_SAMPLES             10
#define CONFIRM_TIMEOUT_MS      1000

// GPIO pin 16 (PC3) for analog sensor input
#define SENSOR_PIN_NUMBER 16

// Calibration defaults for Capacitive Moisture Sensor v1.2
// Higher ADC = drier (more resistance), Lower ADC = wetter (less resistance)
#define ADC_MAX_VALUE   4095
#define ADC_DRY_DEFAULT 3660
#define ADC_WET_DEFAULT 1700
#define ADC_STEP        10

#define CALIBRATION_FILE_PATH APP_DATA_PATH("calibration.txt")

typedef enum {
    AppStateMain,
    AppStateMenu,
    AppStateConfirm,
} AppState;

typedef enum {
    MenuItemDryValue,
    MenuItemWetValue,
    MenuItemResetDefaults,
    MenuItemSave,
    MenuItemCount,
} MenuItem;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriHalAdcHandle* adc_handle;
    const GpioPin* gpio_pin;
    FuriHalAdcChannel adc_channel;
    uint8_t pin_number;

    // Sensor readings (protected by mutex)
    uint16_t raw_adc;
    uint16_t millivolts;
    uint8_t moisture_percent;

    bool running;
    FuriMutex* mutex;
    AppState state;
    MenuItem selected_menu_item;

    // Active calibration values used for calculations
    uint16_t cal_dry_value;
    uint16_t cal_wet_value;

    // Temporary values while editing in menu (applied on save)
    uint16_t edit_dry_value;
    uint16_t edit_wet_value;

    uint32_t confirm_start_tick;
    const char* confirm_message;
} MoistureSensorApp;
