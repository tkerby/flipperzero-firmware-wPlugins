#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <extra_beacon.h>
#include "ble_beacon_icons.h"

#define BEACON_UPDATE_INTERVAL_MS 2000
#define BEACON_TYPE_COUNT         6
#define DEVICE_NAME               "Flipper"
#define EXIT_SPLASH_DURATION_MS   2500
#define INTRO_SPLASH_DURATION_MS  2000

typedef enum {
    BeaconTypeBthomeV2,
    BeaconTypeBthomeV1,
    BeaconTypeIbeacon,
    BeaconTypeEddystoneTlm,
    BeaconTypeAtc1441,
    BeaconTypeMibeacon,
} BeaconType;

typedef enum {
    ScreenSplash,
    ScreenMain,
    ScreenInfo,
    ScreenHelp,
    ScreenExitConfirm,
    ScreenExitSplash,
} AppScreen;

static const char* beacon_type_names[BEACON_TYPE_COUNT] = {
    "BTHome V2 Tag",
    "BTHome V1 Tag",
    "iBeacon Tag",
    "Eddystone TLM Tag",
    "ATC1441 Tag",
    "MiBeacon Tag",
};

static const bool beacon_has_simulated_humidity[BEACON_TYPE_COUNT] = {
    false,
    false,
    false,
    false,
    true,
    true,
};

static const char* info_lines[] = {
    "BLE Beacon Simulator",
    "for Flipper Zero",
    "",
    "This app turns your Flipper",
    "into a configurable BLE",
    "advertising beacon.",
    "",
    "It was created as a Flipper",
    "Zero companion for the",
    "BTHome Logger Android app,",
    "but it can also be used",
    "independently for testing",
    "BLE scanners, smart-home",
    "integrations, widgets, and",
    "mobile applications.",
    "",
    "Supported formats:",
    "BTHome v1/v2, iBeacon,",
    "Eddystone TLM, ATC1441,",
    "and MiBeacon.",
    "",
    "Real Flipper data:",
    "Battery level, voltage,",
    "current, battery and",
    "charger temperatures.",
    "",
    "Simulated data:",
    "Humidity, iBeacon UUID,",
    "major, minor, and RSSI.",
    "",
    "Author:",
    "Siarhei Besarab",
    "aka steanlab",
    "",
    "Contacts:",
    "t.me/lab66",
    "LinkedIn: @steanlab",
    "Mastodon: @lab66",
    "",
    "Support the developer:",
    "Patreon: @steanlab",
    "GitHub: @steanlab",
    "PayPal: @steanlab",
    "Revolut: @steanlab",
    "",
    "Special thanks to:",
    "",
    "Flipper Zero Team",
    "and Pavel Zhouner",
    "for providing",
    "lab equipment",
};

static const char* help_lines[] = {
    "Controls:",
    "",
    "Left / Right:",
    "Change beacon type",
    "",
    "Up, short:",
    "Select Info",
    "",
    "Down, short:",
    "Select Help",
    "",
    "Up, hold:",
    "Open Info",
    "",
    "Down, hold:",
    "Open Help",
    "",
    "OK:",
    "Open selected screen",
    "or Start / Stop beacon",
    "",
    "Back:",
    "Open exit menu",
    "",
    "Exit menu:",
    "Left: return to app",
    "Right: exit app",
};

typedef struct {
    uint8_t battery_percent;
    uint16_t voltage_mv;
    uint16_t current_ma;
    int16_t battery_temp_centi_c;
    int16_t charger_temp_centi_c;
    uint16_t simulated_humidity_centi_percent;
    uint32_t advertising_count;
    uint32_t uptime_seconds;
} BeaconTelemetry;

typedef struct {
    BeaconType type;
    bool broadcasting;
    AppScreen screen;
    bool info_selected;
    bool help_selected;
    bool exit_selected;
    bool exit_confirmed;
    uint8_t info_scroll;
    uint8_t help_scroll;
    BeaconTelemetry telemetry;
} AppState;

typedef struct {
    AppState state;
    FuriMutex* mutex;
    ViewPort* view_port;
    Gui* gui;
    FuriMessageQueue* event_queue;
    FuriTimer* update_timer;
    GapExtraBeaconConfig beacon_config;
} BthomeApp;

