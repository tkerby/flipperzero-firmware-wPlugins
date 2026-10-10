#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <stdlib.h>

#include "flipper_http/flipper_http.h"

#define TAG "SmartHome"

#define MAX_DEVICES 10
#define DEVICE_NAME_SIZE 32
#define DEVICE_REMOTE_ID_SIZE 64
#define URL_SIZE 320
#define HA_URL_SIZE 192
#define HA_TOKEN_SIZE 256
#define DEVICE_FILE APP_DATA_PATH("devices.txt")
#define SETTINGS_FILE APP_DATA_PATH("settings.txt")
#define HA_CONFIG_FILE APP_DATA_PATH("ha_config.txt")
#define LIGHT_COLOR_ACTION_BASE 3000
#define LIGHT_EFFECT_ACTION_BASE 4000


typedef enum {
    DeviceTypeHomeAssistantThermostat = 2,
    DeviceTypeHomeAssistantSwitch = 3,
    DeviceTypeHomeAssistantLight = 4,
} DeviceType;

typedef struct {
    DeviceType type;
    char name[DEVICE_NAME_SIZE];
    char remote_id[DEVICE_REMOTE_ID_SIZE];
} SmartHomeDevice;

typedef struct {
    void* app;
} SmartHomeViewModel;

typedef enum {
    SmartHomeViewMain,
    SmartHomeViewDevice,
    SmartHomeViewResult,
    SmartHomeViewTextInput,
    SmartHomeViewHaUrlKeyboard,
    SmartHomeViewStatus,
    SmartHomeViewLightSolid,
    SmartHomeViewLightEffects,
    SmartHomeViewDeviceType,
    SmartHomeViewSettings,
} SmartHomeView;

typedef enum {
    DeviceActionOn = 100,
    DeviceActionOff,
    DeviceActionToggle,

    DeviceActionLightStatus,
    DeviceActionLightBrightness25,
    DeviceActionLightBrightness50,
    DeviceActionLightBrightness75,
    DeviceActionLightBrightness100,
    DeviceActionLightRed,
    DeviceActionLightGreen,
    DeviceActionLightBlue,
    DeviceActionLightWhite,
    DeviceActionLightSolid,
    DeviceActionLightRainbow,
    DeviceActionLightSolidMenu,
    DeviceActionLightEffectsMenu,

    DeviceActionRename,
    DeviceActionDelete,

    DeviceActionThermostatStatus,
    DeviceActionThermostatTempUp,
    DeviceActionThermostatTempDown,
    DeviceActionThermostatSetTemperature,
    DeviceActionThermostatCool,
    DeviceActionThermostatHeat,
    DeviceActionThermostatAuto,
    DeviceActionThermostatOff,
    DeviceActionThermostatFanAuto,
    DeviceActionThermostatFanOn,
} DeviceAction;

typedef enum {
    InputModeNone,
    InputModeName,
    InputModeRename,
    InputModeSetTemperature,
    InputModeHaEntity,
    InputModeHaUrl,
    InputModeHaToken,
} InputMode;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    Submenu* main_menu;
    Submenu* device_menu;
    Submenu* light_solid_menu;
    Submenu* light_effect_menu;
    Submenu* device_type_menu;
    Submenu* settings_menu;
    DialogEx* dialog;
    TextInput* text_input;
    View* ha_url_keyboard_view;
    View* status_view;

    FlipperHTTP* fhttp;

    SmartHomeDevice devices[MAX_DEVICES];
    size_t device_count;
    size_t selected_device;
    DeviceType pending_device_type;

    char input_buffer[DEVICE_NAME_SIZE];
    char pending_name[DEVICE_NAME_SIZE];
    char entity_input[DEVICE_REMOTE_ID_SIZE];

    char ha_url[HA_URL_SIZE];
    char ha_token[HA_TOKEN_SIZE];
    char settings_input[HA_TOKEN_SIZE];

    char ha_url_keyboard_buffer[HA_URL_SIZE];
    char ha_url_keyboard_error[32];
    uint8_t ha_url_keyboard_row;
    uint8_t ha_url_keyboard_col;

    char status_title[32];
    char status_line1[48];
    char status_line2[48];
    char status_line3[48];

    uint32_t result_return_view;
    char result_header[64];
    char result_message[192];

    /*
     * Home Assistant light requests must never block a GUI callback.
     * The GUI thread fills these persistent fields, then a worker performs
     * the synchronous FlipperHTTP wait and reports completion through a
     * ViewDispatcher custom event.
     */
    FuriThread* ha_worker;
    bool ha_worker_running;
    bool ha_worker_success;
    bool ha_worker_is_get;
    uint32_t ha_worker_action;
    char ha_worker_status_state[16];
    char ha_worker_url[URL_SIZE];
    char ha_worker_payload[160];
    char ha_worker_title[32];

    InputMode input_mode;
} SmartHomeApp;

