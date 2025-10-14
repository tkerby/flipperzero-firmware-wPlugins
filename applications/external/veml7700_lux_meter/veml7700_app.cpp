#include "furi.h"
#include "furi_hal.h"
#include "gui/gui.h"
#include "gui/canvas.h"
#include <furi_hal_i2c.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_bus.h>

// VEML7700 sensor I2C address (7-bit)
#define VEML7700_I2C_ADDR 0x10

// VEML7700 registers
#define ALS_CONF_REG 0x00
#define ALS_MEAS_RESULT_REG 0x04
// Gain
#define ALS_GAIN_1_8_VAL 0x02
#define ALS_GAIN_1_4_VAL 0x01
#define ALS_GAIN_1_VAL 0x00
#define ALS_GAIN_2_VAL 0x03
// Integration Time
#define ALS_IT_100MS_VAL 0x00
// Power saving mode
#define ALS_POWER_SAVE_MODE_1_VAL 0x00

// I2C operation timeout
#define VEML7700_I2C_TIMEOUT 100

// Enumeration for managing application states (screens)
typedef enum {
    AppState_Main,
    AppState_Settings,
    AppState_About,
} AppState;

// Enumeration for options in the settings menu
typedef enum {
    SettingsItem_Address,
    SettingsItem_Gain,
    SettingsItem_Count
} SettingsItem;

// Structure to store application state
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMutex* mutex;
    AppState current_state;
    float lux_value;
    bool running;
    bool is_sensor_initialized;

    // Variables for settings
    uint8_t settings_cursor;
    uint8_t i2c_address;
    uint8_t als_gain; // 0=1/8, 1=1/4, 2=1, 3=2
} VEML7700App;

// Function to draw the main screen
static void draw_main_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "VEML7700 LUX Meter");

    // Secure access to sensor data
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool sensor_ok = app->is_sensor_initialized;
    float lux_value = app->lux_value;
    furi_mutex_release(app->mutex);

    if (sensor_ok) {
        // Draw lux value
        canvas_set_font(canvas, FontBigNumbers);
        FuriString* lux_str = furi_string_alloc();
        furi_string_printf(lux_str, "%.1f", (double)lux_value);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, furi_string_get_cstr(lux_str));
        furi_string_free(lux_str);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "lx");
    } else {
        // Draw "sensor not connected" message
        canvas_set_font(canvas, FontPrimary);
        const char* msg = "Connect sensor";
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, msg);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Menu [<] Exit");
}

// Function to draw the settings screen
static void draw_settings_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Settings");

    FuriString* value_str = furi_string_alloc();
    canvas_set_font(canvas, FontSecondary);
    
    // Draw I2C address option
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_box(canvas, 0, 20, 128, 15);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 5, 30, "I2C Address:");
    furi_string_printf(value_str, "0x%02X", app->i2c_address);
    canvas_draw_str_aligned(canvas, 123, 26, AlignRight, AlignTop, furi_string_get_cstr(value_str));
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_str(canvas, 1, 30, ">");
    }

    // Draw gain option
    canvas_set_font(canvas, FontSecondary);
    if (app->settings_cursor == SettingsItem_Gain) {
        canvas_draw_box(canvas, 0, 35, 128, 15);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    const char* gain_values[] = {"1/8", "1/4", "1", "2"};
    canvas_draw_str(canvas, 5, 45, "Gain:");
    canvas_draw_str_aligned(canvas, 123, 41, AlignRight, AlignTop, gain_values[app->als_gain]);
    if (app->settings_cursor == SettingsItem_Gain) {
        canvas_draw_str(canvas, 1, 45, ">");
    }
    
    furi_string_free(value_str);

    // Back button
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Back");
}

// Function to draw the about screen
static void draw_about_screen(Canvas* canvas, VEML7700App* app) {
    UNUSED(app); // Mark parameter as unused
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "About");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "VEML7700 Application");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Author: Dr Mosfet");
    
    // Back button
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Back");
}

// Main drawing function that switches screens
static void veml7700_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    VEML7700App* app = static_cast<VEML7700App*>(context);

    switch (app->current_state) {
        case AppState_Main:
            draw_main_screen(canvas, app);
            break;
        case AppState_Settings:
            draw_settings_screen(canvas, app);
            break;
        case AppState_About:
            draw_about_screen(canvas, app);
            break;
    }
}

// Function to configure the VEML7700 sensor
static bool init_veml7700(VEML7700App* app) {
    uint8_t als_gain_val;
    switch (app->als_gain) {
        case 0:
            als_gain_val = ALS_GAIN_1_8_VAL;
            break;
        case 1:
            als_gain_val = ALS_GAIN_1_4_VAL;
            break;
        case 2:
            als_gain_val = ALS_GAIN_1_VAL;
            break;
        case 3:
            als_gain_val = ALS_GAIN_2_VAL;
            break;
        default:
            als_gain_val = ALS_GAIN_1_VAL; // Default value
            break;
    }

    uint16_t als_conf_data = (als_gain_val << 11) | (ALS_IT_100MS_VAL << 6) | (ALS_POWER_SAVE_MODE_1_VAL << 1);

    uint8_t write_buffer[3];
    write_buffer[0] = ALS_CONF_REG; // Configuration register
    write_buffer[1] = als_conf_data & 0xFF; // Low byte
    write_buffer[2] = (als_conf_data >> 8) & 0xFF; // High byte

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        write_buffer,
        3,
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        VEML7700_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return success;
}