typedef enum {
    EventTypeTick,
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

static const uint8_t device_mac[6] = {
    0x6C,
    0x45,
    0x23,
    0x01,
    0x89,
    0xC0,
};

static void update_telemetry(BeaconTelemetry* telemetry) {
    telemetry->battery_percent = furi_hal_power_get_pct();

    float voltage_v = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    telemetry->voltage_mv = (uint16_t)(voltage_v * 1000.0f);

    float current_a = furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge);
    float current_ma = current_a * 1000.0f;

    if(current_ma < 0.0f) {
        current_ma = -current_ma;
    }

    telemetry->current_ma = (uint16_t)current_ma;

    float battery_temp_c = furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    telemetry->battery_temp_centi_c = (int16_t)(battery_temp_c * 100.0f);

    float charger_temp_c = furi_hal_power_get_battery_temperature(FuriHalPowerICCharger);
    telemetry->charger_temp_centi_c = (int16_t)(charger_temp_c * 100.0f);

    uint16_t variation = (uint16_t)((telemetry->advertising_count * 37) % 601);
    telemetry->simulated_humidity_centi_percent = 4200 + variation;

    telemetry->advertising_count++;
    telemetry->uptime_seconds += BEACON_UPDATE_INTERVAL_MS / 1000;
}

static void put_u16(uint8_t* payload, uint8_t* p, uint16_t value) {
    payload[(*p)++] = (uint8_t)(value & 0xFF);
    payload[(*p)++] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t* payload, uint8_t* p, uint32_t value) {
    payload[(*p)++] = (uint8_t)(value & 0xFF);
    payload[(*p)++] = (uint8_t)((value >> 8) & 0xFF);
    payload[(*p)++] = (uint8_t)((value >> 16) & 0xFF);
    payload[(*p)++] = (uint8_t)((value >> 24) & 0xFF);
}