typedef struct {
    const char* name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SmartHomeLightColor;

static const SmartHomeLightColor smart_home_light_colors[] = {
    {"White", 255, 255, 255},
    {"Warm White", 255, 147, 41},
    {"Red", 255, 0, 0},
    {"Rose", 255, 64, 96},
    {"Pink", 255, 20, 147},
    {"Magenta", 255, 0, 255},
    {"Purple", 128, 0, 255},
    {"Indigo", 75, 0, 130},
    {"Blue", 0, 0, 255},
    {"Sky Blue", 0, 128, 255},
    {"Cyan", 0, 255, 255},
    {"Turquoise", 64, 224, 208},
    {"Mint", 0, 255, 128},
    {"Green", 0, 255, 0},
    {"Lime", 128, 255, 0},
    {"Yellow", 255, 255, 0},
    {"Amber", 255, 191, 0},
    {"Orange", 255, 96, 0},
};

static const char* const smart_home_light_effects[] = {
    "Blink",
    "Breathe",
    "Wipe",
    "Wipe Random",
    "Random Colors",
    "Sweep",
    "Dynamic",
    "Colorloop",
    "Rainbow",
    "Scan",
    "Scan Dual",
    "Fade",
    "Theater",
    "Theater Rainbow",
    "Running",
    "Saw",
    "Twinkle",
    "Dissolve",
    "Dissolve Rnd",
    "Sparkle",
    "Sparkle Dark",
    "Sparkle+",
    "Strobe",
    "Strobe Rainbow",
    "Strobe Mega",
    "Blink Rainbow",
    "Android",
    "Chase",
    "Chase Random",
    "Chase Rainbow",
    "Chase Flash",
    "Chase Flash Rnd",
    "Rainbow Runner",
    "Colorful",
    "Traffic Light",
    "Sweep Random",
    "Chase 2",
    "Aurora",
    "Stream",
    "Scanner",
    "Lighthouse",
    "Fireworks",
    "Rain",
    "Tetrix",
    "Fire Flicker",
    "Gradient",
    "Loading",
    "Rolling Balls",
    "Fairy",
    "Two Dots",
    "Fairytwinkle",
    "Running Dual",
    "Image",
    "Chase 3",
    "Tri Wipe",
    "Tri Fade",
    "Lightning",
    "ICU",
    "Multi Comet",
    "Scanner Dual",
    "Stream 2",
    "Oscillate",
    "Pride 2015",
    "Juggle",
    "Palette",
    "Fire 2012",
    "Colorwaves",
    "Bpm",
    "Fill Noise",
    "Noise 1",
    "Noise 2",
    "Noise 3",
    "Noise 4",
    "Colortwinkles",
    "Lake",
    "Meteor",
    "Copy Segment",
    "Railway",
    "Ripple",
    "Twinklefox",
    "Twinklecat",
    "Halloween Eyes",
    "Solid Pattern",
    "Solid Pattern Tri",
    "Spots",
    "Spots Fade",
    "Glitter",
    "Candle",
    "Fireworks Starburst",
    "Fireworks 1D",
    "Bouncing Balls",
    "Sinelon",
    "Sinelon Dual",
    "Sinelon Rainbow",
    "Popcorn",
    "Drip",
    "Plasma",
    "Percent",
    "Ripple Rainbow",
    "Heartbeat",
    "Pacifica",
    "Candle Multi",
    "Solid Glitter",
    "Sunrise",
    "Phased",
    "Twinkleup",
    "Noise Pal",
    "Sine",
    "Phased Noise",
    "Flow",
    "Chunchun",
    "Dancing Shadows",
    "Washing Machine",
    "Rotozoomer",
    "Blends",
    "TV Simulator",
    "Dynamic Smooth",
    "Spaceships",
    "Crazy Bees",
    "Ghost Rider",
    "Blobs",
    "Scrolling Text",
    "Drift Rose",
    "Distortion Waves",
    "Soap",
    "Octopus",
    "Waving Cell",
    "Pixels",
    "Pixelwave",
    "Juggles",
    "Matripix",
    "Gravimeter",
    "Plasmoid",
    "Puddles",
    "Midnoise",
    "Noisemeter",
    "Freqwave",
    "Freqmatrix",
    "GEQ",
    "Waterfall",
    "Freqpixels",
    "Noisefire",
    "Puddlepeak",
    "Noisemove",
    "Noise2D",
    "Perlin Move",
    "Ripple Peak",
    "Firenoise",
    "Squared Swirl",
    "PacMan",
    "DNA",
    "Matrix",
    "Metaballs",
    "Freqmap",
    "Gravcenter",
    "Gravcentric",
    "Gravfreq",
    "DJ Light",
    "Funky Plank",
    "Shimmer",
    "Pulser",
    "Blurz",
    "Drift",
    "Waverly",
    "Sun Radiation",
    "Colored Bursts",
    "Julia",
    "Game Of Life",
    "Tartan",
    "Polar Lights",
    "Swirl",
    "Lissajous",
    "Frizzles",
    "Plasma Ball",
    "Flow Stripe",
    "Hiphotic",
    "Sindots",
    "DNA Spiral",
    "Black Hole",
    "Wavesins",
    "Rocktaves",
    "Akemi",
    "PS Volcano",
    "PS Fire",
    "PS Fireworks",
    "PS Vortex",
    "PS Fuzzy Noise",
    "PS Ballpit",
    "PS Box",
    "PS Attractor",
    "PS Impact",
    "PS Waterfall",
    "PS Spray",
    "PS GEQ 2D",
    "PS GEQ Nova",
    "PS Ghost Rider",
    "PS Blobs",
    "PS DripDrop",
    "PS Pinball",
    "PS Dancing Shadows",
    "PS Fireworks 1D",
    "PS Sparkler",
    "PS Hourglass",
    "PS Spray 1D",
    "PS 1D Balance",
    "PS Chase",
    "PS Starburst",
    "PS GEQ 1D",
    "PS Fire 1D",
    "PS Sonic Stream",
    "PS Sonic Boom",
    "PS Springy",
    "PS Galaxy",
    "Color Clouds",
    "Slow Transition",
};

static const size_t smart_home_light_color_count =
    sizeof(smart_home_light_colors) /
    sizeof(smart_home_light_colors[0]);

static const size_t smart_home_light_effect_count =
    sizeof(smart_home_light_effects) /
    sizeof(smart_home_light_effects[0]);


static void smart_home_open_status_view(
    SmartHomeApp* app,
    const char* title,
    const char* line1,
    const char* line2,
    const char* line3);

static bool smart_home_start_ha_light_worker(
    SmartHomeApp* app,
    uint32_t action,
    const char* url,
    const char* payload,
    const char* title,
    bool is_get);

/* ---------- FlipperHTTP ---------- */

static bool smart_home_wait_for_board(FlipperHTTP* fhttp) {
    if(!flipper_http_send_command(fhttp, HTTP_CMD_PING)) {
        return false;
    }

    uint8_t counter = 20;

    while((fhttp->state == INACTIVE) && (--counter > 0)) {
        furi_delay_ms(100);
    }

    return counter > 0;
}

static bool smart_home_send_ha(SmartHomeApp* app, const char* url) {
    if(!app || !app->fhttp || !url ||
       app->ha_token[0] == '\0') {
        return false;
    }

    app->fhttp->state = IDLE;

    if(app->fhttp->last_response) {
        app->fhttp->last_response[0] = '\0';
    }

    char headers[320];
    snprintf(
        headers,
        sizeof(headers),
        "{\"Authorization\":\"Bearer %s\"}",
        app->ha_token);

    if(!flipper_http_request(app->fhttp, GET, url, headers, NULL)) {
        return false;
    }

    app->fhttp->state = RECEIVING;

    uint8_t counter = 50;

    while((app->fhttp->state != IDLE) &&
          (app->fhttp->state != ISSUE) &&
          (--counter > 0)) {
        furi_delay_ms(100);
    }

    if(app->fhttp->state == ISSUE) {
        return false;
    }

    return counter > 0;
}

static bool smart_home_send_ha_post(
    SmartHomeApp* app,
    const char* url,
    const char* payload) {

    if(!app || !app->fhttp || !url ||
       app->ha_token[0] == '\0') {
        return false;
    }

    app->fhttp->state = IDLE;

    if(app->fhttp->last_response) {
        app->fhttp->last_response[0] = '\0';
    }

    char headers[352];
    snprintf(
        headers,
        sizeof(headers),
        "{\"Authorization\":\"Bearer %s\","
        "\"Content-Type\":\"application/json\"}",
        app->ha_token);

    if(!flipper_http_request(
           app->fhttp,
           POST,
           url,
           headers,
           payload ? payload : "{}")) {
        return false;
    }

    app->fhttp->state = RECEIVING;

    uint8_t counter = 50;

    while((app->fhttp->state != IDLE) &&
          (app->fhttp->state != ISSUE) &&
          (--counter > 0)) {
        furi_delay_ms(100);
    }

    if(app->fhttp->state == ISSUE) {
        return false;
    }

    return counter > 0;
}

static bool smart_home_ha_ready(SmartHomeApp* app) {
    return app &&
           app->ha_url[0] != '\0' &&
           app->ha_token[0] != '\0';
}

static bool smart_home_extract_json_scalar(
    const char* json,
    const char* key,
    char* output,
    size_t output_size) {

    if(!json || !key || !output || output_size == 0) {
        return false;
    }

    char needle[64];

    int needle_len = snprintf(
        needle,
        sizeof(needle),
        "\"%s\"",
        key);

    if(needle_len <= 0 ||
       (size_t)needle_len >= sizeof(needle)) {
        return false;
    }

    const char* pos = strstr(json, needle);

    if(!pos) {
        return false;
    }

    pos += strlen(needle);

    while(*pos == ' ' ||
          *pos == '\t' ||
          *pos == '\r' ||
          *pos == '\n') {
        pos++;
    }

    if(*pos != ':') {
        return false;
    }

    pos++;

    while(*pos == ' ' ||
          *pos == '\t' ||
          *pos == '\r' ||
          *pos == '\n') {
        pos++;
    }

    bool quoted = false;

    if(*pos == '"') {
        quoted = true;
        pos++;
    }

    const char* end = pos;

    if(quoted) {
        while(*end && *end != '"') {
            end++;
        }
    } else {
        while(*end &&
              *end != ',' &&
              *end != '}' &&
              *end != ']' &&
              *end != '\r' &&
              *end != '\n') {
            end++;
        }

        while(end > pos &&
              (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
    }

    if(end <= pos) {
        return false;
    }

    size_t len = (size_t)(end - pos);

    if(len >= output_size) {
        len = output_size - 1;
    }

    memcpy(output, pos, len);
    output[len] = '\0';

    return true;
}


/* ---------- Storage ---------- */

static const char* smart_home_device_type_name(DeviceType type) {
    switch(type) {
    case DeviceTypeHomeAssistantThermostat:
        return "ha_thermostat";
    case DeviceTypeHomeAssistantSwitch:
        return "ha_switch";
    case DeviceTypeHomeAssistantLight:
        return "ha_light";
    default:
        return "unknown";
    }
}

static bool smart_home_save_devices(SmartHomeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool ok = storage_file_open(
        file,
        DEVICE_FILE,
        FSAM_WRITE,
        FSOM_CREATE_ALWAYS);

    if(ok) {
        for(size_t i = 0; i < app->device_count; i++) {
            char line[
                16 +
                DEVICE_NAME_SIZE +
                DEVICE_REMOTE_ID_SIZE +
                8];

            int len = snprintf(
                line,
                sizeof(line),
                "%s|%s|%s\n",
                smart_home_device_type_name(app->devices[i].type),
                app->devices[i].name,
                app->devices[i].remote_id);

            if(len > 0 && (size_t)len < sizeof(line)) {
                storage_file_write(file, line, (size_t)len);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return ok;
}

static void smart_home_load_devices(SmartHomeApp* app) {
    app->device_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_file_exists(storage, DEVICE_FILE)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);

    bool opened = storage_file_open(
        file,
        DEVICE_FILE,
        FSAM_READ,
        FSOM_OPEN_EXISTING);

    if(opened) {
        uint64_t file_size = storage_file_size(file);

        if(file_size > 0 && file_size < 4096) {
            char* data = malloc((size_t)file_size + 1);

            if(data) {
                size_t read = storage_file_read(file, data, (size_t)file_size);
                data[read] = '\0';

                char* line = data;

                while(*line && app->device_count < MAX_DEVICES) {
                    char* newline = strchr(line, '\n');
                    if(newline) {
                        *newline = '\0';
                    }

                    char* first = strchr(line, '|');

                    if(first) {
                        *first = '\0';
                        const char* type = line;
                        char* remainder = first + 1;
                        char* second = strchr(remainder, '|');

                        if(second) {
                            *second = '\0';
                            const char* name = remainder;
                            char* third_field = second + 1;
                            char* third_sep = strchr(third_field, '|');
                            const char* remote_id = third_field;

                            /* Migrate Stage 1's four-field HA format:
                             * type|name|unused_ip|entity_id
                             * Current format is type|name|entity_id. */
                            if(third_sep) {
                                *third_sep = '\0';
                                remote_id = third_sep + 1;
                            }

                            DeviceType parsed_type = 0;

                            if(strcmp(type, "ha_thermostat") == 0) {
                                parsed_type = DeviceTypeHomeAssistantThermostat;
                            } else if(strcmp(type, "ha_switch") == 0) {
                                parsed_type = DeviceTypeHomeAssistantSwitch;
                            } else if(strcmp(type, "ha_light") == 0) {
                                parsed_type = DeviceTypeHomeAssistantLight;
                            }

                            if(parsed_type != 0 &&
                               strlen(name) > 0 &&
                               strlen(remote_id) > 0) {

                                SmartHomeDevice* device =
                                    &app->devices[app->device_count];

                                memset(device, 0, sizeof(SmartHomeDevice));
                                device->type = parsed_type;

                                strlcpy(
                                    device->name,
                                    name,
                                    sizeof(device->name));

                                strlcpy(
                                    device->remote_id,
                                    remote_id,
                                    sizeof(device->remote_id));

                                app->device_count++;
                            }
                        }
                    }

                    if(!newline) {
                        break;
                    }

                    line = newline + 1;
                }

                free(data);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Rewrite the file in the current compact HA-only format.
     * Unsupported legacy records are intentionally discarded. */
    smart_home_save_devices(app);
}

/* ---------- Settings storage ---------- */

static bool smart_home_save_settings(SmartHomeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool ok = storage_file_open(
        file,
        SETTINGS_FILE,
        FSAM_WRITE,
        FSOM_CREATE_ALWAYS);

    if(ok) {
        char line[HA_TOKEN_SIZE + 32];

        int len = snprintf(
            line,
            sizeof(line),
            "ha_url=%s\n",
            app->ha_url);

        if(len > 0 && (size_t)len < sizeof(line)) {
            storage_file_write(file, line, (size_t)len);
        }

        len = snprintf(
            line,
            sizeof(line),
            "ha_token=%s\n",
            app->ha_token);

        if(len > 0 && (size_t)len < sizeof(line)) {
            storage_file_write(file, line, (size_t)len);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return ok;
}

static void smart_home_load_settings(SmartHomeApp* app) {
    app->ha_url[0] = '\0';
    app->ha_token[0] = '\0';
    bool loaded = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_file_exists(storage, SETTINGS_FILE)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    File* file = storage_file_alloc(storage);

    bool opened = storage_file_open(
        file,
        SETTINGS_FILE,
        FSAM_READ,
        FSOM_OPEN_EXISTING);

    if(opened) {
        uint64_t file_size = storage_file_size(file);

        if(file_size > 0 && file_size < 2048) {
            char* data = malloc((size_t)file_size + 1);

            if(data) {
                size_t read = storage_file_read(file, data, (size_t)file_size);
                data[read] = '\0';

                char* line = data;

                while(*line) {
                    char* newline = strchr(line, '\n');
                    if(newline) {
                        *newline = '\0';
                    }

                    if(strncmp(
                           line,
                           "ha_url=",
                           strlen("ha_url=")) == 0) {

                        strlcpy(
                            app->ha_url,
                            line + strlen("ha_url="),
                            sizeof(app->ha_url));

                    } else if(strncmp(
                                  line,
                                  "ha_token=",
                                  strlen("ha_token=")) == 0) {

                        strlcpy(
                            app->ha_token,
                            line + strlen("ha_token="),
                            sizeof(app->ha_token));
                    }

                    if(!newline) {
                        break;
                    }

                    line = newline + 1;
                }

                loaded = true;
                free(data);
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    /* Rewrite successfully loaded settings in the current HA-only format. */
    if(loaded) {
        smart_home_save_settings(app);
    }
}

/* ---------- UI helpers ---------- */

static uint32_t smart_home_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t smart_home_device_back(void* context) {
    UNUSED(context);
    return SmartHomeViewMain;
}

static uint32_t smart_home_result_return_view =
    SmartHomeViewDevice;

static uint32_t smart_home_result_back(void* context) {
    UNUSED(context);

    if(smart_home_result_return_view != VIEW_NONE) {
        return smart_home_result_return_view;
    }

    return SmartHomeViewDevice;
}

static uint32_t smart_home_input_back(void* context) {
    UNUSED(context);
    return SmartHomeViewMain;
}

static void smart_home_show_result(
    SmartHomeApp* app,
    const char* header,
    const char* message) {

    app->result_return_view = SmartHomeViewDevice;
    smart_home_result_return_view = SmartHomeViewDevice;

    strlcpy(
        app->result_header,
        header ? header : "",
        sizeof(app->result_header));

    strlcpy(
        app->result_message,
        message ? message : "",
        sizeof(app->result_message));

    dialog_ex_set_header(
        app->dialog,
        app->result_header,
        64,
        12,
        AlignCenter,
        AlignCenter);

    dialog_ex_set_text(
        app->dialog,
        app->result_message,
        64,
        34,
        AlignCenter,
        AlignCenter);

    dialog_ex_set_center_button_text(app->dialog, "OK");

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewResult);
}

/* ---------- Home Assistant URL keyboard ---------- */

#define HA_URL_KEYBOARD_ROWS 4
#define HA_URL_KEYBOARD_COLS 11

static const char* const smart_home_ha_url_keyboard_rows[HA_URL_KEYBOARD_ROWS] = {
    "1234567890-",
    "qwertyuiop.",
    "asdfghjkl_:",
    "zxcvbnm/[]\b",
};

static bool smart_home_validate_ha_base_url(
    const char* url,
    char* error,
    size_t error_size) {

    if(!url || !error || error_size == 0) {
        return false;
    }

    error[0] = '\0';

    const char* host = NULL;

    if(strncmp(url, "https://", 8) == 0) {
        host = url + 8;
    } else if(strncmp(url, "http://", 7) == 0) {
        host = url + 7;
    } else {
        strlcpy(error, "Use http:// or https://", error_size);
        return false;
    }

    if(*host == '\0') {
        strlcpy(error, "Enter a host", error_size);
        return false;
    }

    const char* slash = strchr(host, '/');
    if(slash && slash[1] != '\0') {
        strlcpy(error, "Use the base URL only", error_size);
        return false;
    }

    size_t authority_len = slash ? (size_t)(slash - host) : strlen(host);
    if(authority_len == 0) {
        strlcpy(error, "Enter a host", error_size);
        return false;
    }

    const char* colon = NULL;

    for(size_t i = 0; i < authority_len; i++) {
        char c = host[i];

        if(c == ':') {
            if(colon) {
                strlcpy(error, "Only one port separator", error_size);
                return false;
            }
            colon = host + i;
            continue;
        }

        bool valid =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_';

        if(!valid) {
            strlcpy(error, "Invalid host character", error_size);
            return false;
        }
    }

    size_t host_len = colon ? (size_t)(colon - host) : authority_len;
    if(host_len == 0) {
        strlcpy(error, "Enter a host", error_size);
        return false;
    }

    if(host[0] == '.' || host[host_len - 1] == '.') {
        strlcpy(error, "Host cannot start/end .", error_size);
        return false;
    }

    if(colon) {
        const char* port = colon + 1;
        const char* authority_end = host + authority_len;

        if(port >= authority_end) {
            strlcpy(error, "Enter a port", error_size);
            return false;
        }

        unsigned long value = 0;

        for(const char* p = port; p < authority_end; p++) {
            if(*p < '0' || *p > '9') {
                strlcpy(error, "Port must be numeric", error_size);
                return false;
            }

            value = (value * 10UL) + (unsigned long)(*p - '0');
            if(value > 65535UL) {
                strlcpy(error, "Port must be 1-65535", error_size);
                return false;
            }
        }

        if(value == 0) {
            strlcpy(error, "Port must be 1-65535", error_size);
            return false;
        }
    }

    return true;
}

static char* smart_home_custom_keyboard_buffer(SmartHomeApp* app);
static size_t smart_home_custom_keyboard_capacity(SmartHomeApp* app);

static void smart_home_ha_url_keyboard_draw_callback(
    Canvas* canvas,
    void* model_ptr) {

    SmartHomeViewModel* model = model_ptr;
    SmartHomeApp* app = model->app;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas,
        2,
        8,
        app->input_mode == InputModeHaEntity ?
            "HA Entity  Hold OK=Save" :
            "HA URL   Hold OK=Save");

    canvas_set_font(canvas, FontKeyboard);

    if(app->ha_url_keyboard_error[0] != '\0') {
        canvas_draw_str(canvas, 2, 18, app->ha_url_keyboard_error);
    } else {
        char* active_buffer = smart_home_custom_keyboard_buffer(app);
        size_t len = strlen(active_buffer);
        const char* visible = active_buffer;
        const size_t visible_chars = 17;

        /* Scroll early enough that the characters currently being typed
         * remain visible instead of disappearing off the right edge. */
        if(len > visible_chars) {
            visible += len - visible_chars;
            canvas_draw_str(canvas, 2, 18, "<");
            canvas_draw_str(canvas, 8, 18, visible);
        } else {
            canvas_draw_str(canvas, 2, 18, visible);
        }
    }

    for(uint8_t row = 0; row < HA_URL_KEYBOARD_ROWS; row++) {
        const char* row_text = smart_home_ha_url_keyboard_rows[row];
        size_t row_len = strlen(row_text);
        int32_t start_x = (128 - ((int32_t)row_len * 10)) / 2;
        int32_t baseline_y = 30 + ((int32_t)row * 10);

        for(uint8_t col = 0; col < row_len; col++) {
            int32_t x = start_x + ((int32_t)col * 10);
            char key[2] = {row_text[col], '\0'};
            bool is_backspace = row_text[col] == '\b';
            bool is_underscore = row_text[col] == '_';

            const char* key_text = is_backspace ? "<-" : key;
            int32_t key_width = is_backspace ? 16 : 9;
            int32_t text_x = is_backspace ? (x + 1) : (x + 2);
            int32_t text_y = baseline_y - (is_underscore ? 2 : 0);

            if(row == app->ha_url_keyboard_row &&
               col == app->ha_url_keyboard_col) {

                canvas_draw_box(
                    canvas,
                    x,
                    baseline_y - 8,
                    key_width,
                    9);

                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(
                    canvas,
                    text_x,
                    text_y,
                    key_text);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(
                    canvas,
                    text_x,
                    text_y,
                    key_text);
            }
        }
    }
}


static bool smart_home_import_ha_config(
    SmartHomeApp* app,
    char* error,
    size_t error_size) {

    if(!app || !error || error_size == 0) {
        return false;
    }

    error[0] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_file_exists(storage, HA_CONFIG_FILE)) {
        strlcpy(
            error,
            "ha_config.txt not found",
            error_size);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    File* file = storage_file_alloc(storage);

    bool opened = storage_file_open(
        file,
        HA_CONFIG_FILE,
        FSAM_READ,
        FSOM_OPEN_EXISTING);

    if(!opened) {
        strlcpy(
            error,
            "Could not open config",
            error_size);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint64_t file_size = storage_file_size(file);

    if(file_size == 0 || file_size >= 2048) {
        strlcpy(
            error,
            "Invalid config size",
            error_size);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char* data = malloc((size_t)file_size + 1);

    if(!data) {
        strlcpy(
            error,
            "Out of memory",
            error_size);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    size_t read =
        storage_file_read(
            file,
            data,
            (size_t)file_size);

    data[read] = '\0';

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    char imported_url[HA_URL_SIZE] = {0};
    char imported_token[HA_TOKEN_SIZE] = {0};

    char* line = data;

    while(*line) {
        char* newline = strchr(line, '\n');

        if(newline) {
            *newline = '\0';
        }

        size_t line_len = strlen(line);

        if(line_len > 0 && line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
        }

        if(strncmp(
               line,
               "ha_url=",
               strlen("ha_url=")) == 0) {

            const char* value =
                line + strlen("ha_url=");

            if(strlen(value) >= sizeof(imported_url)) {
                strlcpy(
                    error,
                    "HA URL too long",
                    error_size);
                free(data);
                return false;
            }

            strlcpy(
                imported_url,
                value,
                sizeof(imported_url));

        } else if(strncmp(
                      line,
                      "ha_token=",
                      strlen("ha_token=")) == 0) {

            const char* value =
                line + strlen("ha_token=");

            if(strlen(value) >= sizeof(imported_token)) {
                strlcpy(
                    error,
                    "HA token too long",
                    error_size);
                free(data);
                return false;
            }

            strlcpy(
                imported_token,
                value,
                sizeof(imported_token));
        }

        if(!newline) {
            break;
        }

        line = newline + 1;
    }

    free(data);

    if(imported_url[0] == '\0' &&
       imported_token[0] == '\0') {

        strlcpy(
            error,
            "No HA values found",
            error_size);
        return false;
    }

    if(imported_url[0] != '\0') {
        char url_error[64];

        if(!smart_home_validate_ha_base_url(
               imported_url,
               url_error,
               sizeof(url_error))) {

            strlcpy(
                error,
                url_error,
                error_size);
            return false;
        }
    }

    if(imported_url[0] != '\0') {
        size_t len = strlen(imported_url);

        if(len > 8 &&
           imported_url[len - 1] == '/') {
            imported_url[len - 1] = '\0';
        }

        strlcpy(
            app->ha_url,
            imported_url,
            sizeof(app->ha_url));

    }

    if(imported_token[0] != '\0') {
        strlcpy(
            app->ha_token,
            imported_token,
            sizeof(app->ha_token));
    }

    if(!smart_home_save_settings(app)) {
        strlcpy(
            error,
            "Could not save settings",
            error_size);
        return false;
    }

    return true;
}

static char* smart_home_custom_keyboard_buffer(SmartHomeApp* app) {
    return app->input_mode == InputModeHaEntity ?
        app->entity_input :
        app->ha_url_keyboard_buffer;
}

static size_t smart_home_custom_keyboard_capacity(SmartHomeApp* app) {
    return app->input_mode == InputModeHaEntity ?
        sizeof(app->entity_input) :
        sizeof(app->ha_url_keyboard_buffer);
}

static void smart_home_ha_url_keyboard_redraw(SmartHomeApp* app) {
    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewHaUrlKeyboard);
}

static void smart_home_ha_entity_entered(void* context);

static void smart_home_ha_url_keyboard_save(SmartHomeApp* app) {
    if(app->input_mode == InputModeHaEntity) {
        smart_home_ha_entity_entered(app);
        return;
    }

    size_t len = strlen(app->ha_url_keyboard_buffer);

    /* A single trailing slash after a real host is harmless, but would
     * produce //api when service paths are appended. Never trim the slash
     * characters that belong to the URI scheme itself. */
    size_t scheme_len = 0;
    if(strncmp(app->ha_url_keyboard_buffer, "https://", 8) == 0) {
        scheme_len = 8;
    } else if(strncmp(app->ha_url_keyboard_buffer, "http://", 7) == 0) {
        scheme_len = 7;
    }

    if(len > scheme_len &&
       scheme_len > 0 &&
       app->ha_url_keyboard_buffer[len - 1] == '/') {
        app->ha_url_keyboard_buffer[len - 1] = '\0';
    }

    if(!smart_home_validate_ha_base_url(
           app->ha_url_keyboard_buffer,
           app->ha_url_keyboard_error,
           sizeof(app->ha_url_keyboard_error))) {

        smart_home_ha_url_keyboard_redraw(app);
        return;
    }

    strlcpy(
        app->ha_url,
        app->ha_url_keyboard_buffer,
        sizeof(app->ha_url));

    smart_home_save_settings(app);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewSettings);
}

static bool smart_home_ha_url_keyboard_input_callback(
    InputEvent* event,
    void* context) {

    SmartHomeApp* app = context;

    if(event->type == InputTypeLong) {
        if(event->key == InputKeyOk) {
            smart_home_ha_url_keyboard_save(app);
            return true;
        }

        if(event->key == InputKeyBack) {
            view_dispatcher_switch_to_view(
                app->view_dispatcher,
                app->input_mode == InputModeHaEntity ?
                    SmartHomeViewMain :
                    SmartHomeViewSettings);
            return true;
        }

        return false;
    }

    if(event->type != InputTypeShort &&
       event->type != InputTypeRepeat) {
        return false;
    }

    bool handled = true;

    /* Any fresh edit/navigation clears a previous validation message. */
    app->ha_url_keyboard_error[0] = '\0';

    switch(event->key) {
    case InputKeyLeft: {
        size_t row_len = strlen(
            smart_home_ha_url_keyboard_rows[app->ha_url_keyboard_row]);

        if(app->ha_url_keyboard_col == 0) {
            app->ha_url_keyboard_col = (uint8_t)(row_len - 1);
        } else {
            app->ha_url_keyboard_col--;
        }
        break;
    }

    case InputKeyRight: {
        size_t row_len = strlen(
            smart_home_ha_url_keyboard_rows[app->ha_url_keyboard_row]);

        app->ha_url_keyboard_col =
            (uint8_t)((app->ha_url_keyboard_col + 1) % row_len);
        break;
    }

    case InputKeyUp:
        if(event->type == InputTypeShort) {
            app->ha_url_keyboard_row =
                (app->ha_url_keyboard_row == 0) ?
                    (HA_URL_KEYBOARD_ROWS - 1) :
                    (app->ha_url_keyboard_row - 1);

            size_t row_len = strlen(
                smart_home_ha_url_keyboard_rows[app->ha_url_keyboard_row]);

            if(app->ha_url_keyboard_col >= row_len) {
                app->ha_url_keyboard_col = (uint8_t)(row_len - 1);
            }
        }
        break;

    case InputKeyDown:
        if(event->type == InputTypeShort) {
            app->ha_url_keyboard_row =
                (uint8_t)((app->ha_url_keyboard_row + 1) % HA_URL_KEYBOARD_ROWS);

            size_t row_len = strlen(
                smart_home_ha_url_keyboard_rows[app->ha_url_keyboard_row]);

            if(app->ha_url_keyboard_col >= row_len) {
                app->ha_url_keyboard_col = (uint8_t)(row_len - 1);
            }
        }
        break;

    case InputKeyOk:
        if(event->type == InputTypeShort) {
            const char* row_text =
                smart_home_ha_url_keyboard_rows[app->ha_url_keyboard_row];
            char selected_key =
                row_text[app->ha_url_keyboard_col];
            char* active_buffer =
                smart_home_custom_keyboard_buffer(app);
            size_t capacity =
                smart_home_custom_keyboard_capacity(app);
            size_t len = strlen(active_buffer);

            if(selected_key == '\b') {
                if(len > 0) {
                    active_buffer[len - 1] = '\0';
                }
            } else if(len + 1 < capacity) {
                active_buffer[len] = selected_key;
                active_buffer[len + 1] = '\0';
            } else {
                strlcpy(
                    app->ha_url_keyboard_error,
                    app->input_mode == InputModeHaEntity ?
                        "Entity is too long" :
                        "URL is too long",
                    sizeof(app->ha_url_keyboard_error));
            }
        }
        break;

    case InputKeyBack:
        if(event->type == InputTypeShort) {
            char* active_buffer =
                smart_home_custom_keyboard_buffer(app);
            size_t len = strlen(active_buffer);
            if(len > 0) {
                active_buffer[len - 1] = '\0';
            }
        }
        break;

    default:
        handled = false;
        break;
    }

    if(handled) {
        smart_home_ha_url_keyboard_redraw(app);
    }

    return handled;
}

static void smart_home_open_ha_url_keyboard(SmartHomeApp* app) {
    memset(
        app->ha_url_keyboard_buffer,
        0,
        sizeof(app->ha_url_keyboard_buffer));

    memset(
        app->ha_url_keyboard_error,
        0,
        sizeof(app->ha_url_keyboard_error));

    if(app->ha_url[0] != '\0') {
        strlcpy(
            app->ha_url_keyboard_buffer,
            app->ha_url,
            sizeof(app->ha_url_keyboard_buffer));
    } else {
        strlcpy(
            app->ha_url_keyboard_buffer,
            "https://",
            sizeof(app->ha_url_keyboard_buffer));
    }

    app->ha_url_keyboard_row = 1;
    app->ha_url_keyboard_col = 0;

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewHaUrlKeyboard);
}

static void smart_home_show_result_to(
    SmartHomeApp* app,
    const char* header,
    const char* message,
    uint32_t return_view) {

    smart_home_show_result(app, header, message);
    app->result_return_view = return_view;
    smart_home_result_return_view = return_view;
}

static void smart_home_refresh_main_menu(SmartHomeApp* app);

static void smart_home_open_device(
    SmartHomeApp* app,
    size_t index) {

    if(index >= app->device_count) {
        return;
    }

    app->selected_device = index;

    submenu_reset(app->device_menu);
    submenu_set_header(
        app->device_menu,
        app->devices[index].name);
}

/* ---------- Input ---------- */

static bool smart_home_valid_ha_entity(
    const char* entity_id,
    const char* domain) {

    if(!entity_id || !domain) {
        return false;
    }

    size_t domain_len = strlen(domain);

    if(strncmp(entity_id, domain, domain_len) != 0 ||
       entity_id[domain_len] != '.') {
        return false;
    }

    const char* p = entity_id + domain_len + 1;

    if(*p == '\0') {
        return false;
    }

    for(; *p; p++) {
        bool valid =
            (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') ||
            (*p == '_');

        if(!valid) {
            return false;
        }
    }

    return true;
}

static bool smart_home_valid_climate_entity(
    const char* entity_id) {
    return smart_home_valid_ha_entity(
        entity_id,
        "climate");
}

static bool smart_home_valid_switch_entity(
    const char* entity_id) {
    return smart_home_valid_ha_entity(
        entity_id,
        "switch");
}

static bool smart_home_valid_light_entity(
    const char* entity_id) {
    return smart_home_valid_ha_entity(
        entity_id,
        "light");
}


static void smart_home_ha_entity_entered(
    void* context) {

    SmartHomeApp* app = context;

    if(app->device_count >= MAX_DEVICES) {
        smart_home_show_result(
            app,
            "Smart Home",
            "Device list full");
        return;
    }

    bool valid = false;
    const char* expected = "HA entity";

    if(app->pending_device_type ==
       DeviceTypeHomeAssistantThermostat) {

        valid = smart_home_valid_climate_entity(
            app->entity_input);
        expected = "Use climate.name";

    } else if(app->pending_device_type ==
              DeviceTypeHomeAssistantSwitch) {

        valid = smart_home_valid_switch_entity(
            app->entity_input);
        expected = "Use switch.name";

    } else if(app->pending_device_type ==
              DeviceTypeHomeAssistantLight) {

        valid = smart_home_valid_light_entity(
            app->entity_input);
        expected = "Use light.name";
    }

    if(!valid) {
        /*
         * Invalid/empty Home Assistant entity IDs are ordinary input errors.
         * Keep the user on the custom entity keyboard and render the error
         * inline. Never enter DialogEx or another navigation path here.
         */
        if(app->entity_input[0] == '\0') {
            strlcpy(
                app->ha_url_keyboard_error,
                "Entity ID required",
                sizeof(app->ha_url_keyboard_error));
        } else {
            strlcpy(
                app->ha_url_keyboard_error,
                expected,
                sizeof(app->ha_url_keyboard_error));
        }

        smart_home_ha_url_keyboard_redraw(app);
        return;
    }

    SmartHomeDevice* device =
        &app->devices[app->device_count];

    memset(
        device,
        0,
        sizeof(SmartHomeDevice));

    device->type =
        app->pending_device_type;

    strlcpy(
        device->name,
        app->pending_name,
        sizeof(device->name));

    strlcpy(
        device->remote_id,
        app->entity_input,
        sizeof(device->remote_id));

    app->device_count++;

    smart_home_save_devices(app);
    smart_home_refresh_main_menu(app);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewMain);
}


static void smart_home_name_entered(void* context) {
    SmartHomeApp* app = context;

    if(strlen(app->input_buffer) == 0) {
        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewMain);
        return;
    }

    strlcpy(
        app->pending_name,
        app->input_buffer,
        sizeof(app->pending_name));

    if(!smart_home_ha_ready(app)) {
        smart_home_show_result_to(
            app,
            "Home Assistant",
            app->ha_url[0] == '\0' ?
                "Set HA URL first" :
                "Set HA token first",
            SmartHomeViewMain);
        return;
    }

    memset(
        app->entity_input,
        0,
        sizeof(app->entity_input));

    const char* prefix =
        app->pending_device_type ==
                DeviceTypeHomeAssistantThermostat ?
            "climate." :
        app->pending_device_type ==
                DeviceTypeHomeAssistantLight ?
            "light." :
            "switch.";

    strlcpy(
        app->entity_input,
        prefix,
        sizeof(app->entity_input));

    app->input_mode = InputModeHaEntity;

    memset(
        app->ha_url_keyboard_error,
        0,
        sizeof(app->ha_url_keyboard_error));

    app->ha_url_keyboard_row = 1;
    app->ha_url_keyboard_col = 0;

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewHaUrlKeyboard);
}

static void smart_home_rename_entered(void* context) {
    SmartHomeApp* app = context;

    if(app->selected_device >= app->device_count) {
        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewMain);
        return;
    }

    if(strlen(app->input_buffer) == 0) {
        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewDevice);
        return;
    }

    strlcpy(
        app->devices[app->selected_device].name,
        app->input_buffer,
        sizeof(app->devices[app->selected_device].name));

    smart_home_save_devices(app);
    smart_home_refresh_main_menu(app);

    /*
     * Update the device-menu header too, so the new name is
     * visible immediately without leaving/reopening the app.
     */
    submenu_set_header(
        app->device_menu,
        app->devices[app->selected_device].name);

    smart_home_show_result(
        app,
        "Smart Home",
        "Device renamed");
}

static void smart_home_set_temperature_entered(void* context) {
    SmartHomeApp* app = context;

    if(app->selected_device >= app->device_count) {
        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewMain);
        return;
    }

    SmartHomeDevice* device =
        &app->devices[app->selected_device];

    if(device->type !=
       DeviceTypeHomeAssistantThermostat) {
        return;
    }

    char* end = NULL;

    float temperature =
        strtof(app->input_buffer, &end);

    if(end == app->input_buffer ||
       *end != '\0' ||
       temperature < 50.0f ||
       temperature > 90.0f) {

        smart_home_show_result(
            app,
            "Invalid Temperature",
            "Enter 50 to 90 F");
        return;
    }

    if(!smart_home_ha_ready(app)) {
        smart_home_show_result(
            app,
            "Home Assistant",
            "Configure HA Settings");
        return;
    }

    if(app->ha_worker_running) {
        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Request already running",
            "",
            "");
        return;
    }

    char url[URL_SIZE];
    char payload[160];

    snprintf(
        url,
        sizeof(url),
        "%s/api/services/climate/set_temperature",
        app->ha_url);

    snprintf(
        payload,
        sizeof(payload),
        "{\"entity_id\":\"%s\",\"temperature\":%.1f}",
        device->remote_id,
        (double)temperature);

    if(!smart_home_start_ha_light_worker(
           app,
           DeviceActionThermostatSetTemperature,
           url,
           payload,
           device->name,
           false)) {

        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Could not start request",
            "",
            "");
    }
}

static void smart_home_begin_set_temperature(
    SmartHomeApp* app) {

    if(app->selected_device >= app->device_count) {
        return;
    }

    memset(
        app->input_buffer,
        0,
        sizeof(app->input_buffer));

    /*
     * Convenient starting value. It remains fully editable.
     */
    strlcpy(
        app->input_buffer,
        "75",
        sizeof(app->input_buffer));

    app->input_mode =
        InputModeSetTemperature;

    text_input_reset(app->text_input);

    text_input_set_header_text(
        app->text_input,
        "Set temperature F");

    text_input_set_result_callback(
        app->text_input,
        smart_home_set_temperature_entered,
        app,
        app->input_buffer,
        DEVICE_NAME_SIZE,
        false);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewTextInput);
}


static void smart_home_begin_rename_device(SmartHomeApp* app) {
    if(app->selected_device >= app->device_count) {
        return;
    }

    strlcpy(
        app->input_buffer,
        app->devices[app->selected_device].name,
        sizeof(app->input_buffer));

    app->input_mode = InputModeRename;

    text_input_reset(app->text_input);

    text_input_set_header_text(
        app->text_input,
        "Rename device");

    text_input_set_result_callback(
        app->text_input,
        smart_home_rename_entered,
        app,
        app->input_buffer,
        DEVICE_NAME_SIZE,
        false);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewTextInput);
}

static void smart_home_begin_device_details(SmartHomeApp* app);

static void smart_home_device_type_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    switch(index) {
    case DeviceTypeHomeAssistantThermostat:
    case DeviceTypeHomeAssistantSwitch:
    case DeviceTypeHomeAssistantLight:
        if(!smart_home_ha_ready(app)) {
            smart_home_show_result_to(
                app,
                "Home Assistant",
                app->ha_url[0] == '\0' ?
                    "Set HA URL first" :
                    "Set HA token first",
                SmartHomeViewMain);
            return;
        }

        app->pending_device_type = (DeviceType)index;
        smart_home_begin_device_details(app);
        break;

    default:
        break;
    }
}

static void smart_home_begin_add_device(SmartHomeApp* app) {
    if(app->device_count >= MAX_DEVICES) {
        smart_home_show_result(
            app,
            "Smart Home",
            "Device list full");
        return;
    }

    submenu_reset(app->device_type_menu);
    submenu_set_header(
        app->device_type_menu,
        "Select Device Type");

    submenu_add_item(
        app->device_type_menu,
        "Home Assistant Thermostat",
        DeviceTypeHomeAssistantThermostat,
        smart_home_device_type_callback,
        app);

    submenu_add_item(
        app->device_type_menu,
        "Home Assistant Switch",
        DeviceTypeHomeAssistantSwitch,
        smart_home_device_type_callback,
        app);

    submenu_add_item(
        app->device_type_menu,
        "Home Assistant Light",
        DeviceTypeHomeAssistantLight,
        smart_home_device_type_callback,
        app);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewDeviceType);
}

