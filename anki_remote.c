// Standard Flipper Includes
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <gui/icon_i.h>

// Bluetooth and HID Profile Includes
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

// Image assets
#include "anki_remote_icons.h"

// Defines
#define TAG "AnkiRemoteApp"
#define KEYMAP_PATH APP_DATA_PATH("keymaps.dat")
#define NUM_FLIPPER_BUTTONS 6
#define MENU_OPTIONS_COUNT 2
// Bug fix (this is so it can use the same bluetooth keys as other HID apps)
#define HID_BT_KEYS_STORAGE_PATH EXT_PATH("apps_data/hid_ble/.bt_hid.keys")


// - - - Section 1: Enums and Structs - - -


// Bitmask for modifier keys
#define MOD_CTRL_BIT  (1 << 0)
#define MOD_SHIFT_BIT (1 << 1)
#define MOD_ALT_BIT   (1 << 2)
#define MOD_GUI_BIT   (1 << 3)

// Flipper buttons for key mapping
typedef enum {
    FlipperButtonUp = 0,
    FlipperButtonDown,
    FlipperButtonLeft,
    FlipperButtonRight,
    FlipperButtonOk,
    FlipperButtonBack,
} FlipperButton;

// Main scenes
typedef enum {
    SceneMenu,
    SceneController,
    SceneSettingsMenu,
    SceneSettings,
} AppScene;

// Event types for the message queue
typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

// Event structure
typedef struct {
    EventType type;
    InputEvent input;
} AnkiRemoteEvent;

// Keycodes and modifiers (Ctrl, Alt, etc.)
typedef struct {
    uint8_t keycode;
    uint8_t modifiers;
} KeyMapping;

typedef struct AnkiRemoteApp AnkiRemoteApp;


// - - - Section 2: Keyboard Settings UI Data (from the pre-installed BLE Remote app) - - -


typedef struct {
    uint8_t width;
    char key;
    char shift_key;
    const Icon* icon;
    const Icon* icon_shift;
    const Icon* icon_toggled;
    uint8_t value;
} HidKeyboardKey;

