#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define SENSOR_POLL_INTERVAL_MS 100
#define ADC_SAMPLES             10

// Use pin 16 (PC3) on GPIO header
#define SENSOR_PIN_NUMBER 16

// Calibration values in raw ADC readings for capacitive moisture sensor v1.2
#define ADC_DRY_VALUE 3656 // ADC reading when sensor is in dry air
#define ADC_WET_VALUE 1700 // ADC reading when sensor is submerged in water

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriHalAdcHandle* adc_handle;
    const GpioPin* gpio_pin;
    FuriHalAdcChannel adc_channel;
    uint8_t pin_number;
    uint16_t raw_adc;
    uint16_t millivolts;
    uint8_t moisture_percent;
    bool running;
    FuriMutex* mutex;
} MoistureSensorApp;

static uint8_t calculate_moisture_percent(uint16_t raw) {
    if(raw >= ADC_DRY_VALUE) return 0;
    if(raw <= ADC_WET_VALUE) return 100;

    uint32_t range = ADC_DRY_VALUE - ADC_WET_VALUE;
    uint32_t value = ADC_DRY_VALUE - raw;
    return (uint8_t)((value * 100) / range);
}

static const char* get_moisture_status(uint8_t percent) {
    if(percent < 20) return "Very Dry";
    if(percent < 40) return "Dry";
    if(percent < 60) return "Moist";
    if(percent < 80) return "Wet";
    return "Very Wet";
}

static void draw_callback(Canvas* canvas, void* ctx) {
    MoistureSensorApp* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint16_t mv = app->millivolts;
    uint16_t raw = app->raw_adc;
    uint8_t percent = app->moisture_percent;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Moisture Sensor");

    canvas_draw_line(canvas, 0, 14, 128, 14);

    // Draw moisture percentage (number and % sign separately since FontBigNumbers lacks %)
    char num_str[4];
    snprintf(num_str, sizeof(num_str), "%d", percent);
    canvas_set_font(canvas, FontBigNumbers);
    uint16_t num_width = canvas_string_width(canvas, num_str);
    canvas_set_font(canvas, FontPrimary);
    uint16_t pct_width = canvas_string_width(canvas, "%");
    uint16_t gap = 2;
    uint16_t total_width = num_width + gap + pct_width;
    uint16_t start_x = (128 - total_width) / 2;
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, start_x, 34, num_str);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, start_x + num_width + gap, 34, "%");

    // Draw status text
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, get_moisture_status(percent));

    // Draw mV and raw ADC values
    char info_str[32];
    snprintf(info_str, sizeof(info_str), "%dmV  ADC:%d  Ch:%d", mv, raw, app->adc_channel);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop, info_str);

    // Draw moisture bar
    uint8_t bar_width = percent;
    canvas_draw_frame(canvas, 14, 58, 100, 5);
    if(bar_width > 0) {
        canvas_draw_box(canvas, 14, 58, bar_width, 5);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    MoistureSensorApp* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

static void read_sensor(MoistureSensorApp* app) {
    uint32_t sum = 0;
    for(uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += furi_hal_adc_read(app->adc_handle, app->adc_channel);
        furi_delay_ms(2);
    }
    uint16_t raw = (uint16_t)(sum / ADC_SAMPLES);
    uint16_t mv = furi_hal_adc_convert_to_voltage(app->adc_handle, raw);
    uint8_t percent = calculate_moisture_percent(raw);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->raw_adc = raw;
    app->millivolts = mv;
    app->moisture_percent = percent;
    furi_mutex_release(app->mutex);
}

static MoistureSensorApp* moisture_sensor_app_alloc(void) {
    MoistureSensorApp* app = malloc(sizeof(MoistureSensorApp));
    if(!app) {
        return NULL;
    }

    // Initialize pointers to NULL for safe cleanup on failure
    app->event_queue = NULL;
    app->mutex = NULL;
    app->adc_handle = NULL;
    app->view_port = NULL;
    app->gui = NULL;
    app->gpio_pin = NULL;

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(!app->event_queue) goto cleanup;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->mutex) goto cleanup;

    app->running = true;
    app->raw_adc = 0;
    app->millivolts = 0;
    app->moisture_percent = 0;

    // Get GPIO pin and ADC channel by pin number
    const GpioPinRecord* pin_record = furi_hal_resources_pin_by_number(SENSOR_PIN_NUMBER);
    if(!pin_record || !pin_record->pin) goto cleanup;

    app->gpio_pin = pin_record->pin;
    app->adc_channel = pin_record->channel;
    app->pin_number = SENSOR_PIN_NUMBER;

    // Initialize ADC with proper configuration (2500mV scale for 3.3V sensors)
    app->adc_handle = furi_hal_adc_acquire();
    if(!app->adc_handle) goto cleanup;

    furi_hal_adc_configure_ex(
        app->adc_handle,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversample64,
        FuriHalAdcSamplingtime247_5);

    // Configure GPIO pin for analog input
    furi_hal_gpio_init(app->gpio_pin, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);

    // Setup GUI
    app->view_port = view_port_alloc();
    if(!app->view_port) goto cleanup;

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) goto cleanup;

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;

cleanup:
    if(app->gui) furi_record_close(RECORD_GUI);
    if(app->view_port) view_port_free(app->view_port);
    if(app->adc_handle) furi_hal_adc_release(app->adc_handle);
    if(app->mutex) furi_mutex_free(app->mutex);
    if(app->event_queue) furi_message_queue_free(app->event_queue);
    free(app);
    return NULL;
}

static void moisture_sensor_app_free(MoistureSensorApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_hal_adc_release(app->adc_handle);

    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->event_queue);

    free(app);
}

int32_t moisture_sensor_app(void* p) {
    UNUSED(p);

    MoistureSensorApp* app = moisture_sensor_app_alloc();
    if(!app) {
        return -1;
    }

    InputEvent event;
    uint32_t last_poll = 0;

    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeLong) {
                if(event.key == InputKeyBack) {
                    app->running = false;
                }
            }
        }

        uint32_t now = furi_get_tick();
        // Unsigned subtraction handles tick counter overflow correctly
        if(now - last_poll >= SENSOR_POLL_INTERVAL_MS) {
            last_poll = now;
            read_sensor(app);
            view_port_update(app->view_port);
        }
    }

    moisture_sensor_app_free(app);

    return 0;
}