static void smart_home_begin_device_details(SmartHomeApp* app) {
    if(app->device_count >= MAX_DEVICES) {
        smart_home_show_result(
            app,
            "Smart Home",
            "Device list full");
        return;
    }

    memset(app->input_buffer, 0, sizeof(app->input_buffer));
    memset(app->pending_name, 0, sizeof(app->pending_name));

    app->input_mode = InputModeName;

    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input,
        "Device name");

    text_input_set_result_callback(
        app->text_input,
        smart_home_name_entered,
        app,
        app->input_buffer,
        DEVICE_NAME_SIZE,
        false);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewTextInput);
}

/* ---------- Settings UI ---------- */

#define SETTINGS_INDEX 1001
#define SETTINGS_HA_URL_INDEX 2003
#define SETTINGS_HA_TOKEN_INDEX 2004
#define SETTINGS_HA_IMPORT_INDEX 2005

static void smart_home_open_settings(SmartHomeApp* app);

static uint32_t smart_home_settings_back(void* context) {
    UNUSED(context);
    return SmartHomeViewMain;
}

static void smart_home_ha_token_entered(void* context) {
    SmartHomeApp* app = context;

    if(strlen(app->settings_input) > 0) {
        strlcpy(
            app->ha_token,
            app->settings_input,
            sizeof(app->ha_token));

        smart_home_save_settings(app);
    }

    /* Do not leave the token visible in the reusable input buffer. */
    memset(
        app->settings_input,
        0,
        sizeof(app->settings_input));

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewSettings);
}