static void put_device_name(uint8_t* payload, uint8_t* p) {
    payload[(*p)++] = sizeof(DEVICE_NAME); // Length + AD type
    payload[(*p)++] = 0x09; // Complete Local Name
    memcpy(&payload[*p], DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
    *p += sizeof(DEVICE_NAME) - 1;
}

static uint8_t build_bthome(const AppState* state, uint8_t* payload, bool use_v2) {
    uint8_t p = 0;

    payload[p++] = 0x02;
    payload[p++] = 0x01;
    payload[p++] = 0x06;

    uint8_t service_len_index = p++;
    payload[p++] = 0x16;

    if(use_v2) {
        put_u16(payload, &p, 0xFCD2);
        payload[p++] = 0x40; // Unencrypted BTHome V2
    } else {
        put_u16(payload, &p, 0x181C);
    }

    const BeaconTelemetry* telemetry = &state->telemetry;

    payload[p++] = 0x01;
    payload[p++] = telemetry->battery_percent;

    payload[p++] = 0x0C;
    put_u16(payload, &p, telemetry->voltage_mv);

    payload[p++] = 0x43;
    put_u16(payload, &p, telemetry->current_ma);

    payload[p++] = 0x02;
    put_u16(payload, &p, (uint16_t)telemetry->battery_temp_centi_c);

    payload[p++] = 0x02;
    put_u16(payload, &p, (uint16_t)telemetry->charger_temp_centi_c);

    payload[service_len_index] = p - service_len_index - 1;

    put_device_name(payload, &p);

    return p;
}

static uint8_t build_ibeacon(uint8_t* payload) {
    static const uint8_t proximity_uuid[16] = {
        0xE2,
        0xC5,
        0x6D,
        0xB5,
        0xDF,
        0xFB,
        0x48,
        0xD2,
        0xB0,
        0x60,
        0xD0,
        0xF5,
        0xA7,
        0x10,
        0x96,
        0xE6,
    };

    uint8_t p = 0;

    payload[p++] = 0x02;
    payload[p++] = 0x01;
    payload[p++] = 0x06;

    payload[p++] = 0x1A;
    payload[p++] = 0xFF;
    put_u16(payload, &p, 0x004C); // Apple
    payload[p++] = 0x02; // iBeacon
    payload[p++] = 0x15;

    memcpy(&payload[p], proximity_uuid, sizeof(proximity_uuid));
    p += sizeof(proximity_uuid);

    put_u16(payload, &p, 0x0001); // Major
    put_u16(payload, &p, 0x0001); // Minor
    payload[p++] = (uint8_t)-59; // RSSI at 1 m

    return p;
}

static uint8_t build_eddystone_tlm(const AppState* state, uint8_t* payload) {
    uint8_t p = 0;

    payload[p++] = 0x02;
    payload[p++] = 0x01;
    payload[p++] = 0x06;

    payload[p++] = 0x11;
    payload[p++] = 0x16;
    put_u16(payload, &p, 0xFEAA); // Eddystone UUID
    payload[p++] = 0x20; // TLM frame
    payload[p++] = 0x00; // TLM version

    put_u16(payload, &p, state->telemetry.voltage_mv);

    // Eddystone temperature uses 8.8 fixed-point notation.
    int16_t temp_c = state->telemetry.battery_temp_centi_c / 100;
    put_u16(payload, &p, (uint16_t)(temp_c << 8));

    put_u32(payload, &p, state->telemetry.advertising_count);
    put_u32(payload, &p, state->telemetry.uptime_seconds);

    put_device_name(payload, &p);

    return p;
}

static uint8_t build_atc1441(const AppState* state, uint8_t* payload) {
    uint8_t p = 0;

    payload[p++] = 0x02;
    payload[p++] = 0x01;
    payload[p++] = 0x06;

    payload[p++] = 0x11;
    payload[p++] = 0x16;
    put_u16(payload, &p, 0x181A); // Environmental Sensing UUID

    memcpy(&payload[p], device_mac, sizeof(device_mac));
    p += sizeof(device_mac);

    // ATC1441 temperature is int16 in 0.1 C.
    put_u16(payload, &p, (uint16_t)(state->telemetry.battery_temp_centi_c / 10));

    // ATC1441 humidity is uint8 in whole percent.
    payload[p++] = (uint8_t)(state->telemetry.simulated_humidity_centi_percent / 100);

    payload[p++] = state->telemetry.battery_percent;
    put_u16(payload, &p, state->telemetry.voltage_mv);
    payload[p++] = (uint8_t)(state->telemetry.advertising_count & 0xFF);

    put_device_name(payload, &p);

    return p;
}

static uint8_t build_mibeacon(const AppState* state, uint8_t* payload) {
    uint8_t p = 0;

    payload[p++] = 0x14;
    payload[p++] = 0x16;
    put_u16(payload, &p, 0xFE95); // Xiaomi service UUID

    put_u16(payload, &p, 0x3040); // Unencrypted advertisement frame control
    put_u16(payload, &p, 0x1F8A); // Product ID; replace for your device

    memcpy(&payload[p], device_mac, sizeof(device_mac));
    p += sizeof(device_mac);

    payload[p++] = 0x0D; // Temperature
    put_u16(payload, &p, (uint16_t)(state->telemetry.battery_temp_centi_c / 100));

    payload[p++] = 0x06; // Humidity
    put_u16(payload, &p, (uint16_t)(state->telemetry.simulated_humidity_centi_percent / 10));

    payload[p++] = 0x0A; // Battery level
    payload[p++] = state->telemetry.battery_percent;

    put_device_name(payload, &p);

    return p;
}

static bool build_payload(const AppState* state, uint8_t* payload, uint8_t* length) {
    switch(state->type) {
    case BeaconTypeBthomeV2:
        *length = build_bthome(state, payload, true);
        break;

    case BeaconTypeBthomeV1:
        *length = build_bthome(state, payload, false);
        break;

    case BeaconTypeIbeacon:
        *length = build_ibeacon(payload);
        break;

    case BeaconTypeEddystoneTlm:
        *length = build_eddystone_tlm(state, payload);
        break;

    case BeaconTypeAtc1441:
        *length = build_atc1441(state, payload);
        break;

    case BeaconTypeMibeacon:
        *length = build_mibeacon(state, payload);
        break;

    default:
        return false;
    }

    return *length > 0 && *length <= EXTRA_BEACON_MAX_DATA_SIZE;
}

static void restart_beacon(BthomeApp* app) {
    furi_hal_bt_extra_beacon_stop();

    if(!app->state.broadcasting) {
        return;
    }

    uint8_t payload[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t payload_length = 0;

    update_telemetry(&app->state.telemetry);

    if(!build_payload(&app->state, payload, &payload_length)) {
        return;
    }

    if(!furi_hal_bt_extra_beacon_set_config(&app->beacon_config)) {
        return;
    }

    if(!furi_hal_bt_extra_beacon_set_data(payload, payload_length)) {
        return;
    }

    furi_hal_bt_extra_beacon_start();
}

static void draw_bluetooth_icon(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x + 3, y, x + 3, y + 9);
    canvas_draw_line(canvas, x + 3, y, x + 7, y + 3);
    canvas_draw_line(canvas, x + 3, y + 9, x + 7, y + 6);
    canvas_draw_line(canvas, x + 3, y + 3, x + 7, y + 6);
    canvas_draw_line(canvas, x + 3, y + 6, x + 7, y + 3);
}

static void draw_up_triangle(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y + 4, x + 4, y);
    canvas_draw_line(canvas, x + 4, y, x + 8, y + 4);
    canvas_draw_line(canvas, x, y + 4, x + 8, y + 4);
}