#define KEYBOARD_ROW_COUNT 7
#define KEYBOARD_COLUMN_COUNT 12
const HidKeyboardKey hid_keyboard_keyset[KEYBOARD_ROW_COUNT][KEYBOARD_COLUMN_COUNT] = {
    // F-keys
    {{.width = 1, .icon = &I_ButtonF1_5x8, .value = HID_KEYBOARD_F1}, {.width = 1, .icon = &I_ButtonF2_5x8, .value = HID_KEYBOARD_F2}, {.width = 1, .icon = &I_ButtonF3_5x8, .value = HID_KEYBOARD_F3}, {.width = 1, .icon = &I_ButtonF4_5x8, .value = HID_KEYBOARD_F4}, {.width = 1, .icon = &I_ButtonF5_5x8, .value = HID_KEYBOARD_F5}, {.width = 1, .icon = &I_ButtonF6_5x8, .value = HID_KEYBOARD_F6}, {.width = 1, .icon = &I_ButtonF7_5x8, .value = HID_KEYBOARD_F7}, {.width = 1, .icon = &I_ButtonF8_5x8, .value = HID_KEYBOARD_F8}, {.width = 1, .icon = &I_ButtonF9_5x8, .value = HID_KEYBOARD_F9}, {.width = 1, .icon = &I_ButtonF10_5x8, .value = HID_KEYBOARD_F10}, {.width = 1, .icon = &I_ButtonF11_5x8, .value = HID_KEYBOARD_F11}, {.width = 1, .icon = &I_ButtonF12_5x8, .value = HID_KEYBOARD_F12}},
    // Numbers row
    {{.width = 1, .key = '1', .shift_key = '!', .value = HID_KEYBOARD_1}, {.width = 1, .key = '2', .shift_key = '@', .value = HID_KEYBOARD_2}, {.width = 1, .icon_shift = &I_hash_button_9x11, .key = '3', .shift_key = '#', .value = HID_KEYBOARD_3}, {.width = 1, .key = '4', .shift_key = '$', .value = HID_KEYBOARD_4}, {.width = 1, .icon_shift = &I_percent_button_9x11, .key = '5', .shift_key = '%', .value = HID_KEYBOARD_5}, {.width = 1, .key = '6', .shift_key = '^', .value = HID_KEYBOARD_6}, {.width = 1, .key = '7', .shift_key = '&', .value = HID_KEYBOARD_7}, {.width = 1, .key = '8', .shift_key = '*', .value = HID_KEYBOARD_8}, {.width = 1, .key = '9', .shift_key = '(', .value = HID_KEYBOARD_9}, {.width = 1, .key = '0', .shift_key = ')', .value = HID_KEYBOARD_0}, {.width = 2, .icon = &I_backspace_19x11, .value = HID_KEYBOARD_DELETE}},
    // QWERTY row
    {{.width = 1, .key = 'q', .shift_key = 'Q', .value = HID_KEYBOARD_Q}, {.width = 1, .key = 'w', .shift_key = 'W', .value = HID_KEYBOARD_W}, {.width = 1, .key = 'e', .shift_key = 'E', .value = HID_KEYBOARD_E}, {.width = 1, .key = 'r', .shift_key = 'R', .value = HID_KEYBOARD_R}, {.width = 1, .key = 't', .shift_key = 'T', .value = HID_KEYBOARD_T}, {.width = 1, .key = 'y', .shift_key = 'Y', .value = HID_KEYBOARD_Y}, {.width = 1, .key = 'u', .shift_key = 'U', .value = HID_KEYBOARD_U}, {.width = 1, .key = 'i', .shift_key = 'I', .value = HID_KEYBOARD_I}, {.width = 1, .key = 'o', .shift_key = 'O', .value = HID_KEYBOARD_O}, {.width = 1, .key = 'p', .shift_key = 'P', .value = HID_KEYBOARD_P}, {.width = 1, .icon = &I_sq_bracket_left_button_9x11, .icon_shift = &I_brace_left_button_9x11, .value = HID_KEYBOARD_OPEN_BRACKET}, {.width = 1, .icon = &I_sq_bracket_right_button_9x11, .icon_shift = &I_brace_right_button_9x11, .value = HID_KEYBOARD_CLOSE_BRACKET}},
    // ASDF row
    {{.width = 1, .key = 'a', .shift_key = 'A', .value = HID_KEYBOARD_A}, {.width = 1, .key = 's', .shift_key = 'S', .value = HID_KEYBOARD_S}, {.width = 1, .key = 'd', .shift_key = 'D', .value = HID_KEYBOARD_D}, {.width = 1, .key = 'f', .shift_key = 'F', .value = HID_KEYBOARD_F}, {.width = 1, .key = 'g', .shift_key = 'G', .value = HID_KEYBOARD_G}, {.width = 1, .key = 'h', .shift_key = 'H', .value = HID_KEYBOARD_H}, {.width = 1, .key = 'j', .shift_key = 'J', .value = HID_KEYBOARD_J}, {.width = 1, .key = 'k', .shift_key = 'K', .value = HID_KEYBOARD_K}, {.width = 1, .key = 'l', .shift_key = 'L', .value = HID_KEYBOARD_L}, {.width = 1, .key = ';', .shift_key = ':', .value = HID_KEYBOARD_SEMICOLON}, {.width = 2, .icon = &I_Return_10x7, .value = HID_KEYBOARD_RETURN}},
    // ZXCV row
    {{.width = 1, .key = 'z', .shift_key = 'Z', .value = HID_KEYBOARD_Z}, {.width = 1, .key = 'x', .shift_key = 'X', .value = HID_KEYBOARD_X}, {.width = 1, .key = 'c', .shift_key = 'C', .value = HID_KEYBOARD_C}, {.width = 1, .key = 'v', .shift_key = 'V', .value = HID_KEYBOARD_V}, {.width = 1, .key = 'b', .shift_key = 'B', .value = HID_KEYBOARD_B}, {.width = 1, .key = 'n', .shift_key = 'N', .value = HID_KEYBOARD_N}, {.width = 1, .key = 'm', .shift_key = 'M', .value = HID_KEYBOARD_M}, {.width = 1, .icon = &I_slash_button_9x11, .shift_key = '?', .value = HID_KEYBOARD_SLASH}, {.width = 1, .icon = &I_backslash_button_9x11, .shift_key = '|', .value = HID_KEYBOARD_BACKSLASH}, {.width = 1, .icon = &I_backtick_button_9x11, .shift_key = '~', .value = HID_KEYBOARD_GRAVE_ACCENT}, {.width = 1, .icon = &I_ButtonUp_7x4, .value = HID_KEYBOARD_UP_ARROW}, {.width = 1, .icon_shift = &I_underscore_button_9x11, .key = '-', .shift_key = '_', .value = HID_KEYBOARD_MINUS}},
    // Bottom symbol row
    {{.width = 1, .icon = &I_Shift_inactive_7x9, .icon_toggled = &I_Shift_active_7x9, .value = HID_KEYBOARD_L_SHIFT}, {.width = 1, .key = ',', .shift_key = '<', .value = HID_KEYBOARD_COMMA}, {.width = 1, .key = '.', .shift_key = '>', .value = HID_KEYBOARD_DOT}, {.width = 4, .value = HID_KEYBOARD_SPACEBAR}, {.width = 0}, {.width = 0}, {.width = 0},
    {.width = 1, .key = '\'', .shift_key = '"', .icon = &I_apostrophe_button_9x11, .icon_shift = &I_quote_button_9x11, .value = HID_KEYBOARD_APOSTROPHE},
    {.width = 1, .icon = &I_equals_button_9x11, .shift_key = '+', .value = HID_KEYBOARD_EQUAL_SIGN}, {.width = 1, .icon = &I_ButtonLeft_4x7, .value = HID_KEYBOARD_LEFT_ARROW}, {.width = 1, .icon = &I_ButtonDown_7x4, .value = HID_KEYBOARD_DOWN_ARROW}, {.width = 1, .icon = &I_ButtonRight_4x7, .value = HID_KEYBOARD_RIGHT_ARROW}},
    // Modifier keys row
    {{.width = 2, .icon = &I_Ctrl_17x10, .icon_toggled = &I_Ctrl_active_17x9, .value = HID_KEYBOARD_L_CTRL}, {.width = 0}, {.width = 2, .icon = &I_Alt_17x10, .icon_toggled = &I_Alt_active_17x9, .value = HID_KEYBOARD_L_ALT}, {.width = 0}, {.width = 2, .icon = &I_Cmd_17x10, .icon_toggled = &I_Cmd_active_17x9, .value = HID_KEYBOARD_L_GUI}, {.width = 0}, {.width = 2, .icon = &I_Tab_17x10, .value = HID_KEYBOARD_TAB}, {.width = 0}, {.width = 2, .icon = &I_Esc_17x10, .value = HID_KEYBOARD_ESCAPE}, {.width = 0}, {.width = 2, .icon = &I_Del_17x10, .value = HID_KEYBOARD_DELETE_FORWARD}}};


// - - - Section 3: Main Application State Struct - - -


struct AnkiRemoteApp {
    // Flipper os stuff
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    Bt* bt;
    FuriHalBleProfileBase* ble_hid_profile;
    
    // App state
    AppScene current_scene;
    bool running;
    bool connected;
    KeyMapping keymap[NUM_FLIPPER_BUTTONS];
    uint8_t led_brightness; // v1.2 NEW
    struct {
        uint8_t selected_item;
    } menu_state;
    
    // Scene specific states
    struct {
        uint8_t selected_item;
        uint8_t scroll_offset;
    } settings_menu_state;
    struct {
        bool up_pressed;
        bool down_pressed;
        bool left_pressed;
        bool right_pressed;
        bool ok_pressed;
        bool back_pressed;
    } controller_state;
    struct {
        uint8_t x, y; // Cursor position on keyboard
        bool shift_pressed;
        bool ctrl_pressed;
        bool alt_pressed;
        bool gui_pressed;
        FlipperButton configuring_button;
    } settings_state;
    
