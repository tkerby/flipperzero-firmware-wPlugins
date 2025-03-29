#include <input/input.h>
#include <gui/gui.h>
#include <furi.h>
#include <string.h>
#include <time.h>

#define TAG              "Roman Numeral Decoder"
#define DEBOUNCE_TIME_MS 300
#define MAX_ROMAN_LENGTH 15

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    char pressedKey[32];
    char displayString[64];
    int current_char_index;
    uint32_t last_input_time;
} inputDemo;

const char* roman_chars[] = {"M", "D", "C", "L", "X", "V", "I"};
const int roman_chars_count = sizeof(roman_chars) / sizeof(roman_chars[0]);

void draw_callback(Canvas* canvas, void* context) {
    inputDemo* app = (inputDemo*)context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    const char* app_name = "Roman Decoder";
    canvas_draw_str_aligned(canvas, 60, 10, AlignCenter, AlignCenter, app_name);

    canvas_set_bitmap_mode(canvas, true);

    canvas_draw_str(canvas, 23, 35, "M");
    canvas_draw_str(canvas, 38, 35, "D");
    canvas_draw_str(canvas, 49, 35, "C");
    canvas_draw_str(canvas, 61, 35, "L");
    canvas_draw_str(canvas, 71, 35, "X");
    canvas_draw_str(canvas, 83, 35, "V");
    canvas_draw_str(canvas, 96, 35, "I");

    int underscore_positions[] = {23, 38, 49, 61, 71, 83, 96};
    int underscore_x = underscore_positions[app->current_char_index];
    canvas_draw_str(canvas, underscore_x, 35 + 4, "_");

    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, app->pressedKey);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, app->displayString);
}

void input_callback(InputEvent* event, void* context) {
    inputDemo* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

int roman_to_integer(const char* s) {
    if(strlen(s) > MAX_ROMAN_LENGTH) {
        return -1;
    }

    int total = 0;
    int prev_value = 0;

    while(*s) {
        int current_value;
        switch(*s) {
        case 'I':
            current_value = 1;
            break;
        case 'V':
            current_value = 5;
            break;
        case 'X':
            current_value = 10;
            break;
        case 'L':
            current_value = 50;
            break;
        case 'C':
            current_value = 100;
            break;
        case 'D':
            current_value = 500;
            break;
        case 'M':
            current_value = 1000;
            break;
        default:
            return -1;
        }

        if(current_value > prev_value) {
            total += current_value - 2 * prev_value;
        } else {
            total += current_value;
        }
        prev_value = current_value;
        s++;
    }

    return total;
}

void safe_strcat(char* dest, const char* src, size_t dest_size) {
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    if(dest_len + src_len >= dest_size) {
        src_len = dest_size - dest_len - 1;
    }

    strncpy(dest + dest_len, src, src_len);
    dest[dest_len + src_len] = '\0';
}

void handle_ok_key(inputDemo* app) {
    if(strlen(app->pressedKey) < MAX_ROMAN_LENGTH) {
        safe_strcat(
            app->pressedKey, roman_chars[app->current_char_index], sizeof(app->pressedKey));
        int decimal = roman_to_integer(app->pressedKey);
        if(decimal == -1) {
            snprintf(app->displayString, sizeof(app->displayString), "Invalid input!");
        } else {
            snprintf(app->displayString, sizeof(app->displayString), "Decimal: %d", decimal);
        }
    } else {
        snprintf(app->displayString, sizeof(app->displayString), "Max length reached!");
    }
}

int32_t roman_decoder_main(void*) {
    inputDemo app;

    app.current_char_index = 0;
    app.pressedKey[0] = '\0';
    app.displayString[0] = '\0';
    app.last_input_time = 0;

    app.view_port = view_port_alloc();
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);

    app.gui = furi_record_open("gui");
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    InputEvent input;
    FURI_LOG_I(TAG, "Start the main loop.");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);

        uint32_t current_time = furi_get_tick();

        switch(input.key) {
        case InputKeyRight:
            if(current_time - app.last_input_time > DEBOUNCE_TIME_MS) {
                app.current_char_index = (app.current_char_index + 1) % roman_chars_count;
                app.last_input_time = current_time;
            }
            break;
        case InputKeyLeft:
            if(current_time - app.last_input_time > DEBOUNCE_TIME_MS) {
                app.current_char_index =
                    (app.current_char_index - 1 + roman_chars_count) % roman_chars_count;
                app.last_input_time = current_time;
            }
            break;
        case InputKeyOk:
            if(current_time - app.last_input_time > DEBOUNCE_TIME_MS) {
                handle_ok_key(&app);
                app.last_input_time = current_time;
            }
            break;
        case InputKeyBack:
            if(input.type == InputTypeLong) {
                break;
            } else {
                if(current_time - app.last_input_time > DEBOUNCE_TIME_MS) {
                    size_t len = strlen(app.pressedKey);
                    if(len > 0) {
                        app.pressedKey[len - 1] = '\0';
                        int decimal = roman_to_integer(app.pressedKey);
                        snprintf(
                            app.displayString, sizeof(app.displayString), "Decimal: %d", decimal);
                    }
                    app.last_input_time = current_time;
                }
            }
            break;

        default:
            break;
        }

        view_port_update(app.view_port);

        if(input.type == InputTypeLong && input.key == InputKeyBack) {
            break;
        }
    }

    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close("gui");
    view_port_free(app.view_port);

    return 0;
}