static void smart_home_settings_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    if(index == SETTINGS_HA_URL_INDEX) {
        app->input_mode = InputModeHaUrl;
        smart_home_open_ha_url_keyboard(app);
        return;
    }

    memset(
        app->settings_input,
        0,
        sizeof(app->settings_input));

    text_input_reset(app->text_input);

    if(index == SETTINGS_HA_TOKEN_INDEX) {
        app->input_mode = InputModeHaToken;

        strlcpy(
            app->settings_input,
            app->ha_token,
            sizeof(app->settings_input));

        text_input_set_header_text(
            app->text_input,
            "Edit HA Token");

        text_input_set_result_callback(
            app->text_input,
            smart_home_ha_token_entered,
            app,
            app->settings_input,
            HA_TOKEN_SIZE,
            false);

    } else if(index == SETTINGS_HA_IMPORT_INDEX) {
        char import_error[64];

        if(smart_home_import_ha_config(
               app,
               import_error,
               sizeof(import_error))) {

            smart_home_show_result_to(
                app,
                "Home Assistant",
                "HA config imported",
                SmartHomeViewSettings);
        } else {
            smart_home_show_result_to(
                app,
                "Import Failed",
                import_error,
                SmartHomeViewSettings);
        }

        return;

    } else {
        return;
    }

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewTextInput);
}