    // BUG FIX: Flag to prevent the OK button release from being processed by the next screen
    bool ignore_next_ok_release;
};

// Function List
static void anki_remote_draw_callback(Canvas* canvas, void* context);
static void anki_remote_input_callback(InputEvent* input_event, void* context);
static void anki_remote_connection_status_callback(BtStatus status, void* context);
static void anki_remote_set_scene(AnkiRemoteApp* app, AppScene scene);
static void anki_remote_send_key_combo(AnkiRemoteApp* app, KeyMapping mapping, bool is_press);
static const char* get_button_name(FlipperButton button);
static void get_key_combo_name(KeyMapping mapping, char* buffer, size_t buffer_size);


// - - - Section 4: Keymap Saving - - -


// v1.1: No longer a set of default keymappings (unmaps all keys)
static void anki_remote_set_default_keymap(AnkiRemoteApp* app) {
    for(uint8_t i = 0; i < NUM_FLIPPER_BUTTONS; ++i) {
        app->keymap[i] = (KeyMapping){.keycode = 0, .modifiers = 0};
    }
    app->led_brightness = 255; // v1.2: Max brightness is default and will be used if missing in file
}

// Try to load button settings from SD card; if it fails, use defaults
static void anki_remote_load_keymap(AnkiRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, KEYMAP_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, app->keymap, sizeof(app->keymap)) != sizeof(app->keymap)) {
            anki_remote_set_default_keymap(app);
        } else {
            if(storage_file_read(file, &app->led_brightness, sizeof(uint8_t)) != sizeof(uint8_t)) {
                app->led_brightness = 255;
            }
        }
    } else {
        anki_remote_set_default_keymap(app);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Save button settings to the SD card
static void anki_remote_save_keymap(AnkiRemoteApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, KEYMAP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, app->keymap, sizeof(app->keymap));
        storage_file_write(file, &app->led_brightness, sizeof(uint8_t)); // v1.2: Append brightness
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}


// - - - Section 5a: Menu Scene - - -


static void anki_remote_scene_menu_draw_callback(Canvas* canvas, void* context) {
    AnkiRemoteApp* app = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Custom BLE Remote");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < MENU_OPTIONS_COUNT; ++i) {
        if(i == app->menu_state.selected_item) {
            elements_slightly_rounded_box(canvas, 10, 22 + (i * 15), 108, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        if(i == 0) canvas_draw_str(canvas, 15, 33, "Start");
        if(i == 1) canvas_draw_str(canvas, 15, 48, "Settings");
        canvas_set_color(canvas, ColorBlack);
    }
    elements_multiline_text_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, "Created by @blue5gd");
}

static void anki_remote_scene_menu_input_handler(AnkiRemoteApp* app, InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;
    if(event->key == InputKeyUp) {
        app->menu_state.selected_item =
            (app->menu_state.selected_item + MENU_OPTIONS_COUNT - 1) % MENU_OPTIONS_COUNT;
    } else if(event->key == InputKeyDown) {
        app->menu_state.selected_item = (app->menu_state.selected_item + 1) % MENU_OPTIONS_COUNT;
    } else if(event->key == InputKeyOk) {
        if(app->menu_state.selected_item == 0) anki_remote_set_scene(app, SceneController); // Start
        else if(app->menu_state.selected_item == 1) anki_remote_set_scene(app, SceneSettingsMenu); // Settings
    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        app->running = false; // Bug fix (Only exit the app on a short press from the menu)
        // so the whole app doesn't accidently close
    }
}


// - - - Section 5b: Controller Scene - - -


static void hid_keynote_draw_arrow(Canvas* canvas, uint8_t x, uint8_t y, CanvasDirection dir) {
    canvas_draw_triangle(canvas, x, y, 5, 3, dir);
    if(dir == CanvasDirectionBottomToTop) canvas_draw_line(canvas, x, y + 6, x, y - 1);
    else if(dir == CanvasDirectionTopToBottom) canvas_draw_line(canvas, x, y - 6, x, y + 1);
    else if(dir == CanvasDirectionRightToLeft) canvas_draw_line(canvas, x + 6, y, x - 1, y);
    else if(dir == CanvasDirectionLeftToRight) canvas_draw_line(canvas, x - 6, y, x + 1, y);
}

