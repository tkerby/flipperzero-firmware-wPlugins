#include "moisture_sensor.h"
#include "calibration.h"
#include "ui.h"

// Convert raw ADC reading to moisture percentage (0-100%)
// ADC values are inverted: higher = drier, lower = wetter
static uint8_t calculate_moisture_percent(MoistureSensorApp* app, uint16_t raw) {
    if(app->cal_dry_value <= app->cal_wet_value) return 0;
    if(raw >= app->cal_dry_value) return 0;
    if(raw <= app->cal_wet_value) return 100;

    uint32_t range = app->cal_dry_value - app->cal_wet_value;
    uint32_t value = app->cal_dry_value - raw;
    return (uint8_t)((value * 100) / range);
}

static void show_confirm(MoistureSensorApp* app, const char* message, bool return_to_menu) {
    app->confirm_message = message;
    app->confirm_return_to_menu = return_to_menu;
    app->state = AppStateConfirm;
    app->confirm_start_tick = furi_get_tick();
}

static void handle_menu_selection(MoistureSensorApp* app) {
    switch(app->selected_menu_item) {
    case MenuItemResetDefaults:
        app->edit_dry_value = ADC_DRY_DEFAULT;
        app->edit_wet_value = ADC_WET_DEFAULT;
        app->cal_dry_value = ADC_DRY_DEFAULT;
        app->cal_wet_value = ADC_WET_DEFAULT;
        show_confirm(app, calibration_save(app) ? "Defaults restored!" : "Save failed!", false);
        break;
    case MenuItemSave:
        if(app->edit_dry_value <= app->edit_wet_value) {
            show_confirm(app, "Dry must be > Wet!", true);
            break;
        }
        app->cal_dry_value = app->edit_dry_value;
        app->cal_wet_value = app->edit_wet_value;
        show_confirm(app, calibration_save(app) ? "Saved!" : "Save failed!", false);
        break;
    default:
        break;
    }
}

static void handle_input_main(MoistureSensorApp* app, InputEvent* event) {
    if(event->key == InputKeyBack) {
        app->running = false;
    } else if(event->key == InputKeyLeft) {
        app->state = AppStateMenu;
        app->selected_menu_item = MenuItemDryValue;
        app->edit_dry_value = app->cal_dry_value;
        app->edit_wet_value = app->cal_wet_value;
    }
}

static void adjust_value(uint16_t* value, int16_t delta) {
    int32_t new_val = (int32_t)*value + delta;
    if(new_val < 0) new_val = 0;
    if(new_val > ADC_MAX_VALUE) new_val = ADC_MAX_VALUE;
    *value = (uint16_t)new_val;
}

static uint16_t* get_selected_edit_value(MoistureSensorApp* app) {
    if(app->selected_menu_item == MenuItemDryValue) return &app->edit_dry_value;
    if(app->selected_menu_item == MenuItemWetValue) return &app->edit_wet_value;
    return NULL;
}

static void handle_input_menu(MoistureSensorApp* app, InputEvent* event) {
    uint16_t* edit_val = get_selected_edit_value(app);

    // Long press/repeat adjusts by 100, short press by 10
    int16_t step = (event->type == InputTypeLong || event->type == InputTypeRepeat) ?
                       (ADC_STEP * 10) :
                       ADC_STEP;

    switch(event->key) {
    case InputKeyBack:
        app->state = AppStateMain;
        break;
    case InputKeyUp:
        if(app->selected_menu_item > 0) {
            app->selected_menu_item--;
        } else {
            app->selected_menu_item = MenuItemCount - 1;
        }
        break;
    case InputKeyDown:
        app->selected_menu_item++;
        if(app->selected_menu_item >= MenuItemCount) {
            app->selected_menu_item = 0;
        }
        break;
    case InputKeyLeft:
        if(edit_val) adjust_value(edit_val, -step);
        break;
    case InputKeyRight:
        if(edit_val) adjust_value(edit_val, step);
        break;
    case InputKeyOk:
        handle_menu_selection(app);
        break;
    default:
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    MoistureSensorApp* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// Read and average multiple ADC samples for stability
static void read_sensor(MoistureSensorApp* app) {
    uint32_t sum = 0;
    for(uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += furi_hal_adc_read(app->adc_handle, app->adc_channel);
        furi_delay_ms(2);
    }
    uint16_t raw = (uint16_t)(sum / ADC_SAMPLES);
    uint16_t mv = furi_hal_adc_convert_to_voltage(app->adc_handle, raw);
    bool connected = raw >= SENSOR_MIN_THRESHOLD;
    uint8_t percent = connected ? calculate_moisture_percent(app, raw) : 0;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->raw_adc = raw;
    app->millivolts = mv;
    app->sensor_connected = connected;
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
    app->sensor_connected = false;
    app->state = AppStateMain;
    app->selected_menu_item = MenuItemDryValue;
    app->cal_dry_value = ADC_DRY_DEFAULT;
    app->cal_wet_value = ADC_WET_DEFAULT;
    app->edit_dry_value = ADC_DRY_DEFAULT;
    app->edit_wet_value = ADC_WET_DEFAULT;
    app->confirm_start_tick = 0;
    app->confirm_message = NULL;
    app->confirm_return_to_menu = false;

    calibration_load(app);

    const GpioPinRecord* pin_record = furi_hal_resources_pin_by_number(SENSOR_PIN_NUMBER);
    if(!pin_record || !pin_record->pin) goto cleanup;

    app->gpio_pin = pin_record->pin;
    app->adc_channel = pin_record->channel;

    app->adc_handle = furi_hal_adc_acquire();
    if(!app->adc_handle) goto cleanup;

    // Configure ADC: 2.5V scale for 3.3V sensor, with oversampling for noise reduction
    furi_hal_adc_configure_ex(
        app->adc_handle,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversample64,
        FuriHalAdcSamplingtime247_5);

    furi_hal_gpio_init(app->gpio_pin, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);

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
            // In menu: long press and repeat only for Left/Right (value adjustment)
            // Prevents accidental repeated navigation
            bool handle = false;
            if(app->state == AppStateMenu) {
                if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                    handle =
                        (event.type == InputTypePress || event.type == InputTypeLong ||
                         event.type == InputTypeRepeat);
                } else {
                    handle = (event.type == InputTypePress);
                }
            } else {
                handle = (event.type == InputTypePress || event.type == InputTypeLong);
            }

            if(handle) {
                switch(app->state) {
                case AppStateMain:
                    handle_input_main(app, &event);
                    break;
                case AppStateMenu:
                    handle_input_menu(app, &event);
                    break;
                case AppStateConfirm:
                    app->state = app->confirm_return_to_menu ? AppStateMenu : AppStateMain;
                    break;
                }
                view_port_update(app->view_port);
            }
        }

        if(app->state == AppStateConfirm) {
            if(furi_get_tick() - app->confirm_start_tick >= CONFIRM_TIMEOUT_MS) {
                app->state = app->confirm_return_to_menu ? AppStateMenu : AppStateMain;
                view_port_update(app->view_port);
            }
        }

        uint32_t now = furi_get_tick();
        if(now - last_poll >= SENSOR_POLL_INTERVAL_MS) {
            last_poll = now;
            read_sensor(app);
            view_port_update(app->view_port);
        }
    }

    moisture_sensor_app_free(app);

    return 0;
}