// Function to read data from the sensor
static bool read_veml7700(VEML7700App* app) {
    uint8_t raw_data[2];
    uint8_t reg_addr = ALS_MEAS_RESULT_REG;

    // Acquire I2C bus access
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Attempt to send register address
    bool success_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1, // Use address from settings
        false,
        &reg_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        VEML7700_I2C_TIMEOUT);

    // Attempt to read data
    bool success_rx = false;
    if (success_tx) {
        success_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            app->i2c_address << 1, // Use address from settings
            false,
            raw_data,
            2,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            VEML7700_I2C_TIMEOUT);
    }

    // Release I2C bus
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (success_tx && success_rx) {
        uint16_t als_value = (static_cast<uint16_t>(raw_data[1]) << 8) | raw_data[0];
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        
        // Apply compensation factor based on the set gain
        float gain_compensation;
        switch (app->als_gain) {
            case 0: // 1/8
                gain_compensation = 0.005572f;
                break;
            case 1: // 1/4
                gain_compensation = 0.011144f;
                break;
            case 2: // 1
                gain_compensation = 0.044576f;
                break;
            case 3: // 2
                gain_compensation = 0.089152f;
                break;
            default:
                gain_compensation = 0.044576f; // Default for GAIN 1
                break;
        }

        app->lux_value = als_value * gain_compensation;
        app->is_sensor_initialized = true;
        furi_mutex_release(app->mutex);
        return true;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->is_sensor_initialized = false;
    furi_mutex_release(app->mutex);
    return false;
}

// Function to handle input events (keys)
static void veml7700_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    VEML7700App* app = static_cast<VEML7700App*>(context);

    if (input_event->type == InputTypeShort) {
        switch (app->current_state) {
            case AppState_Main:
                if (input_event->key == InputKeyOk) {
                    app->current_state = AppState_Settings;
                } else if (input_event->key == InputKeyBack) {
                    app->running = false;
                } else if (input_event->key == InputKeyRight) {
                    app->current_state = AppState_About;
                }
                break;
            case AppState_Settings:
                if (input_event->key == InputKeyUp) {
                    if (app->settings_cursor > 0) {
                        app->settings_cursor--;
                    }
                } else if (input_event->key == InputKeyDown) {
                    if (app->settings_cursor < SettingsItem_Count - 1) {
                        app->settings_cursor++;
                    }
                } else if (input_event->key == InputKeyLeft || input_event->key == InputKeyRight) {
                    if (app->settings_cursor == SettingsItem_Address) {
                        if (input_event->key == InputKeyLeft) {
                            app->i2c_address--;
                        } else {
                            app->i2c_address++;
                        }
                    } else if (app->settings_cursor == SettingsItem_Gain) {
                        if (input_event->key == InputKeyLeft) {
                            if (app->als_gain > 0) {
                                app->als_gain--;
                            } else {
                                app->als_gain = 3; // Looping
                            }
                        } else {
                            if (app->als_gain < 3) {
                                app->als_gain++;
                            } else {
                                app->als_gain = 0; // Looping
                            }
                        }
                    }
                    // Apply new settings after changing a value
                    init_veml7700(app);
                } else if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
            case AppState_About:
                if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
        }
    }
}

// Application allocation and initialization
static VEML7700App* veml7700_app_alloc() {
    VEML7700App* app = static_cast<VEML7700App*>(malloc(sizeof(VEML7700App)));
    furi_assert(app);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, veml7700_draw_callback, app);
    view_port_input_callback_set(app->view_port, veml7700_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->running = true;
    app->current_state = AppState_Main;
    app->lux_value = 0.0f;
    app->is_sensor_initialized = false;
    
    // Settings initialization
    app->settings_cursor = SettingsItem_Address;
    app->i2c_address = VEML7700_I2C_ADDR;
    app->als_gain = 2; // Default gain 1, which is index 2
    
    return app;
}

// Free resources
static void veml7700_app_free(VEML7700App* app) {
    furi_assert(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);
    free(app);
}

// Application entry point
extern "C" int32_t veml7700_app(void* p) {
    UNUSED(p);
    VEML7700App* app = veml7700_app_alloc();

    while (app->running) {
        if(!app->is_sensor_initialized) {
            app->is_sensor_initialized = init_veml7700(app);
        }

        if(app->is_sensor_initialized) {
            read_veml7700(app);
        }

        view_port_update(app->view_port);
        furi_delay_ms(500); // Refresh every 500ms
    }
    
    veml7700_app_free(app);
    return 0;
}