static void draw_down_triangle(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y, x + 8, y);
    canvas_draw_line(canvas, x, y, x + 4, y + 4);
    canvas_draw_line(canvas, x + 8, y, x + 4, y + 4);
}

static void
    draw_side_button(Canvas* canvas, int x, const char* label, bool draw_up_arrow, bool selected) {
    if(selected) {
        canvas_draw_box(canvas, x, 51, 38, 13);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, 51, 38, 13, 2);
    }

    if(draw_up_arrow) {
        draw_up_triangle(canvas, x + 3, 56);
    } else {
        draw_down_triangle(canvas, x + 3, 56);
    }

    canvas_draw_str(canvas, x + 15, 61, label);

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
    }
}

static void
    draw_scrolled_text(Canvas* canvas, const char** lines, uint8_t line_count, uint8_t scroll) {
    const int top_y = 16;
    const int bottom_y = 48;
    const int line_height = 10;

    for(uint8_t i = 0; i < line_count; i++) {
        int y = top_y + ((int)i - (int)scroll) * line_height;

        if(y >= top_y && y <= bottom_y) {
            canvas_draw_str(canvas, 4, y, lines[i]);
        }
    }
}

static uint8_t get_scroll_limit(uint8_t line_count) {
    if(line_count <= 4) {
        return 0;
    }

    return line_count - 4;
}

static void draw_splash_screen(Canvas* canvas) {
    canvas_clear(canvas);

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_draw_icon(canvas, 5, 6, &I_splash_intro);

    const uint8_t text_center_x = 91;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, text_center_x, 22, AlignCenter, AlignCenter, "BLE Beacon");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, text_center_x, 36, AlignCenter, AlignCenter, "SIMULATOR");

    canvas_draw_str_aligned(canvas, text_center_x, 48, AlignCenter, AlignCenter, "V1.0");
}

static void draw_main_screen(Canvas* canvas, BthomeApp* app) {
    char buffer[48];
    bool icon_visible = !app->state.broadcasting ||
                        (app->state.telemetry.advertising_count & 1) != 0;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "BLE Beacon");

    canvas_set_font(canvas, FontSecondary);

    if(app->state.broadcasting) {
        canvas_draw_str(canvas, 16, 11, "ADV");

        if(icon_visible) {
            draw_bluetooth_icon(canvas, 5, 2);
        }
    }

    canvas_draw_str(canvas, 2, 23, "<");
    canvas_draw_str(canvas, 121, 23, ">");

    snprintf(buffer, sizeof(buffer), "Type: %s", beacon_type_names[app->state.type]);
    canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "V:%lumV %lu%% T:%ldC",
        (unsigned long)app->state.telemetry.voltage_mv,
        (unsigned long)app->state.telemetry.battery_percent,
        (long)(app->state.telemetry.battery_temp_centi_c / 100));
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignTop, buffer);

    if(beacon_has_simulated_humidity[app->state.type]) {
        snprintf(
            buffer,
            sizeof(buffer),
            "Hum: %lu.%02lu%% sim",
            (unsigned long)(app->state.telemetry.simulated_humidity_centi_percent / 100),
            (unsigned long)(app->state.telemetry.simulated_humidity_centi_percent % 100));
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, buffer);
    } else {
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, "Unencrypted BLE beacon");
    }

    draw_side_button(canvas, 2, "Info", true, app->state.info_selected);

    if(app->state.broadcasting) {
        elements_button_center(canvas, "Stop");
    } else {
        elements_button_center(canvas, "Start");
    }

    draw_side_button(canvas, 88, "Help", false, app->state.help_selected);
}