static void smart_home_open_settings(SmartHomeApp* app) {
    submenu_reset(app->settings_menu);

    submenu_set_header(
        app->settings_menu,
        "Settings");

    submenu_add_item(
        app->settings_menu,
        "Home Assistant URL",
        SETTINGS_HA_URL_INDEX,
        smart_home_settings_callback,
        app);

    submenu_add_item(
        app->settings_menu,
        "Edit HA Token",
        SETTINGS_HA_TOKEN_INDEX,
        smart_home_settings_callback,
        app);

    submenu_add_item(
        app->settings_menu,
        "Import HA Config",
        SETTINGS_HA_IMPORT_INDEX,
        smart_home_settings_callback,
        app);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewSettings);
}

/* ---------- Menu callbacks ---------- */

#define ADD_DEVICE_INDEX 1000





static void smart_home_status_draw_callback(
    Canvas* canvas,
    void* model_ptr) {

    SmartHomeViewModel* model = model_ptr;
    SmartHomeApp* app = model->app;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        64,
        9,
        AlignCenter,
        AlignCenter,
        app->status_title);

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str_aligned(
        canvas,
        64,
        25,
        AlignCenter,
        AlignCenter,
        app->status_line1);

    canvas_draw_str_aligned(
        canvas,
        64,
        38,
        AlignCenter,
        AlignCenter,
        app->status_line2);

    canvas_draw_str_aligned(
        canvas,
        64,
        51,
        AlignCenter,
        AlignCenter,
        app->status_line3);

    canvas_draw_str_aligned(
        canvas,
        64,
        63,
        AlignCenter,
        AlignBottom,
        "Back = device menu");
}

static bool smart_home_status_input_callback(
    InputEvent* event,
    void* context) {

    SmartHomeApp* app = context;

    /*
     * Keep the UI alive while the background request runs, but keep the
     * selected device/result target stable until completion.
     */
    if(app->ha_worker_running) {
        return true;
    }

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort ||
        event->type == InputTypeLong)) {

        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewDevice);

        return true;
    }

    return true;
}

static void smart_home_open_status_view(
    SmartHomeApp* app,
    const char* title,
    const char* line1,
    const char* line2,
    const char* line3) {

    strlcpy(
        app->status_title,
        title ? title : "Status",
        sizeof(app->status_title));

    strlcpy(
        app->status_line1,
        line1 ? line1 : "",
        sizeof(app->status_line1));

    strlcpy(
        app->status_line2,
        line2 ? line2 : "",
        sizeof(app->status_line2));

    strlcpy(
        app->status_line3,
        line3 ? line3 : "",
        sizeof(app->status_line3));

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewStatus);
}

enum {
    SmartHomeCustomEventHaLightDone = 1,
};

static void smart_home_ha_status_rx_line_callback(
    const char* line,
    void* context) {

    SmartHomeApp* app = context;

    if(!app || !line ||
       app->ha_worker_action != DeviceActionLightStatus ||
       app->ha_worker_status_state[0] != '\0') {
        return;
    }

    const char* p = strstr(line, "\"state\"");

    if(!p) {
        return;
    }

    p = strchr(p, ':');

    if(!p) {
        return;
    }

    p++;

    while(*p == ' ' || *p == '\t' ||
          *p == '\r' || *p == '\n') {
        p++;
    }

    if(*p != '"') {
        return;
    }

    p++;

    size_t i = 0;

    while(*p && *p != '"' &&
          i + 1 < sizeof(app->ha_worker_status_state)) {
        app->ha_worker_status_state[i++] = *p++;
    }

    app->ha_worker_status_state[i] = '\0';
}

