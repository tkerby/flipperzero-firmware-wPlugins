#include "furi.h"
#include "furi_hal.h"
#include "gui/gui.h"
#include "gui/canvas.h"
#include <furi_hal_i2c.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_bus.h>

// Adres I2C czujnika VEML7700 (7-bitowy)
#define VEML7700_I2C_ADDR 0x10

// Rejestry VEML7700
#define ALS_CONF_REG 0x00
#define ALS_MEAS_RESULT_REG 0x04
// Wzmocnienie (Gain)
#define ALS_GAIN_1_8_VAL 0x02
#define ALS_GAIN_1_4_VAL 0x01
#define ALS_GAIN_1_VAL 0x00
#define ALS_GAIN_2_VAL 0x03
// Czas integracji (Integration Time)
#define ALS_IT_100MS_VAL 0x00
// Tryb oszczedzania energii
#define ALS_POWER_SAVE_MODE_1_VAL 0x00

// Timeout dla operacji I2C
#define VEML7700_I2C_TIMEOUT 100

// Enumeracja do zarzadzania stanami (ekranami) aplikacji
typedef enum {
    AppState_Main,
    AppState_Settings,
    AppState_Credits,
} AppState;

// Enumeracja dla opcji w menu ustawien
typedef enum {
    SettingsItem_Address,
    SettingsItem_Gain,
    SettingsItem_Count
} SettingsItem;

// Struktura do przechowywania stanu aplikacji
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMutex* mutex;
    AppState current_state;
    float lux_value;
    bool running;
    bool is_sensor_initialized;

    // Zmienne dla ustawien
    uint8_t settings_cursor;
    uint8_t i2c_address;
    uint8_t als_gain; // 0=1/8, 1=1/4, 2=1, 3=2
} VEML7700App;

// Funkcja rysujaca glowny ekran
static void draw_main_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "VEML7700 LUX Meter");

    // Zabezpiecz dostep do danych sensora
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool sensor_ok = app->is_sensor_initialized;
    float lux_value = app->lux_value;
    furi_mutex_release(app->mutex);

    if (sensor_ok) {
        // Rysuj wartosc lux
        canvas_set_font(canvas, FontBigNumbers);
        FuriString* lux_str = furi_string_alloc();
        furi_string_printf(lux_str, "%.1f", (double)lux_value);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, furi_string_get_cstr(lux_str));
        furi_string_free(lux_str);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "lx");
    } else {
        // Rysuj komunikat o braku czujnika
        canvas_set_font(canvas, FontPrimary);
        const char* msg = "Podlacz czujnik";
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, msg);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Menu [<-] Wyjscie");
}

// Funkcja rysujaca ekran ustawien
static void draw_settings_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Ustawienia");

    FuriString* value_str = furi_string_alloc();
    canvas_set_font(canvas, FontSecondary);
    
    // Rysuj opcje adresu I2C
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_box(canvas, 0, 20, 128, 15);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 5, 30, "Adres I2C:");
    furi_string_printf(value_str, "0x%02X", app->i2c_address);
    canvas_draw_str_aligned(canvas, 123, 26, AlignRight, AlignTop, furi_string_get_cstr(value_str));
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_str(canvas, 1, 30, ">");
    }

    // Rysuj opcje wzmocnienia
    canvas_set_font(canvas, FontSecondary);
    if (app->settings_cursor == SettingsItem_Gain) {
        canvas_draw_box(canvas, 0, 35, 128, 15);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    const char* gain_values[] = {"1/8", "1/4", "1", "2"};
    canvas_draw_str(canvas, 5, 45, "Wzmocnienie:");
    canvas_draw_str_aligned(canvas, 123, 41, AlignRight, AlignTop, gain_values[app->als_gain]);
    if (app->settings_cursor == SettingsItem_Gain) {
        canvas_draw_str(canvas, 1, 45, ">");
    }
    
    furi_string_free(value_str);

    // Przycisk powrotu
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Powrot");
}

// Funkcja rysujaca ekran z informacjami
static void draw_credits_screen(Canvas* canvas, VEML7700App* app) {
    UNUSED(app); // Oznaczenie parametru jako nieuzywanego
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Informacje");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Aplikacja VEML7700");
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "Autor: Dr Mosfet");
    
    // Przycisk powrotu
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Powrot");
}

// Glowna funkcja rysowania, ktora przelacza ekrany
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
        case AppState_Credits:
            draw_credits_screen(canvas, app);
            break;
    }
}

// Nowa funkcja do konfiguracji czujnika VEML7700
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
            als_gain_val = ALS_GAIN_1_VAL; // Domyslna wartosc
            break;
    }

    uint16_t als_conf_data = (als_gain_val << 11) | (ALS_IT_100MS_VAL << 6) | (ALS_POWER_SAVE_MODE_1_VAL << 1);

    uint8_t write_buffer[3];
    write_buffer[0] = ALS_CONF_REG; // Rejestr konfiguracji
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

// Funkcja odczytu danych z czujnika
static bool read_veml7700(VEML7700App* app) {
    uint8_t raw_data[2];
    uint8_t reg_addr = ALS_MEAS_RESULT_REG;

    // Uzyskaj dostep do magistrali
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Sprobuj wyslac adres rejestru
    bool success_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1, // Uzycie adresu z ustawien
        false,
        &reg_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        VEML7700_I2C_TIMEOUT);

    // Sprobuj odczytac dane
    bool success_rx = false;
    if (success_tx) {
        success_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            app->i2c_address << 1, // Uzycie adresu z ustawien
            false,
            raw_data,
            2,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            VEML7700_I2C_TIMEOUT);
    }

    // Zwolnij magistrale
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (success_tx && success_rx) {
        uint16_t als_value = (static_cast<uint16_t>(raw_data[1]) << 8) | raw_data[0];
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        
        // Zastosuj współczynnik kompensacji w zależności od ustawionego wzmocnienia
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
                gain_compensation = 0.044576f; // Domyślne dla GAIN 1
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

// Funkcja obslugi zdarzen wejscia (klawiszy)
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
                     app->current_state = AppState_Credits;
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
                                app->als_gain = 3; // Zapętlenie
                            }
                        } else {
                            if (app->als_gain < 3) {
                                app->als_gain++;
                            } else {
                                app->als_gain = 0; // Zapętlenie
                            }
                        }
                    }
                    // Zastosuj nowe ustawienia po zmianie wartosci
                    init_veml7700(app);
                } else if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
            case AppState_Credits:
                if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
        }
    }
}

// Alokacja i inicjalizacja aplikacji
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
    
    // Inicjalizacja ustawien
    app->settings_cursor = SettingsItem_Address;
    app->i2c_address = VEML7700_I2C_ADDR;
    app->als_gain = 2; // Domyslne wzmocnienie 1, to jest 2gi indeks
    
    // ZAWSZE inicjalizuj czujnik na poczatku
    init_veml7700(app);

    return app;
}

// Zwolnienie zasobow
static void veml7700_app_free(VEML7700App* app) {
    furi_assert(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);
    free(app);
}

// Punkt wejscia aplikacji
extern "C" int32_t veml7700_app(void* p) {
    UNUSED(p);
    VEML7700App* app = veml7700_app_alloc();

    // Glowna petla aplikacji
    while (app->running) {
        read_veml7700(app);
        view_port_update(app->view_port);
        furi_delay_ms(500); // Odswiezanie co 500ms
    }
    
    veml7700_app_free(app);
    return 0;
}