static void draw_info_screen(Canvas* canvas, BthomeApp* app) {
    uint8_t line_count = sizeof(info_lines) / sizeof(info_lines[0]);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Info");

    canvas_set_font(canvas, FontSecondary);
    draw_scrolled_text(canvas, info_lines, line_count, app->state.info_scroll);

    elements_button_center(canvas, "Back");
}

static void draw_help_screen(Canvas* canvas, BthomeApp* app) {
    uint8_t line_count = sizeof(help_lines) / sizeof(help_lines[0]);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Help");

    canvas_set_font(canvas, FontSecondary);
    draw_scrolled_text(canvas, help_lines, line_count, app->state.help_scroll);

    elements_button_center(canvas, "Back");
}

static void draw_exit_confirm_screen(Canvas* canvas, BthomeApp* app) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Exit?");

    canvas_set_font(canvas, FontSecondary);

    if(app->state.exit_selected) {
        elements_button_left(canvas, "No");
        elements_button_right(canvas, "Yes");
    } else {
        elements_button_left(canvas, "No");
        elements_button_right(canvas, "Yes");
    }
}

static void draw_exit_splash_screen(Canvas* canvas) {
    canvas_clear(canvas);

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "BLE Beacon");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Simulator v1.0");

    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, "LAB-66 (C) Siarhei Besarab");
}