static int32_t smart_home_ha_light_worker(void* context) {
    SmartHomeApp* app = context;

    if(!app) {
        return -1;
    }

    if(app->ha_worker_action == DeviceActionThermostatTempUp ||
       app->ha_worker_action == DeviceActionThermostatTempDown) {

        app->ha_worker_success =
            smart_home_send_ha(
                app,
                app->ha_worker_url);

        if(app->ha_worker_success &&
           app->fhttp->last_response) {

            char target[24];

            if(smart_home_extract_json_scalar(
                   app->fhttp->last_response,
                   "temperature",
                   target,
                   sizeof(target))) {

                char* end = NULL;
                float current = strtof(target, &end);

                if(end != target && *end == '\0') {
                    float delta =
                        app->ha_worker_action ==
                                DeviceActionThermostatTempUp ?
                            1.0f :
                            -1.0f;

                    char url[URL_SIZE];
                    char payload[160];

                    snprintf(
                        url,
                        sizeof(url),
                        "%s/api/services/climate/set_temperature",
                        app->ha_url);

                    snprintf(
                        payload,
                        sizeof(payload),
                        "{\"entity_id\":\"%.63s\",\"temperature\":%.1f}",
                        app->ha_worker_payload,
                        (double)(current + delta));

                    app->ha_worker_success =
                        smart_home_send_ha_post(
                            app,
                            url,
                            payload);
                } else {
                    app->ha_worker_success = false;
                }
            } else {
                app->ha_worker_success = false;
            }
        }
    } else if(app->ha_worker_is_get) {
        app->ha_worker_status_state[0] = '\0';

        if(app->ha_worker_action == DeviceActionLightStatus) {
            app->fhttp->user_rx_line_cb =
                smart_home_ha_status_rx_line_callback;
            app->fhttp->user_callback_context = app;
        }

        app->ha_worker_success =
            smart_home_send_ha(
                app,
                app->ha_worker_url);

        if(app->ha_worker_action == DeviceActionLightStatus) {
            app->fhttp->user_rx_line_cb = NULL;
            app->fhttp->user_callback_context = NULL;
        }
    } else {
        app->ha_worker_success =
            smart_home_send_ha_post(
                app,
                app->ha_worker_url,
                app->ha_worker_payload);
    }

    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        SmartHomeCustomEventHaLightDone);

    return 0;
}

static bool smart_home_start_ha_light_worker(
    SmartHomeApp* app,
    uint32_t action,
    const char* url,
    const char* payload,
    const char* title,
    bool is_get) {

    if(!app || !url || !title || app->ha_worker_running) {
        return false;
    }

    strlcpy(
        app->ha_worker_url,
        url,
        sizeof(app->ha_worker_url));

    strlcpy(
        app->ha_worker_payload,
        payload ? payload : "{}",
        sizeof(app->ha_worker_payload));

    strlcpy(
        app->ha_worker_title,
        title,
        sizeof(app->ha_worker_title));

    app->ha_worker_action = action;
    app->ha_worker_is_get = is_get;
    app->ha_worker_success = false;
    app->ha_worker_running = true;

    app->ha_worker = furi_thread_alloc();

    if(!app->ha_worker) {
        app->ha_worker_running = false;
        return false;
    }

    furi_thread_set_name(
        app->ha_worker,
        "SmartHomeHA");

    furi_thread_set_stack_size(
        app->ha_worker,
        4096);

    furi_thread_set_context(
        app->ha_worker,
        app);

    furi_thread_set_callback(
        app->ha_worker,
        smart_home_ha_light_worker);

    smart_home_open_status_view(
        app,
        title,
        "Sending...",
        "",
        "");

    furi_thread_start(app->ha_worker);

    return true;
}

static bool smart_home_extract_ha_root_state(
    const char* json,
    char* out,
    size_t out_size) {

    if(!json || !out || out_size < 2) {
        return false;
    }

    /*
     * Home Assistant entity-state JSON normally contains:
     *   "state":"on"
     * Some FlipperHTTP paths may preserve escaping:
     *   \"state\":\"on\"
     */
    const char* p = strstr(json, "\"state\"");
    bool escaped = false;

    if(!p) {
        p = strstr(json, "\\\"state\\\"");
        escaped = (p != NULL);
    }

    if(!p) {
        return false;
    }

    p = strchr(p, ':');
    if(!p) {
        return false;
    }

    p++;

    while(*p == ' ' || *p == '\t' ||
          *p == '\r' || *p == '\n') {
        p++;
    }

    if(escaped) {
        if(p[0] != '\\' || p[1] != '"') {
            return false;
        }
        p += 2;
    } else {
        if(*p != '"') {
            return false;
        }
        p++;
    }

    size_t i = 0;

    while(*p && i + 1 < out_size) {
        if(escaped) {
            if(p[0] == '\\' && p[1] == '"') {
                break;
            }
        } else if(*p == '"') {
            break;
        }

        out[i++] = *p++;
    }

    out[i] = '\0';
    return i > 0;
}

static bool smart_home_custom_event_callback(
    void* context,
    uint32_t event) {

    SmartHomeApp* app = context;

    if(!app ||
       event != SmartHomeCustomEventHaLightDone) {
        return false;
    }

    if(app->ha_worker) {
        furi_thread_join(app->ha_worker);
        furi_thread_free(app->ha_worker);
        app->ha_worker = NULL;
    }

    app->ha_worker_running = false;

    if(!app->ha_worker_success) {
        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Light request failed",
            "",
            "");
        return true;
    }

    if(app->ha_worker_action == DeviceActionThermostatStatus) {
        char current[16] = "?";
        char target[16] = "?";
        char state[20] = "?";
        char fan[20] = "?";
        char humidity[16] = "?";

        const char* response =
            app->fhttp->last_response;

        if(response) {
            smart_home_extract_json_scalar(
                response,
                "current_temperature",
                current,
                sizeof(current));

            smart_home_extract_json_scalar(
                response,
                "temperature",
                target,
                sizeof(target));

            if(!smart_home_extract_ha_root_state(
                   response,
                   state,
                   sizeof(state))) {
                smart_home_extract_json_scalar(
                    response,
                    "state",
                    state,
                    sizeof(state));
            }

            smart_home_extract_json_scalar(
                response,
                "fan_mode",
                fan,
                sizeof(fan));

            smart_home_extract_json_scalar(
                response,
                "current_humidity",
                humidity,
                sizeof(humidity));
        }

        char line1[48];
        char line2[48];
        char line3[48];

        snprintf(
            line1,
            sizeof(line1),
            "Now %sF  Set %sF",
            current,
            target);

        snprintf(
            line2,
            sizeof(line2),
            "%s / Fan %s",
            state,
            fan);

        snprintf(
            line3,
            sizeof(line3),
            "Humidity %s%%",
            humidity);

        smart_home_open_status_view(
            app,
            app->ha_worker_title,
            line1,
            line2,
            line3);

        return true;
    }

    if(app->ha_worker_action == DeviceActionLightStatus) {
        char state[16] = "?";
        char brightness[16] = "?";
        char effect[32] = "?";

        const char* response =
            app->fhttp->last_response;

        if(app->ha_worker_status_state[0] != '\0') {
            strlcpy(
                state,
                app->ha_worker_status_state,
                sizeof(state));
        } else if(response) {
            if(!smart_home_extract_ha_root_state(
                   response,
                   state,
                   sizeof(state))) {

                strlcpy(
                    state,
                    "?",
                    sizeof(state));
            }
        }

        if(response) {
            smart_home_extract_json_scalar(
                response,
                "brightness",
                brightness,
                sizeof(brightness));

            smart_home_extract_json_scalar(
                response,
                "effect",
                effect,
                sizeof(effect));
        }

        char brightness_line[48];

        if(strcmp(brightness, "?") != 0 &&
           strcmp(brightness, "null") != 0) {

            int raw = atoi(brightness);
            int percent =
                (raw * 100 + 127) / 255;

            snprintf(
                brightness_line,
                sizeof(brightness_line),
                "Brightness %d%%",
                percent);
        } else {
            strlcpy(
                brightness_line,
                "Brightness ?",
                sizeof(brightness_line));
        }

        char effect_line[48];

        snprintf(
            effect_line,
            sizeof(effect_line),
            "Effect %.38s",
            effect);

        smart_home_open_status_view(
            app,
            app->ha_worker_title,
            state,
            brightness_line,
            effect_line);

        return true;
    }

    if(app->ha_worker_action >= LIGHT_COLOR_ACTION_BASE &&
       app->ha_worker_action <
           LIGHT_COLOR_ACTION_BASE + smart_home_light_color_count) {

        size_t color_index =
            app->ha_worker_action - LIGHT_COLOR_ACTION_BASE;

        char color_message[48];
        snprintf(
            color_message,
            sizeof(color_message),
            "Color: %s",
            smart_home_light_colors[color_index].name);

        smart_home_open_status_view(
            app,
            app->ha_worker_title,
            color_message,
            "",
            "");
        return true;
    }

    if(app->ha_worker_action >= LIGHT_EFFECT_ACTION_BASE &&
       app->ha_worker_action <
           LIGHT_EFFECT_ACTION_BASE + smart_home_light_effect_count) {

        size_t effect_index =
            app->ha_worker_action - LIGHT_EFFECT_ACTION_BASE;

        char effect_message[48];
        snprintf(
            effect_message,
            sizeof(effect_message),
            "Effect: %.38s",
            smart_home_light_effects[effect_index]);

        smart_home_open_status_view(
            app,
            app->ha_worker_title,
            effect_message,
            "",
            "");
        return true;
    }

    const char* message = "Command sent";

    switch(app->ha_worker_action) {
    case DeviceActionThermostatTempUp:
        message = "Temperature +1";
        break;
    case DeviceActionThermostatTempDown:
        message = "Temperature -1";
        break;
    case DeviceActionThermostatSetTemperature:
        message = "Temperature set";
        break;
    case DeviceActionThermostatCool:
        message = "Mode: Cool";
        break;
    case DeviceActionThermostatHeat:
        message = "Mode: Heat";
        break;
    case DeviceActionThermostatAuto:
        message = "Mode: Auto";
        break;
    case DeviceActionThermostatOff:
        message = "HVAC Off";
        break;
    case DeviceActionThermostatFanAuto:
        message = "Fan: Auto";
        break;
    case DeviceActionThermostatFanOn:
        message = "Fan: On";
        break;
    case DeviceActionOn:
        message = "Turned ON";
        break;
    case DeviceActionOff:
        message = "Turned OFF";
        break;
    case DeviceActionToggle:
        message = "Toggled";
        break;
    case DeviceActionLightBrightness25:
        message = "Brightness 25%";
        break;
    case DeviceActionLightBrightness50:
        message = "Brightness 50%";
        break;
    case DeviceActionLightBrightness75:
        message = "Brightness 75%";
        break;
    case DeviceActionLightBrightness100:
        message = "Brightness 100%";
        break;
    case DeviceActionLightRed:
        message = "Color: Red";
        break;
    case DeviceActionLightGreen:
        message = "Color: Green";
        break;
    case DeviceActionLightBlue:
        message = "Color: Blue";
        break;
    case DeviceActionLightWhite:
        message = "Color: White";
        break;
    case DeviceActionLightSolid:
        message = "Effect: Solid";
        break;
    case DeviceActionLightRainbow:
        message = "Effect: Rainbow";
        break;
    default:
        break;
    }

    smart_home_open_status_view(
        app,
        app->ha_worker_title,
        message,
        "",
        "");

    return true;
}