static void anki_remote_scene_controller_draw_callback(Canvas* canvas, void* context) {
    AnkiRemoteApp* app = context;
    canvas_clear(canvas);
    if(!app->connected) {
        // "Awaiting Bluetooth" Screen
        // CREDIT: https://lab.flipper.net/apps/bt_trigger
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "BLE Remote");
        canvas_draw_icon(canvas, 111, 2, &I_Ble_disconnected_15x15);
        canvas_draw_icon(canvas, 1, 21, &I_WarningDolphin_45x42);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 48, 37, "Awaiting bluetooth");
    } else {
        // "Controller" Screen
        canvas_draw_icon(canvas, 0, 0, &I_Ble_connected_15x15);
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 17, 3, AlignLeft, AlignTop, "Remote");
        canvas_draw_icon(canvas, 68, 2, &I_Pin_back_arrow_10x8);
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(canvas, 127, 3, AlignRight, AlignTop, "Hold to exit");
        
        // D-Pad buttons
        canvas_draw_icon(canvas, 21, 24, &I_Button_18x18); // Up
        if(app->controller_state.up_pressed) {
            elements_slightly_rounded_box(canvas, 24, 26, 13, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        hid_keynote_draw_arrow(canvas, 30, 30, CanvasDirectionBottomToTop);
        canvas_set_color(canvas, ColorBlack);
       
        canvas_draw_icon(canvas, 21, 45, &I_Button_18x18); // Down
        if(app->controller_state.down_pressed) {
            elements_slightly_rounded_box(canvas, 24, 47, 13, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        hid_keynote_draw_arrow(canvas, 30, 55, CanvasDirectionTopToBottom);
        canvas_set_color(canvas, ColorBlack);
       
        canvas_draw_icon(canvas, 0, 45, &I_Button_18x18); // Left
        if(app->controller_state.left_pressed) {
            elements_slightly_rounded_box(canvas, 3, 47, 13, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        hid_keynote_draw_arrow(canvas, 7, 53, CanvasDirectionRightToLeft);
        canvas_set_color(canvas, ColorBlack);
        
        canvas_draw_icon(canvas, 42, 45, &I_Button_18x18); // Right
        if(app->controller_state.right_pressed) {
            elements_slightly_rounded_box(canvas, 45, 47, 13, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        hid_keynote_draw_arrow(canvas, 53, 53, CanvasDirectionLeftToRight);
        canvas_set_color(canvas, ColorBlack);
      
        canvas_draw_icon(canvas, 63, 24, &I_Space_65x18); // OK
        if(app->controller_state.ok_pressed) {
            elements_slightly_rounded_box(canvas, 66, 26, 60, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_icon(canvas, 74, 28, &I_Ok_btn_9x9);
        elements_multiline_text_aligned(canvas, 91, 36, AlignLeft, AlignBottom, "OK");
        canvas_set_color(canvas, ColorBlack);
        
        canvas_draw_icon(canvas, 63, 45, &I_Space_65x18); // Back
        if(app->controller_state.back_pressed) {
            elements_slightly_rounded_box(canvas, 66, 47, 60, 13);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_icon(canvas, 74, 49, &I_Pin_back_arrow_10x8);
        elements_multiline_text_aligned(canvas, 91, 57, AlignLeft, AlignBottom, "Back");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void anki_remote_scene_controller_input_handler(AnkiRemoteApp* app, InputEvent* event) {
    // Bug fix: #1 if you hold back, go to the menu
    // #2 if you just press it, it does what you set it to
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeLong || (!app->connected && event->type == InputTypeShort)) {
            anki_remote_set_scene(app, SceneMenu);
            return;
        }
    }
    bool is_press = (event->type == InputTypePress);
    bool is_release = (event->type == InputTypeRelease);
    if(is_press || is_release) {
        KeyMapping mapping = {0};
        bool key_event = true;
        switch(event->key) {
        case InputKeyUp:
            app->controller_state.up_pressed = is_press;
            mapping = app->keymap[FlipperButtonUp];
            break;
        case InputKeyDown:
            app->controller_state.down_pressed = is_press;
            mapping = app->keymap[FlipperButtonDown];
            break;
        case InputKeyLeft:
            app->controller_state.left_pressed = is_press;
            mapping = app->keymap[FlipperButtonLeft];
            break;
        case InputKeyRight:
            app->controller_state.right_pressed = is_press;
            mapping = app->keymap[FlipperButtonRight];
            break;
        case InputKeyOk:
            app->controller_state.ok_pressed = is_press;
            mapping = app->keymap[FlipperButtonOk];
            break;
        case InputKeyBack:
            app->controller_state.back_pressed = is_press;
            mapping = app->keymap[FlipperButtonBack];
            break;
        default:
            key_event = false;
            break;
        }
        if(key_event && mapping.keycode != 0) {
            anki_remote_send_key_combo(app, mapping, is_press);
        }
    }
}


// - - - Secton 5c: Settings Scene - - -


// Gets name of the button being configured
static const char* get_button_name(FlipperButton button) {
    switch(button) {
    case FlipperButtonUp:
        return "Up";
    case FlipperButtonDown:
        return "Down";
    case FlipperButtonLeft:
        return "Left";
    case FlipperButtonRight:
        return "Right";
    case FlipperButtonOk:
        return "OK";
    case FlipperButtonBack:
        return "Back";
    default:
        return "??"; // Won't happen though lol
    }
}

// Builds a string name for a key combination (e.g., "Ctrl+Shift+A")
void get_key_combo_name(KeyMapping mapping, char* buffer, size_t buffer_size) {
    buffer[0] = '\0';

    // v1.1
    if(mapping.keycode == 0) {
        strlcpy(buffer, "Not mapped", buffer_size);
        return;
    }

    if(mapping.modifiers & MOD_CTRL_BIT) strlcat(buffer, "Ctrl+", buffer_size);
    if(mapping.modifiers & MOD_ALT_BIT) strlcat(buffer, "Alt+", buffer_size);
    if(mapping.modifiers & MOD_GUI_BIT) strlcat(buffer, "Cmd+", buffer_size);
    const char* base_key_name = "???";
    bool found = false;
    
    // Loop through virtual keyboard layout to find the keycode.
    for(uint8_t y = 0; y < KEYBOARD_ROW_COUNT && !found; ++y) {
        for(uint8_t x = 0; x < KEYBOARD_COLUMN_COUNT && !found; ++x) {
            const HidKeyboardKey* key = &hid_keyboard_keyset[y][x];
            if(key->value == mapping.keycode) {
                found = true;
                // This took 5 hours to figure out how to do
                if((mapping.modifiers & MOD_SHIFT_BIT) && key->shift_key != 0) {
                    char s[2] = {key->shift_key, '\0'};
                    strlcat(buffer, s, buffer_size);
                } else if(key->key != 0) {
                    char s[2] = {key->key, '\0'};
                    strlcat(buffer, s, buffer_size);
                } else {
                    if(key->value >= HID_KEYBOARD_A && key->value <= HID_KEYBOARD_Z) {
                        char s[2] = {'A' + (key->value - HID_KEYBOARD_A), '\0'};
                        strlcat(buffer, s, buffer_size);
                    } else if(key->value >= HID_KEYBOARD_1 && key->value <= HID_KEYBOARD_9) {
                        char s[2] = {'1' + (key->value - HID_KEYBOARD_1), '\0'};
                        strlcat(buffer, s, buffer_size);
                    } else if(key->value == HID_KEYBOARD_0) {
                        strlcat(buffer, "0", buffer_size);
                    } else {
                        switch(key->value) {
                        case HID_KEYBOARD_UP_ARROW:
                            base_key_name = "Up";
                            break;
                        case HID_KEYBOARD_DOWN_ARROW:
                            base_key_name = "Down";
                            break;
                        case HID_KEYBOARD_LEFT_ARROW:
                            base_key_name = "Left";
                            break;
                        case HID_KEYBOARD_RIGHT_ARROW:
                            base_key_name = "Right";
                            break;
                        case HID_KEYBOARD_SPACEBAR:
                            base_key_name = "Space";
                            break;
                        case HID_KEYBOARD_RETURN:
                            base_key_name = "Enter";
                            break;
                        case HID_KEYBOARD_ESCAPE:
                            base_key_name = "Esc";
                            break;
                        case HID_KEYBOARD_DELETE:
                            base_key_name = "Bsp";
                            break;
                        case HID_KEYBOARD_DELETE_FORWARD:
                            base_key_name = "Del";
                            break;
                        case HID_KEYBOARD_TAB:
                            base_key_name = "Tab";
                            break;
                        case HID_KEYBOARD_F1:
                            base_key_name = "F1";
                            break;
                        case HID_KEYBOARD_F2:
                            base_key_name = "F2";
                            break;
                        case HID_KEYBOARD_F3:
                            base_key_name = "F3";
                            break;
                        case HID_KEYBOARD_F4:
                            base_key_name = "F4";
                            break;
                        case HID_KEYBOARD_F5:
                            base_key_name = "F5";
                            break;
                        case HID_KEYBOARD_F6:
                            base_key_name = "F6";
                            break;
                        case HID_KEYBOARD_F7:
                            base_key_name = "F7";
                            break;
                        case HID_KEYBOARD_F8:
                            base_key_name = "F8";
                            break;
                        case HID_KEYBOARD_F9:
                            base_key_name = "F9";
                            break;
                        case HID_KEYBOARD_F10:
                            base_key_name = "F10";
                            break;
                        case HID_KEYBOARD_F11:
                            base_key_name = "F11";
                            break;
                        case HID_KEYBOARD_F12:
                            base_key_name = "F12";
                            break;
                        case HID_KEYBOARD_OPEN_BRACKET:
                            base_key_name = "[";
                            break;
                        case HID_KEYBOARD_CLOSE_BRACKET:
                            base_key_name = "]";
                            break;
                        case HID_KEYBOARD_BACKSLASH:
                            base_key_name = "\\";
                            break;
                        case HID_KEYBOARD_SLASH:
                            base_key_name = "/";
                            break;
                        case HID_KEYBOARD_APOSTROPHE:
                            base_key_name = "'";
                            break;
                        case HID_KEYBOARD_GRAVE_ACCENT:
                            base_key_name = "`";
                            break;
                        case HID_KEYBOARD_EQUAL_SIGN:
                            base_key_name = "=";
                            break;
                        case HID_KEYBOARD_L_SHIFT:
                            base_key_name = "Shift";
                            break;
                        case HID_KEYBOARD_L_CTRL:
                            base_key_name = "Ctrl";
                            break;
                        case HID_KEYBOARD_L_ALT:
                            base_key_name = "Alt";
                            break;
                        case HID_KEYBOARD_L_GUI:
                            base_key_name = "Cmd";
                            break;
                        default:
                            base_key_name = "Key";
                            break;
                        }
                        strlcat(buffer, base_key_name, buffer_size);
                    }
                }
            }
        }
    }
}

// Gets brightness label
static const char* get_led_brightness_label(uint8_t brightness) {
    if(brightness == 0) return "0%";
    else if(brightness <= 64) return "25%";
    else if(brightness <= 128) return "50%";
    else if(brightness <= 192) return "75%";
    else return "100%";
}

// Draws the sub menu
static void anki_remote_scene_settings_menu_draw_callback(Canvas* canvas, void* context) {
    AnkiRemoteApp* app = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Remote Settings"); // v1.2: changed from "Button Settings"
    canvas_set_font(canvas, FontSecondary);
    
    const uint8_t total_items = NUM_FLIPPER_BUTTONS + 2; // v1.2: +2 now for Reset + LED
    const uint8_t visible_items = 5;
    uint8_t y_start = 19;
    uint8_t y_spacing = 10; // v1.1: for better visibility

    for(uint8_t i = 0; i < visible_items; ++i) {
        uint8_t item_index = app->settings_menu_state.scroll_offset + i;
        if(item_index >= total_items) break;

        uint8_t y_pos = y_start + (i * y_spacing);
        char label[32];
        bool is_led_item = (item_index == NUM_FLIPPER_BUTTONS + 1);
        bool is_selected = (item_index == app->settings_menu_state.selected_item);

        if(item_index < NUM_FLIPPER_BUTTONS) {
            char key_name_buffer[24];
            get_key_combo_name(app->keymap[item_index], key_name_buffer, sizeof(key_name_buffer));
            snprintf(label, sizeof(label), "%s: %s", get_button_name(item_index), key_name_buffer);
        } else if(item_index == NUM_FLIPPER_BUTTONS) {
            strlcpy(label, "Reset Mappings", sizeof(label));
        } else {
            strlcpy(label, "LED Brightness:", sizeof(label));
        }

        if(is_selected) {
            elements_slightly_rounded_box(canvas, 2, y_pos - 7, 124, 9);
            canvas_set_color(canvas, ColorWhite);
        }

        // Draw the label or prefix
        uint8_t label_x = 5;
        canvas_draw_str(canvas, label_x, y_pos, label);
        
        // v1.2: draw percentage with arrows around if selected
        if(is_led_item) {
            const char* percent_str = get_led_brightness_label(app->led_brightness);
            uint8_t char_width = 5;
            uint8_t label_width = strlen(label) * char_width;
            uint8_t end_of_label = label_x + label_width;
            uint8_t right_edge = 123;
            uint8_t available = right_edge - end_of_label;
            uint8_t percent_len = strlen(percent_str);
            uint8_t percent_width = percent_len * char_width;

            if(is_selected) {
                uint8_t arrow_width = char_width;
                uint8_t total_width = arrow_width + 1 + percent_width + 6 + arrow_width;
                uint8_t start_x = end_of_label + (available - total_width) / 2 + 1;
                canvas_draw_str(canvas, start_x, y_pos, "<");
                canvas_draw_str(canvas, start_x + arrow_width + 1, y_pos, percent_str);
                canvas_draw_str(canvas, start_x + arrow_width + 1 + percent_width + 6, y_pos, ">");
            } else {
                uint8_t start_x = end_of_label + (available - percent_width) / 2 + 1;
                canvas_draw_str(canvas, start_x, y_pos, percent_str);
            }
        }
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, app->settings_menu_state.selected_item, total_items);
}

// Settings menu inputs (not in the keyboard)
static void anki_remote_scene_settings_menu_input_handler(AnkiRemoteApp* app, InputEvent* event) {
    if(app->ignore_next_ok_release && event->key == InputKeyOk &&
       (event->type == InputTypeShort || event->type == InputTypeRelease)) {
        app->ignore_next_ok_release = false;
        return;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    const uint8_t settings_menu_item_count = NUM_FLIPPER_BUTTONS + 2; // v1.2: +2 (Reset + LED)
    uint8_t selected_item = app->settings_menu_state.selected_item;

    if(event->key == InputKeyUp) {
        selected_item = (selected_item + settings_menu_item_count - 1) % settings_menu_item_count;
    } else if(event->key == InputKeyDown) {
        selected_item = (selected_item + 1) % settings_menu_item_count;
    } else if(event->key == InputKeyOk) {
        if(selected_item < NUM_FLIPPER_BUTTONS) {
            app->settings_state.configuring_button = selected_item;
            anki_remote_set_scene(app, SceneSettings);
        } else if(selected_item == NUM_FLIPPER_BUTTONS) { // Reset button is pressed
            anki_remote_set_default_keymap(app);
            anki_remote_save_keymap(app);
        } // v1.2: LED is changed via left and right, not by OK
    } else if(event->key == InputKeyBack) {
        anki_remote_set_scene(app, SceneMenu);
    } else if(selected_item == (NUM_FLIPPER_BUTTONS + 1)) {
        if(event->key == InputKeyLeft) {
            // Cycle down
            if(app->led_brightness == 0) app->led_brightness = 255;
            else if(app->led_brightness == 64) app->led_brightness = 0;
            else if(app->led_brightness == 128) app->led_brightness = 64;
            else if(app->led_brightness == 192) app->led_brightness = 128;
            else app->led_brightness = 192;
            anki_remote_save_keymap(app);
        } else if(event->key == InputKeyRight) {
            // Cycle up
            if(app->led_brightness == 0) app->led_brightness = 64;
            else if(app->led_brightness == 64) app->led_brightness = 128;
            else if(app->led_brightness == 128) app->led_brightness = 192;
            else if(app->led_brightness == 192) app->led_brightness = 255;
            else app->led_brightness = 0;
            anki_remote_save_keymap(app);
        }
    }

    // Scrolling Logic
    const uint8_t visible_items = 5;
    if(selected_item < app->settings_menu_state.scroll_offset) {
        app->settings_menu_state.scroll_offset = selected_item;
    } else if(selected_item >= app->settings_menu_state.scroll_offset + visible_items) {
        app->settings_menu_state.scroll_offset = selected_item - visible_items + 1;
    }

    app->settings_menu_state.selected_item = selected_item;
}


// - - - Section 5d: Keyboard Selection Scene - - -


// Goofy ahh key drawing algorithm from BT remote
static void hid_keyboard_draw_key(
    Canvas* canvas,
    const HidKeyboardKey* key,
    uint8_t x,
    uint8_t y,
    bool selected,
    const struct AnkiRemoteApp* app) {
    if(!key->width) return;
#define MARGIN_TOP 12
#define MARGIN_LEFT 3
#define KEY_WIDTH 11
#define KEY_HEIGHT 13
#define KEY_PADDING -1
    canvas_set_color(canvas, ColorBlack);
    uint8_t key_width = KEY_WIDTH * key->width + KEY_PADDING * (key->width - 1);
    if(selected) {
        elements_slightly_rounded_box(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING),
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING),
            key_width,
            KEY_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    } else {
        elements_slightly_rounded_frame(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING),
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING),
            key_width,
            KEY_HEIGHT);
    }
    bool shift = app->settings_state.shift_pressed;
    bool is_toggled =
        (key->value == HID_KEYBOARD_L_SHIFT && app->settings_state.shift_pressed) ||
        (key->value == HID_KEYBOARD_L_CTRL && app->settings_state.ctrl_pressed) ||
        (key->value == HID_KEYBOARD_L_ALT && app->settings_state.alt_pressed) ||
        (key->value == HID_KEYBOARD_L_GUI && app->settings_state.gui_pressed);
    if(is_toggled && key->icon_toggled) {
        const Icon* icon = key->icon_toggled;
        canvas_draw_icon(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + key_width / 2 - icon->width / 2,
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + KEY_HEIGHT / 2 - icon->height / 2,
            icon);
        return;
    }
    if(shift) {
        // If Shift is active and the key has a special icon for Shift...
        if(key->icon_shift) {
            const Icon* icon = key->icon_shift;
            canvas_draw_icon(
                canvas,
                MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + key_width / 2 - icon->width / 2,
                MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + KEY_HEIGHT / 2 - icon->height / 2,
                icon);
            return;
        }
        
        // If Shift is active and the key has a special character for Shift...
        if(key->shift_key != 0) {
            char text[2] = {key->shift_key, '\0'};
            canvas_draw_str_aligned(
                canvas,
                MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + key_width / 2 + 1,
                MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + KEY_HEIGHT / 2 + 1,
                AlignCenter,
                AlignCenter,
                text);
            return;
        }
    }

    // If no special shifted icon/character, try to draw the regular icon.
    if(key->icon) {
        const Icon* icon = key->icon;
        canvas_draw_icon(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + key_width / 2 - icon->width / 2,
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + KEY_HEIGHT / 2 - icon->height / 2,
            icon);
        return;
    }
    
    // If no icon, try to draw the regular character
    if(key->key != 0) {
        char text[2] = {key->key, '\0'};
        canvas_draw_str_aligned(
            canvas,
            MARGIN_LEFT + x * (KEY_WIDTH + KEY_PADDING) + key_width / 2 + 1,
            MARGIN_TOP + y * (KEY_HEIGHT + KEY_PADDING) + KEY_HEIGHT / 2 + 1,
            AlignCenter,
            AlignCenter,
            text);
        return;
    }
}

// Drawing the entire keyboard
static void anki_remote_scene_settings_draw_callback(Canvas* canvas, void* context) {
    AnkiRemoteApp* app = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    char header_str[40];
    snprintf(
        header_str,
        sizeof(header_str),
        "Set key for %s button",
        get_button_name(app->settings_state.configuring_button));
    canvas_draw_str(canvas, 2, 10, header_str);
    canvas_set_font(canvas, FontKeyboard);
    uint8_t start_row = (app->settings_state.y > 3) ? (app->settings_state.y - 3) : 0;
    for(uint8_t y = start_row; y < KEYBOARD_ROW_COUNT; y++) {
        uint8_t current_x = 0;
        for(uint8_t x_idx = 0; x_idx < KEYBOARD_COLUMN_COUNT; x_idx++) {
            const HidKeyboardKey* key = &hid_keyboard_keyset[y][x_idx];
            if(key->width > 0) {
                bool is_selected = (app->settings_state.y == y && app->settings_state.x == x_idx);
                hid_keyboard_draw_key(canvas, key, current_x, y - start_row, is_selected, app);
                current_x += key->width;
            }
        }
    }
    elements_scrollbar(canvas, app->settings_state.y, 7);
}

static void settings_move_cursor(AnkiRemoteApp* app, int8_t dx, int8_t dy) {
    int current_x = app->settings_state.x;
    int current_y = app->settings_state.y;
    if(dy != 0) {
        current_y += dy;
        if(current_y < 0) current_y = KEYBOARD_ROW_COUNT - 1;
        if(current_y >= KEYBOARD_ROW_COUNT) current_y = 0;
    }
    if(dx != 0) {
        current_x += dx;
        while(true) {
            if(current_x < 0) current_x = KEYBOARD_COLUMN_COUNT - 1;
            if(current_x >= KEYBOARD_COLUMN_COUNT) current_x = 0;
            if(hid_keyboard_keyset[current_y][current_x].width != 0) break;
            current_x += dx > 0 ? 1 : -1;
        }
    }

     // BUG FIX: find nearest valid key if you land on empty key slot
    if(hid_keyboard_keyset[current_y][current_x].width == 0) {
        int temp_x = current_x;
        while(hid_keyboard_keyset[current_y][temp_x].width == 0) {
            temp_x--;
            if(temp_x < 0) {
                temp_x = -1;
                break;
            }
        }
        if(temp_x != -1) {
            current_x = temp_x;
        } else {
            while(hid_keyboard_keyset[current_y][current_x].width == 0) {
                current_x++;
                if(current_x >= KEYBOARD_COLUMN_COUNT) {
                    current_x = 0;
                    break;
                }
            }
        }
    }
    app->settings_state.x = current_x;
    app->settings_state.y = current_y;
}

// Button press logic on keyboard screen
static void anki_remote_scene_settings_input_handler(AnkiRemoteApp* app, InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort || event->type == InputTypeLong) {
            anki_remote_set_scene(app, SceneSettingsMenu);
            return;
        }
    }
    if(event->key == InputKeyOk) {
        if(event->type != InputTypePress) return;
    } else {
        if(event->type != InputTypePress && event->type != InputTypeRepeat) return;
    }
    
    // Arrow key presses
    if(event->key == InputKeyUp) settings_move_cursor(app, 0, -1);
    else if(event->key == InputKeyDown) settings_move_cursor(app, 0, 1);
    else if(event->key == InputKeyLeft) settings_move_cursor(app, -1, 0);
    else if(event->key == InputKeyRight) settings_move_cursor(app, 1, 0);
    
    // Select and get HID keycode if OK is pressed
    else if(event->key == InputKeyOk) {
        uint8_t selected_keycode =
            hid_keyboard_keyset[app->settings_state.y][app->settings_state.x].value;
        bool is_modifier = true; // Assume for now it's a modifier
        if(selected_keycode == HID_KEYBOARD_L_SHIFT) {
            app->settings_state.shift_pressed = !app->settings_state.shift_pressed;
        } else if(selected_keycode == HID_KEYBOARD_L_CTRL) {
            app->settings_state.ctrl_pressed = !app->settings_state.ctrl_pressed;
        } else if(selected_keycode == HID_KEYBOARD_L_ALT) {
            app->settings_state.alt_pressed = !app->settings_state.alt_pressed;
        } else if(selected_keycode == HID_KEYBOARD_L_GUI) {
            app->settings_state.gui_pressed = !app->settings_state.gui_pressed;
        } else {
            is_modifier = false; // it wasn't a modifier
        }
        if(is_modifier) {
            view_port_update(app->view_port);
        } else {
            KeyMapping new_mapping;
            new_mapping.keycode = selected_keycode;
            new_mapping.modifiers = 0;
            
            // Add modifiers to keycode if they are toggled
            if(app->settings_state.shift_pressed) new_mapping.modifiers |= MOD_SHIFT_BIT;
            if(app->settings_state.ctrl_pressed) new_mapping.modifiers |= MOD_CTRL_BIT;
            if(app->settings_state.alt_pressed) new_mapping.modifiers |= MOD_ALT_BIT;
            if(app->settings_state.gui_pressed) new_mapping.modifiers |= MOD_GUI_BIT;
            app->keymap[app->settings_state.configuring_button] = new_mapping;
            anki_remote_save_keymap(app);
            
            // BUG FIX: Set the flag to ignore the upcoming OK button release.
            app->ignore_next_ok_release = true;
            anki_remote_set_scene(app, SceneSettingsMenu);
            return;
        }
    }
}


// - - - Section 6: Main Application Logic - - -

// Decides which screen to draw
static void anki_remote_draw_callback(Canvas* canvas, void* context) {
    AnkiRemoteApp* app = (AnkiRemoteApp*)context;
    furi_assert(app);
    switch(app->current_scene) {
    case SceneMenu:
        anki_remote_scene_menu_draw_callback(canvas, app);
        break;
    case SceneController:
        anki_remote_scene_controller_draw_callback(canvas, app);
        break;
    case SceneSettingsMenu:
        anki_remote_scene_settings_menu_draw_callback(canvas, app);
        break;
    case SceneSettings:
        anki_remote_scene_settings_draw_callback(canvas, app);
        break;
    }
}

// Figures out what to do when a button is pressed
static void anki_remote_input_handler(AnkiRemoteApp* app, InputEvent* event) {
    switch(app->current_scene) {
    case SceneMenu:
        anki_remote_scene_menu_input_handler(app, event);
        break;
    case SceneController:
        anki_remote_scene_controller_input_handler(app, event);
        break;
    case SceneSettingsMenu:
        anki_remote_scene_settings_menu_input_handler(app, event);
        break;
    case SceneSettings:
        anki_remote_scene_settings_input_handler(app, event);
        break;
    }
}

// Gets called by flipper os when a button is pressed
static void anki_remote_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;
    AnkiRemoteEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

// Gets called when bluetooth connects or disconnects
static void anki_remote_connection_status_callback(BtStatus status, void* context) {
    furi_assert(context);
    AnkiRemoteApp* app = context;
    app->connected = (status == BtStatusConnected);
    if(app->connected) {
        furi_hal_light_set(LightBlue, app->led_brightness);
    } else {
        furi_hal_light_set(LightBlue, 0); // Off
    }
    view_port_update(app->view_port);
}

// Sends or releases a key combo over BLE HID
static void anki_remote_send_key_combo(AnkiRemoteApp* app, KeyMapping mapping, bool is_press) {
    if(is_press) {
        // Press modifiers using HID keycodes
        if(mapping.modifiers & MOD_CTRL_BIT) {
            ble_profile_hid_kb_press(app->ble_hid_profile, KEY_MOD_LEFT_CTRL);
        }
        if(mapping.modifiers & MOD_SHIFT_BIT) {
            ble_profile_hid_kb_press(app->ble_hid_profile, KEY_MOD_LEFT_SHIFT);
        }
        if(mapping.modifiers & MOD_ALT_BIT) {
            ble_profile_hid_kb_press(app->ble_hid_profile, KEY_MOD_LEFT_ALT);
        }
        if(mapping.modifiers & MOD_GUI_BIT) {
            ble_profile_hid_kb_press(app->ble_hid_profile, KEY_MOD_LEFT_GUI);
        }
        // Now press the main key
        ble_profile_hid_kb_press(app->ble_hid_profile, mapping.keycode);
    } else {
        // Release all keys (modifiers and main key)
        ble_profile_hid_kb_release_all(app->ble_hid_profile);
    }
}

// Switches between screens
static void anki_remote_set_scene(AnkiRemoteApp* app, AppScene new_scene) {
    AppScene old_scene = app->current_scene;
    if(old_scene == new_scene) return;
    if(old_scene == SceneController) {
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        
        // BUG FIX: Very important so you can use other BLE apps normally
        furi_check(bt_profile_restore_default(app->bt));
    }
    if(new_scene == SceneController) {
        // bt_disconnect(app->bt);
        // furi_delay_ms(200);
        
        // BUG FIX: This prevents buttons from appearing "stuck" as pressed if you left the scene
        // while holding a button down.
        memset(&app->controller_state, 0, sizeof(app->controller_state));
        bt_set_status_changed_callback(app->bt, anki_remote_connection_status_callback, app);

        /*
        // v1.1: Set custom device name prefix (scrapped change)
        // won't work unless you remove cross compatibility
        BleProfileHidParams params = {
            .device_name_prefix = "Control", 
            .mac_xor = 0x0001,
        };
        app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid, &params);
        */

        app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid, NULL);
        furi_check(app->ble_hid_profile);
        furi_hal_bt_start_advertising();
    } else if(new_scene == SceneSettingsMenu) {
        app->settings_menu_state.selected_item = 0; // select first item
        app->settings_menu_state.scroll_offset = 0; // v1.1 for "reset keymappings"
    } else if(new_scene == SceneSettings) {
        app->settings_state.x = 0;
        app->settings_state.y = 1;
        app->settings_state.shift_pressed = false;
        app->settings_state.ctrl_pressed = false;
        app->settings_state.alt_pressed = false;
        app->settings_state.gui_pressed = false;
    }
    app->current_scene = new_scene;
}

// Memory allocation
static AnkiRemoteApp* anki_remote_app_alloc() {
    AnkiRemoteApp* app = malloc(sizeof(AnkiRemoteApp));
    app->event_queue = furi_message_queue_alloc(8, sizeof(AnkiRemoteEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->bt = furi_record_open(RECORD_BT);
    bt_keys_storage_set_storage_path(app->bt, HID_BT_KEYS_STORAGE_PATH);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, anki_remote_draw_callback, app);
    view_port_input_callback_set(app->view_port, anki_remote_input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->running = true;
    app->connected = false;
    app->current_scene = SceneMenu;
    app->menu_state.selected_item = 0;
    app->ignore_next_ok_release = false;
    app->led_brightness = 255;
    anki_remote_load_keymap(app);
    return app;
}

// Free all resources used by app and disconnect bluetooth
static void anki_remote_app_free(AnkiRemoteApp* app) {
    furi_assert(app);
    if(app->current_scene == SceneController) {
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_profile_restore_default(app->bt);
    }
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    // BUG FIX: Restore default Bluetooth key storage path
    // If u don't do this other apps will BREAK !!!!
    bt_keys_storage_set_default_path(app->bt);
    furi_record_close(RECORD_BT);
    app->bt = NULL;
    furi_message_queue_free(app->event_queue);
    free(app);
}


// - - - Final Section: Main Entry Point - - -


int32_t anki_remote_app(void* p) {
    UNUSED(p);
    AnkiRemoteApp* app = anki_remote_app_alloc();
    AnkiRemoteEvent event;
    
    // Main loop
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == EventTypeInput) {
                anki_remote_input_handler(app, &event.input);
            }
        }
        view_port_update(app->view_port);
    }
    
    // Reset LED and clean up
    notification_internal_message(app->notifications, &sequence_reset_blue);
    anki_remote_app_free(app);
    return 0;
}