static void draw_callback(Canvas* canvas, void* context) {
    BthomeApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    switch(app->state.screen) {
    case ScreenSplash:
        draw_splash_screen(canvas);
        break;
    case ScreenMain:
        draw_main_screen(canvas, app);
        break;
    case ScreenInfo:
        draw_info_screen(canvas, app);
        break;
    case ScreenHelp:
        draw_help_screen(canvas, app);
        break;
    case ScreenExitConfirm:
        draw_exit_confirm_screen(canvas, app);
        break;
    case ScreenExitSplash:
        draw_exit_splash_screen(canvas);
        break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* context) {
    BthomeApp* app = context;

    AppEvent event = {
        .type = EventTypeInput,
        .input = *input,
    };

    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

static void timer_callback(void* context) {
    BthomeApp* app = context;

    AppEvent event = {.type = EventTypeTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void handle_main_input(BthomeApp* app, const InputEvent* input) {
    if(input->type != InputTypeShort && input->type != InputTypeRepeat &&
       input->type != InputTypeLong) {
        return;
    }

    if(input->key == InputKeyLeft) {
        app->state.type =
            (BeaconType)((app->state.type + BEACON_TYPE_COUNT - 1) % BEACON_TYPE_COUNT);
        app->state.info_selected = false;
        app->state.help_selected = false;
        restart_beacon(app);
    } else if(input->key == InputKeyRight) {
        app->state.type = (BeaconType)((app->state.type + 1) % BEACON_TYPE_COUNT);
        app->state.info_selected = false;
        app->state.help_selected = false;
        restart_beacon(app);
    } else if(input->key == InputKeyUp) {
        if(input->type == InputTypeLong) {
            app->state.screen = ScreenInfo;
            app->state.info_scroll = 0;
            app->state.info_selected = false;
            app->state.help_selected = false;
        } else {
            app->state.info_selected = true;
            app->state.help_selected = false;
        }
    } else if(input->key == InputKeyDown) {
        if(input->type == InputTypeLong) {
            app->state.screen = ScreenHelp;
            app->state.help_scroll = 0;
            app->state.info_selected = false;
            app->state.help_selected = false;
        } else {
            app->state.info_selected = false;
            app->state.help_selected = true;
        }
    } else if(input->key == InputKeyOk) {
        if(app->state.info_selected) {
            app->state.screen = ScreenInfo;
            app->state.info_scroll = 0;
            app->state.info_selected = false;
        } else if(app->state.help_selected) {
            app->state.screen = ScreenHelp;
            app->state.help_scroll = 0;
            app->state.help_selected = false;
        } else {
            app->state.broadcasting = !app->state.broadcasting;
            restart_beacon(app);
        }
    }
}

static void handle_info_input(BthomeApp* app, const InputEvent* input) {
    if(input->type != InputTypeShort && input->type != InputTypeRepeat) {
        return;
    }

    uint8_t line_count = sizeof(info_lines) / sizeof(info_lines[0]);
    uint8_t scroll_limit = get_scroll_limit(line_count);

    if(input->key == InputKeyUp && app->state.info_scroll > 0) {
        app->state.info_scroll--;
    } else if(input->key == InputKeyDown && app->state.info_scroll < scroll_limit) {
        app->state.info_scroll++;
    } else if(input->key == InputKeyBack || input->key == InputKeyOk) {
        app->state.screen = ScreenMain;
    }
}

static void handle_help_input(BthomeApp* app, const InputEvent* input) {
    if(input->type != InputTypeShort && input->type != InputTypeRepeat) {
        return;
    }

    uint8_t line_count = sizeof(help_lines) / sizeof(help_lines[0]);
    uint8_t scroll_limit = get_scroll_limit(line_count);

    if(input->key == InputKeyUp && app->state.help_scroll > 0) {
        app->state.help_scroll--;
    } else if(input->key == InputKeyDown && app->state.help_scroll < scroll_limit) {
        app->state.help_scroll++;
    } else if(input->key == InputKeyBack || input->key == InputKeyOk) {
        app->state.screen = ScreenMain;
    }
}

static void handle_exit_input(BthomeApp* app, const InputEvent* input) {
    if(input->type != InputTypeShort) {
        return;
    }

    if(input->key == InputKeyLeft) {
        app->state.screen = ScreenMain;
    } else if(input->key == InputKeyRight) {
        app->state.exit_confirmed = true;
    } else if(input->key == InputKeyOk) {
        if(app->state.exit_selected) {
            app->state.exit_confirmed = true;
        } else {
            app->state.screen = ScreenMain;
        }
    } else if(input->key == InputKeyBack) {
        app->state.screen = ScreenMain;
    }
}

int32_t ble_beacon_app(void* p) {
    UNUSED(p);

    BthomeApp* app = malloc(sizeof(BthomeApp));
    furi_check(app);

    app->state.type = BeaconTypeBthomeV2;
    app->state.broadcasting = false;
    app->state.screen = ScreenSplash;
    app->state.info_selected = false;
    app->state.help_selected = false;
    app->state.exit_selected = false;
    app->state.exit_confirmed = false;
    app->state.info_scroll = 0;
    app->state.help_scroll = 0;

    update_telemetry(&app->state.telemetry);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->beacon_config.adv_channel_map = 0x07;
    app->beacon_config.address_type = GapAddressTypeRandom;
    memcpy(app->beacon_config.address, device_mac, sizeof(device_mac));
    app->beacon_config.min_adv_interval_ms = 1000;
    app->beacon_config.max_adv_interval_ms = 1000;

    app->update_timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);

    furi_timer_start(
        app->update_timer, furi_kernel_get_tick_frequency() * BEACON_UPDATE_INTERVAL_MS / 1000);

    view_port_update(app->view_port);
    furi_delay_ms(INTRO_SPLASH_DURATION_MS);

    app->state.screen = ScreenMain;
    view_port_update(app->view_port);

    AppEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if(event.type == EventTypeInput) {
            switch(app->state.screen) {
            case ScreenSplash:
                break;

            case ScreenMain:
                if(event.input.key == InputKeyBack) {
                    app->state.screen = ScreenExitConfirm;
                    app->state.exit_selected = false;
                } else {
                    handle_main_input(app, &event.input);
                }
                break;

            case ScreenInfo:
                handle_info_input(app, &event.input);
                break;

            case ScreenHelp:
                handle_help_input(app, &event.input);
                break;

            case ScreenExitConfirm:
                handle_exit_input(app, &event.input);

                if(app->state.exit_confirmed) {
                    app->state.screen = ScreenExitSplash;
                }
                break;

            case ScreenExitSplash:
                break;
            }
        } else if(
            event.type == EventTypeTick && app->state.broadcasting &&
            app->state.screen == ScreenMain) {
            restart_beacon(app);
        }

        furi_mutex_release(app->mutex);

        if(app->state.screen == ScreenExitSplash) {
            view_port_update(app->view_port);
            furi_delay_ms(EXIT_SPLASH_DURATION_MS);
            running = false;
        } else {
            view_port_update(app->view_port);
        }
    }

    furi_hal_bt_extra_beacon_stop();

    furi_timer_stop(app->update_timer);
    furi_timer_free(app->update_timer);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