static uint32_t smart_home_light_submenu_back(void* context) {
    UNUSED(context);
    return SmartHomeViewDevice;
}

static void smart_home_light_color_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    if(!app ||
       app->ha_worker_running ||
       app->selected_device >= app->device_count ||
       index < LIGHT_COLOR_ACTION_BASE) {
        return;
    }

    size_t color_index =
        index - LIGHT_COLOR_ACTION_BASE;

    if(color_index >= smart_home_light_color_count) {
        return;
    }

    SmartHomeDevice* device =
        &app->devices[app->selected_device];

    if(device->type != DeviceTypeHomeAssistantLight ||
       !smart_home_ha_ready(app)) {
        return;
    }

    const SmartHomeLightColor* color =
        &smart_home_light_colors[color_index];

    char url[URL_SIZE];
    char payload[160];

    snprintf(
        url,
        sizeof(url),
        "%s/api/services/light/turn_on",
        app->ha_url);

    snprintf(
        payload,
        sizeof(payload),
        "{\"entity_id\":\"%s\","
        "\"rgb_color\":[%u,%u,%u],"
        "\"effect\":\"Solid\"}",
        device->remote_id,
        color->r,
        color->g,
        color->b);

    if(!smart_home_start_ha_light_worker(
           app,
           index,
           url,
           payload,
           device->name,
           false)) {

        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Could not start request",
            "",
            "");
    }
}

static void smart_home_light_effect_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    if(!app ||
       app->ha_worker_running ||
       app->selected_device >= app->device_count ||
       index < LIGHT_EFFECT_ACTION_BASE) {
        return;
    }

    size_t effect_index =
        index - LIGHT_EFFECT_ACTION_BASE;

    if(effect_index >= smart_home_light_effect_count) {
        return;
    }

    SmartHomeDevice* device =
        &app->devices[app->selected_device];

    if(device->type != DeviceTypeHomeAssistantLight ||
       !smart_home_ha_ready(app)) {
        return;
    }

    char url[URL_SIZE];
    char payload[160];

    snprintf(
        url,
        sizeof(url),
        "%s/api/services/light/turn_on",
        app->ha_url);

    snprintf(
        payload,
        sizeof(payload),
        "{\"entity_id\":\"%s\","
        "\"effect\":\"%s\"}",
        device->remote_id,
        smart_home_light_effects[effect_index]);

    if(!smart_home_start_ha_light_worker(
           app,
           index,
           url,
           payload,
           device->name,
           false)) {

        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Could not start request",
            "",
            "");
    }
}

static void smart_home_open_light_solid_menu(
    SmartHomeApp* app) {

    submenu_reset(app->light_solid_menu);
    submenu_set_header(
        app->light_solid_menu,
        "Solid Colors");

    for(size_t i = 0;
        i < smart_home_light_color_count;
        i++) {

        submenu_add_item(
            app->light_solid_menu,
            smart_home_light_colors[i].name,
            LIGHT_COLOR_ACTION_BASE + i,
            smart_home_light_color_callback,
            app);
    }

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewLightSolid);
}

static void smart_home_open_light_effect_menu(
    SmartHomeApp* app) {

    submenu_reset(app->light_effect_menu);
    submenu_set_header(
        app->light_effect_menu,
        "Effects");

    for(size_t i = 0;
        i < smart_home_light_effect_count;
        i++) {

        submenu_add_item(
            app->light_effect_menu,
            smart_home_light_effects[i],
            LIGHT_EFFECT_ACTION_BASE + i,
            smart_home_light_effect_callback,
            app);
    }

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewLightEffects);
}

static void smart_home_device_action_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    if(app->selected_device >= app->device_count) {
        return;
    }

    SmartHomeDevice* device =
        &app->devices[app->selected_device];

    if(index == DeviceActionRename) {
        smart_home_begin_rename_device(app);
        return;
    }

    if(index == DeviceActionThermostatSetTemperature) {
        smart_home_begin_set_temperature(app);
        return;
    }

    if(index == DeviceActionDelete) {
        for(size_t i = app->selected_device;
            i + 1 < app->device_count;
            i++) {

            app->devices[i] = app->devices[i + 1];
        }

        if(app->device_count > 0) {
            app->device_count--;
        }

        smart_home_save_devices(app);
        smart_home_refresh_main_menu(app);

        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewMain);

        return;
    }

    if(index == DeviceActionLightSolidMenu) {
        smart_home_open_light_solid_menu(app);
        return;
    }

    if(index == DeviceActionLightEffectsMenu) {
        smart_home_open_light_effect_menu(app);
        return;
    }

    if(!smart_home_ha_ready(app)) {
        smart_home_show_result(
            app,
            "Home Assistant",
            "Configure HA Settings");
        return;
    }

    char url[URL_SIZE];
    char payload[160];
    if(device->type == DeviceTypeHomeAssistantSwitch) {
        if(app->ha_worker_running) {
            return;
        }

        const char* service = NULL;

        switch(index) {
        case DeviceActionOn:
            service = "turn_on";
            break;
        case DeviceActionOff:
            service = "turn_off";
            break;
        case DeviceActionToggle:
            service = "toggle";
            break;
        default:
            return;
        }

        snprintf(
            url,
            sizeof(url),
            "%s/api/services/switch/%s",
            app->ha_url,
            service);

        snprintf(
            payload,
            sizeof(payload),
            "{\"entity_id\":\"%s\"}",
            device->remote_id);

        if(!smart_home_start_ha_light_worker(
               app,
               index,
               url,
               payload,
               device->name,
               false)) {

            smart_home_open_status_view(
                app,
                "Home Assistant",
                "Could not start request",
                "",
                "");
        }

        return;
    }

    if(device->type == DeviceTypeHomeAssistantLight) {
        if(app->ha_worker_running) {
            return;
        }

        if(index == DeviceActionLightStatus) {
            snprintf(
                url,
                sizeof(url),
                "%s/api/states/%s",
                app->ha_url,
                device->remote_id);

            if(!smart_home_start_ha_light_worker(
                   app,
                   index,
                   url,
                   NULL,
                   device->name,
                   true)) {

                smart_home_open_status_view(
                    app,
                    "Home Assistant",
                    "Could not start request",
                    "",
                    "");
            }

            return;
        }

        const char* service = NULL;
        const char* extra = NULL;

        switch(index) {
        case DeviceActionOn:
            service = "turn_on";
            break;
        case DeviceActionOff:
            service = "turn_off";
            break;
        case DeviceActionToggle:
            service = "toggle";
            break;
        case DeviceActionLightBrightness25:
            service = "turn_on";
            extra = "\"brightness_pct\":25";
            break;
        case DeviceActionLightBrightness50:
            service = "turn_on";
            extra = "\"brightness_pct\":50";
            break;
        case DeviceActionLightBrightness75:
            service = "turn_on";
            extra = "\"brightness_pct\":75";
            break;
        case DeviceActionLightBrightness100:
            service = "turn_on";
            extra = "\"brightness_pct\":100";
            break;
        case DeviceActionLightRed:
            service = "turn_on";
            extra = "\"rgb_color\":[255,0,0],\"effect\":\"Solid\"";
            break;
        case DeviceActionLightGreen:
            service = "turn_on";
            extra = "\"rgb_color\":[0,255,0],\"effect\":\"Solid\"";
            break;
        case DeviceActionLightBlue:
            service = "turn_on";
            extra = "\"rgb_color\":[0,0,255],\"effect\":\"Solid\"";
            break;
        case DeviceActionLightWhite:
            service = "turn_on";
            extra = "\"rgb_color\":[255,255,255],\"effect\":\"Solid\"";
            break;
        case DeviceActionLightSolid:
            service = "turn_on";
            extra = "\"effect\":\"Solid\"";
            break;
        case DeviceActionLightRainbow:
            service = "turn_on";
            extra = "\"effect\":\"Rainbow\"";
            break;
        default:
            return;
        }

        snprintf(
            url,
            sizeof(url),
            "%s/api/services/light/%s",
            app->ha_url,
            service);

        if(extra) {
            snprintf(
                payload,
                sizeof(payload),
                "{\"entity_id\":\"%s\",%s}",
                device->remote_id,
                extra);
        } else {
            snprintf(
                payload,
                sizeof(payload),
                "{\"entity_id\":\"%s\"}",
                device->remote_id);
        }

        if(!smart_home_start_ha_light_worker(
               app,
               index,
               url,
               payload,
               device->name,
               false)) {

            smart_home_open_status_view(
                app,
                "Home Assistant",
                "Could not start request",
                "",
                "");
        }

        return;
    }


    if(device->type !=
       DeviceTypeHomeAssistantThermostat) {
        return;
    }

    if(app->ha_worker_running) {
        return;
    }

    if(index == DeviceActionThermostatStatus) {
        snprintf(
            url,
            sizeof(url),
            "%s/api/states/%s",
            app->ha_url,
            device->remote_id);

        if(!smart_home_start_ha_light_worker(
               app,
               index,
               url,
               NULL,
               device->name,
               true)) {

            smart_home_open_status_view(
                app,
                "Home Assistant",
                "Could not start request",
                "",
                "");
        }

        return;
    }

    if(index == DeviceActionThermostatTempUp ||
       index == DeviceActionThermostatTempDown) {

        snprintf(
            url,
            sizeof(url),
            "%s/api/states/%s",
            app->ha_url,
            device->remote_id);

        if(!smart_home_start_ha_light_worker(
               app,
               index,
               url,
               device->remote_id,
               device->name,
               true)) {

            smart_home_open_status_view(
                app,
                "Home Assistant",
                "Could not start request",
                "",
                "");
        }

        return;
    }

    const char* service = NULL;
    const char* field = NULL;
    const char* value = NULL;

    switch(index) {
    case DeviceActionThermostatCool:
        service = "set_hvac_mode";
        field = "hvac_mode";
        value = "cool";
        break;

    case DeviceActionThermostatHeat:
        service = "set_hvac_mode";
        field = "hvac_mode";
        value = "heat";
        break;

    case DeviceActionThermostatAuto:
        service = "set_hvac_mode";
        field = "hvac_mode";
        value = "heat_cool";
        break;

    case DeviceActionThermostatOff:
        service = "set_hvac_mode";
        field = "hvac_mode";
        value = "off";
        break;

    case DeviceActionThermostatFanAuto:
        service = "set_fan_mode";
        field = "fan_mode";
        value = "auto";
        break;

    case DeviceActionThermostatFanOn:
        service = "set_fan_mode";
        field = "fan_mode";
        value = "on";
        break;

    default:
        return;
    }

    snprintf(
        url,
        sizeof(url),
        "%s/api/services/climate/%s",
        app->ha_url,
        service);

    snprintf(
        payload,
        sizeof(payload),
        "{\"entity_id\":\"%s\",\"%s\":\"%s\"}",
        device->remote_id,
        field,
        value);

    if(!smart_home_start_ha_light_worker(
           app,
           index,
           url,
           payload,
           device->name,
           false)) {

        smart_home_open_status_view(
            app,
            "Home Assistant",
            "Could not start request",
            "",
            "");
    }

    return;
}


static void smart_home_main_callback(
    void* context,
    uint32_t index) {

    SmartHomeApp* app = context;

    if(index == ADD_DEVICE_INDEX) {
        smart_home_begin_add_device(app);
        return;
    }

    if(index == SETTINGS_INDEX) {
        smart_home_open_settings(app);
        return;
    }

    if(index < app->device_count) {
        smart_home_open_device(app, index);

        submenu_reset(app->device_menu);
        submenu_set_header(
            app->device_menu,
            app->devices[index].name);

        SmartHomeDevice* device =
            &app->devices[index];

        if(device->type == DeviceTypeHomeAssistantSwitch) {

            submenu_add_item(
                app->device_menu,
                "Turn ON",
                DeviceActionOn,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Turn OFF",
                DeviceActionOff,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Toggle",
                DeviceActionToggle,
                smart_home_device_action_callback,
                app);

        } else if(device->type == DeviceTypeHomeAssistantLight) {

            submenu_add_item(app->device_menu, "Status", DeviceActionLightStatus, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Turn ON", DeviceActionOn, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Turn OFF", DeviceActionOff, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Toggle", DeviceActionToggle, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Brightness 25%", DeviceActionLightBrightness25, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Brightness 50%", DeviceActionLightBrightness50, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Brightness 75%", DeviceActionLightBrightness75, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Brightness 100%", DeviceActionLightBrightness100, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Solid", DeviceActionLightSolidMenu, smart_home_device_action_callback, app);
            submenu_add_item(app->device_menu, "Effects", DeviceActionLightEffectsMenu, smart_home_device_action_callback, app);

        } else if(
            device->type ==
            DeviceTypeHomeAssistantThermostat) {

            submenu_add_item(
                app->device_menu,
                "Status",
                DeviceActionThermostatStatus,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Temp +1",
                DeviceActionThermostatTempUp,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Temp -1",
                DeviceActionThermostatTempDown,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Set Temperature",
                DeviceActionThermostatSetTemperature,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Cool",
                DeviceActionThermostatCool,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Heat",
                DeviceActionThermostatHeat,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Auto",
                DeviceActionThermostatAuto,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Off",
                DeviceActionThermostatOff,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Fan Auto",
                DeviceActionThermostatFanAuto,
                smart_home_device_action_callback,
                app);

            submenu_add_item(
                app->device_menu,
                "Fan On",
                DeviceActionThermostatFanOn,
                smart_home_device_action_callback,
                app);
        }

        submenu_add_item(
            app->device_menu,
            "Rename Device",
            DeviceActionRename,
            smart_home_device_action_callback,
            app);

        submenu_add_item(
            app->device_menu,
            "Delete Device",
            DeviceActionDelete,
            smart_home_device_action_callback,
            app);

        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            SmartHomeViewDevice);
    }
}

static void smart_home_refresh_main_menu(SmartHomeApp* app) {
    submenu_reset(app->main_menu);
    submenu_set_header(app->main_menu, "Smart Home");

    for(size_t i = 0; i < app->device_count; i++) {
        submenu_add_item(
            app->main_menu,
            app->devices[i].name,
            (uint32_t)i,
            smart_home_main_callback,
            app);
    }

    submenu_add_item(
        app->main_menu,
        "+ Add Device",
        ADD_DEVICE_INDEX,
        smart_home_main_callback,
        app);

    submenu_add_item(
        app->main_menu,
        "Settings",
        SETTINGS_INDEX,
        smart_home_main_callback,
        app);
}

static void smart_home_dialog_callback(
    DialogExResult result,
    void* context) {

    SmartHomeApp* app = context;

    if(result == DialogExResultCenter) {
        uint32_t target = app->result_return_view;

        if(target > SmartHomeViewSettings) {
            target = SmartHomeViewMain;
        }

        view_dispatcher_switch_to_view(
            app->view_dispatcher,
            target);
    }
}

/* ---------- Application ---------- */

int32_t smart_home_app(void* p) {
    UNUSED(p);

    SmartHomeApp* app = malloc(sizeof(SmartHomeApp));

    if(!app) {
        return -1;
    }

    memset(app, 0, sizeof(SmartHomeApp));

    app->fhttp = flipper_http_alloc();

    if(!app->fhttp) {
        free(app);
        return -1;
    }

    if(!smart_home_wait_for_board(app->fhttp)) {
        flipper_http_free(app->fhttp);
        free(app);
        return -1;
    }

    furi_delay_ms(500);

    if(!flipper_http_send_command(
           app->fhttp,
           HTTP_CMD_WIFI_CONNECT)) {

        flipper_http_free(app->fhttp);
        free(app);
        return -1;
    }

    furi_delay_ms(2000);

    smart_home_load_devices(app);
    smart_home_load_settings(app);

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->device_menu = submenu_alloc();
    app->light_solid_menu = submenu_alloc();
    app->light_effect_menu = submenu_alloc();
    app->device_type_menu = submenu_alloc();
    app->settings_menu = submenu_alloc();
    app->dialog = dialog_ex_alloc();
    app->text_input = text_input_alloc();

    app->ha_url_keyboard_view = view_alloc();
    app->status_view = view_alloc();

    view_allocate_model(
        app->ha_url_keyboard_view,
        ViewModelTypeLockFree,
        sizeof(SmartHomeViewModel));

    SmartHomeViewModel* ha_url_model =
        view_get_model(app->ha_url_keyboard_view);

    ha_url_model->app = app;

    view_set_context(app->ha_url_keyboard_view, app);
    view_set_draw_callback(
        app->ha_url_keyboard_view,
        smart_home_ha_url_keyboard_draw_callback);
    view_set_input_callback(
        app->ha_url_keyboard_view,
        smart_home_ha_url_keyboard_input_callback);

    view_allocate_model(
        app->status_view,
        ViewModelTypeLockFree,
        sizeof(SmartHomeViewModel));

    SmartHomeViewModel* status_model =
        view_get_model(app->status_view);

    status_model->app = app;

    view_set_context(app->status_view, app);
    view_set_draw_callback(
        app->status_view,
        smart_home_status_draw_callback);
    view_set_input_callback(
        app->status_view,
        smart_home_status_input_callback);

    view_dispatcher_set_event_callback_context(
        app->view_dispatcher,
        app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher,
        smart_home_custom_event_callback);

    view_dispatcher_attach_to_gui(
        app->view_dispatcher,
        app->gui,
        ViewDispatcherTypeFullscreen);

    smart_home_refresh_main_menu(app);

    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(
        app->dialog,
        smart_home_dialog_callback);

    /*
     * DialogEx's result callback context and the underlying View's
     * navigation/previous-callback context are separate.
     *
     * smart_home_result_back() dereferences its context to read
     * result_return_view, so the DialogEx View must explicitly own
     * the SmartHomeApp context as well. Without this, physical Back
     * can call smart_home_result_back(NULL) and reboot the Flipper.
     */
    view_set_context(
        dialog_ex_get_view(app->dialog),
        app);

    view_set_previous_callback(
        submenu_get_view(app->main_menu),
        smart_home_exit_callback);

    view_set_previous_callback(
        submenu_get_view(app->device_menu),
        smart_home_device_back);

    view_set_previous_callback(
        submenu_get_view(app->light_solid_menu),
        smart_home_light_submenu_back);

    view_set_previous_callback(
        submenu_get_view(app->light_effect_menu),
        smart_home_light_submenu_back);

    view_set_previous_callback(
        submenu_get_view(app->device_type_menu),
        smart_home_input_back);

    view_set_previous_callback(
        submenu_get_view(app->settings_menu),
        smart_home_settings_back);

    view_set_previous_callback(
        dialog_ex_get_view(app->dialog),
        smart_home_result_back);

    view_set_previous_callback(
        text_input_get_view(app->text_input),
        smart_home_input_back);

    view_set_previous_callback(
        app->ha_url_keyboard_view,
        smart_home_settings_back);

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewMain,
        submenu_get_view(app->main_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewDevice,
        submenu_get_view(app->device_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewLightSolid,
        submenu_get_view(app->light_solid_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewLightEffects,
        submenu_get_view(app->light_effect_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewDeviceType,
        submenu_get_view(app->device_type_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewSettings,
        submenu_get_view(app->settings_menu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewResult,
        dialog_ex_get_view(app->dialog));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewTextInput,
        text_input_get_view(app->text_input));

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewHaUrlKeyboard,
        app->ha_url_keyboard_view);

    view_dispatcher_add_view(
        app->view_dispatcher,
        SmartHomeViewStatus,
        app->status_view);

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        SmartHomeViewMain);

    view_dispatcher_run(app->view_dispatcher);

    if(app->ha_worker) {
        furi_thread_join(app->ha_worker);
        furi_thread_free(app->ha_worker);
        app->ha_worker = NULL;
        app->ha_worker_running = false;
    }

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewMain);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewDevice);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewLightSolid);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewLightEffects);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewResult);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewTextInput);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewHaUrlKeyboard);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewStatus);

    view_free(app->status_view);
    view_free(app->ha_url_keyboard_view);
    text_input_free(app->text_input);
    dialog_ex_free(app->dialog);
    submenu_free(app->light_solid_menu);
    submenu_free(app->light_effect_menu);
    submenu_free(app->device_menu);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewDeviceType);

    view_dispatcher_remove_view(
        app->view_dispatcher,
        SmartHomeViewSettings);

    submenu_free(app->device_type_menu);
    submenu_free(app->settings_menu);
    submenu_free(app->main_menu);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    flipper_http_free(app->fhttp);
    free(app);

    return 0;
}
