#include "payload_manager.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_usb.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include "keylogger_app.h"

#define TAG "PayloadManager"

struct PayloadManager {
    KeyloggerApp* app;
    FuriString* primary_payload_path_str;
    FuriHalUsbInterface* usb_if_prev;
};

// Forward declarations of static functions
static bool setup_usb_hid_mode(PayloadManager* manager);
static void restore_usb_mode(PayloadManager* manager);
static uint16_t char_to_hid(char c);
static bool read_line_from_file(File* file, char* buffer, size_t buffer_size);
static bool execute_ducky_line(
    const char* line,
    void (*progress_callback)(void* ctx, const char* status),
    void* progress_ctx,
    int* default_delay_ms,
    const char** last_line);
static bool execute_payload_file(
    const char* payload_path,
    void (*progress_view_set_status_callback)(void* ctx, const char* status),
    void* progress_view_ctx);

PayloadManager* payload_manager_alloc(KeyloggerApp* app_context) {
    PayloadManager* manager = malloc(sizeof(PayloadManager));
    if(!manager) {
        return NULL;
    }

    manager->app = app_context;
    manager->primary_payload_path_str = furi_string_alloc();
    if(!manager->primary_payload_path_str) {
        free(manager);
        return NULL;
    }

    manager->usb_if_prev = NULL;

    return manager;
}

void payload_manager_free(PayloadManager* manager) {
    if(!manager) {
        return;
    }

    // Ensure USB mode is restored before freeing
    restore_usb_mode(manager);

    if(manager->primary_payload_path_str) {
        furi_string_free(manager->primary_payload_path_str);
    }
    free(manager);
}

// Cleanup function to ensure USB mode is restored
void payload_manager_cleanup(PayloadManager* manager) {
    if(manager) {
        FURI_LOG_I(TAG, "Cleaning up payload manager and restoring USB mode");
        restore_usb_mode(manager);
    }
}

void payload_manager_set_primary_payload_path(PayloadManager* manager, const char* path) {
    furi_assert(manager);
    furi_string_set(manager->primary_payload_path_str, path);
    FuriString* filename_str = furi_string_alloc();
    if(path && path[0] != '\0') {
        path_extract_filename(manager->primary_payload_path_str, filename_str, true);
        FURI_LOG_I(
            TAG,
            "Primary payload set to: %s (full path: %s)",
            furi_string_get_cstr(filename_str),
            path);
    } else {
        FURI_LOG_I(TAG, "Primary payload cleared.");
    }
    furi_string_free(filename_str);
}

// Setup USB HID mode programmatically
static bool setup_usb_hid_mode(PayloadManager* manager) {
    FURI_LOG_I(TAG, "Setting up USB HID mode...");

    // Store current USB interface
    manager->usb_if_prev = (FuriHalUsbInterface*)furi_hal_usb_get_config();

    // Set USB to HID mode
    furi_hal_usb_unlock();
    bool usb_set_result = furi_hal_usb_set_config(&usb_hid, NULL);

    if(!usb_set_result) {
        FURI_LOG_E(TAG, "Failed to set USB HID mode");
        return false;
    }

    // Wait for USB to be ready
    furi_delay_ms(1000);

    FURI_LOG_I(TAG, "USB HID mode enabled successfully");
    return true;
}

// Restore previous USB mode
static void restore_usb_mode(PayloadManager* manager) {
    if(manager->usb_if_prev) {
        FURI_LOG_I(TAG, "Restoring previous USB mode");
        furi_hal_usb_unlock();
        furi_hal_usb_set_config(manager->usb_if_prev, NULL);
        manager->usb_if_prev = NULL;
    }
}

// Helper function to convert character to HID key code
static uint16_t char_to_hid(char c) {
    if((uint8_t)c < 128) {
        return HID_ASCII_TO_KEY(c);
    }
    return HID_KEYBOARD_NONE;
}

// Map a keyword to a single HID key code (no modifiers)
static uint16_t keyword_to_hid(const char* keyword) {
    if(strcmp(keyword, "ENTER") == 0 || strcmp(keyword, "RETURN") == 0) return HID_KEYBOARD_RETURN;
    if(strcmp(keyword, "TAB") == 0) return HID_KEYBOARD_TAB;
    if(strcmp(keyword, "ESCAPE") == 0 || strcmp(keyword, "ESC") == 0) return HID_KEYBOARD_ESCAPE;
    if(strcmp(keyword, "SPACE") == 0) return HID_KEYBOARD_SPACEBAR;
    if(strcmp(keyword, "BACKSPACE") == 0) return HID_KEYBOARD_DELETE;
    if(strcmp(keyword, "DELETE") == 0) return HID_KEYBOARD_DELETE_FORWARD;
    if(strcmp(keyword, "INSERT") == 0) return HID_KEYBOARD_INSERT;
    if(strcmp(keyword, "HOME") == 0) return HID_KEYBOARD_HOME;
    if(strcmp(keyword, "END") == 0) return HID_KEYBOARD_END;
    if(strcmp(keyword, "PAGEUP") == 0) return HID_KEYBOARD_PAGE_UP;
    if(strcmp(keyword, "PAGEDOWN") == 0) return HID_KEYBOARD_PAGE_DOWN;
    if(strcmp(keyword, "UPARROW") == 0 || strcmp(keyword, "UP") == 0) return HID_KEYBOARD_UP_ARROW;
    if(strcmp(keyword, "DOWNARROW") == 0 || strcmp(keyword, "DOWN") == 0)
        return HID_KEYBOARD_DOWN_ARROW;
    if(strcmp(keyword, "LEFTARROW") == 0 || strcmp(keyword, "LEFT") == 0)
        return HID_KEYBOARD_LEFT_ARROW;
    if(strcmp(keyword, "RIGHTARROW") == 0 || strcmp(keyword, "RIGHT") == 0)
        return HID_KEYBOARD_RIGHT_ARROW;
    if(strcmp(keyword, "CAPSLOCK") == 0) return HID_KEYBOARD_CAPS_LOCK;
    if(strcmp(keyword, "NUMLOCK") == 0) return HID_KEYBOARD_LOCK_NUM_LOCK;
    if(strcmp(keyword, "SCROLLLOCK") == 0) return HID_KEYBOARD_LOCK_SCROLL_LOCK;
    if(strcmp(keyword, "PRINTSCREEN") == 0) return HID_KEYBOARD_PRINT_SCREEN;
    if(strcmp(keyword, "PAUSE") == 0 || strcmp(keyword, "BREAK") == 0) return HID_KEYBOARD_PAUSE;
    if(strcmp(keyword, "MENU") == 0 || strcmp(keyword, "APP") == 0)
        return HID_KEYBOARD_APPLICATION;
    if(strcmp(keyword, "F1") == 0) return HID_KEYBOARD_F1;
    if(strcmp(keyword, "F2") == 0) return HID_KEYBOARD_F2;
    if(strcmp(keyword, "F3") == 0) return HID_KEYBOARD_F3;
    if(strcmp(keyword, "F4") == 0) return HID_KEYBOARD_F4;
    if(strcmp(keyword, "F5") == 0) return HID_KEYBOARD_F5;
    if(strcmp(keyword, "F6") == 0) return HID_KEYBOARD_F6;
    if(strcmp(keyword, "F7") == 0) return HID_KEYBOARD_F7;
    if(strcmp(keyword, "F8") == 0) return HID_KEYBOARD_F8;
    if(strcmp(keyword, "F9") == 0) return HID_KEYBOARD_F9;
    if(strcmp(keyword, "F10") == 0) return HID_KEYBOARD_F10;
    if(strcmp(keyword, "F11") == 0) return HID_KEYBOARD_F11;
    if(strcmp(keyword, "F12") == 0) return HID_KEYBOARD_F12;
    // Single letter/number: convert via char_to_hid
    if(strlen(keyword) == 1) return char_to_hid(keyword[0]);
    return HID_KEYBOARD_NONE;
}

// Simple string reading from file (replacement for storage_file_gets)
static bool read_line_from_file(File* file, char* buffer, size_t buffer_size) {
    if(!file || !buffer || buffer_size == 0) {
        return false;
    }

    size_t pos = 0;
    uint8_t ch;
    bool found_line = false;

    while(pos < buffer_size - 1) {
        uint16_t bytes_read = storage_file_read(file, &ch, 1);
        if(bytes_read != 1) {
            // End of file or error
            break;
        }

        found_line = true; // We read at least one character from the file

        if(ch == '\n') {
            // End of line
            break;
        }

        if(ch != '\r') { // Skip carriage return
            buffer[pos++] = ch;
        }
    }

    buffer[pos] = '\0';
    return found_line; // Return true if we read any character from file (including empty lines)
}

// Helper: press a key combo, hold briefly, release
static void press_and_release(uint16_t key, uint16_t hold_ms, uint16_t after_ms) {
    furi_hal_hid_kb_press(key);
    furi_delay_ms(hold_ms);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(after_ms);
}

// Helper: press a modifier+key combo with clean state
static void press_modified_key(uint16_t key, uint16_t modifier) {
    furi_hal_hid_kb_release_all();
    furi_delay_ms(50);
    press_and_release(key | modifier, 100, 100);
}

// Execute a single DuckyScript line
static bool execute_ducky_line(
    const char* line,
    void (*progress_callback)(void* ctx, const char* status),
    void* progress_ctx,
    int* default_delay_ms,
    const char** last_line) {
    if(!line) return true;

    size_t len = strlen(line);
    if(len == 0) return true;

    // Skip whitespace-only lines
    bool only_whitespace = true;
    for(size_t i = 0; i < len; i++) {
        if(line[i] != ' ' && line[i] != '\t') {
            only_whitespace = false;
            break;
        }
    }
    if(only_whitespace) return true;

    // Skip comments (REM ...)
    if(strncmp(line, "REM", 3) == 0 && (len == 3 || line[3] == ' ')) return true;

    // DEFAULT_DELAY / DEFAULTDELAY
    if(strncmp(line, "DEFAULT_DELAY ", 14) == 0 || strncmp(line, "DEFAULTDELAY ", 13) == 0) {
        const char* val = strchr(line, ' ') + 1;
        *default_delay_ms = atoi(val);
        FURI_LOG_I(TAG, "Default delay set to %d ms", *default_delay_ms);
        return true;
    }

    // DELAY
    if(strncmp(line, "DELAY", 5) == 0) {
        int delay_ms = 500;
        if(len > 6) delay_ms = atoi(line + 6);
        FURI_LOG_D(TAG, "DELAY %d ms", delay_ms);
        furi_delay_ms(delay_ms);
        return true;
    }

    // REPEAT
    if(strncmp(line, "REPEAT ", 7) == 0) {
        int count = atoi(line + 7);
        if(count < 1) count = 1;
        if(count > 999) count = 999;
        if(*last_line) {
            FURI_LOG_I(TAG, "REPEAT %d times: %s", count, *last_line);
            for(int i = 0; i < count; i++) {
                execute_ducky_line(
                    *last_line, progress_callback, progress_ctx, default_delay_ms, last_line);
                if(*default_delay_ms > 0) furi_delay_ms(*default_delay_ms);
            }
        }
        return true;
    }

    // STRING / STRINGLN
    if(strncmp(line, "STRING", 6) == 0) {
        bool add_enter = (strncmp(line, "STRINGLN", 8) == 0);
        const char* text = line + (add_enter ? 9 : 7);
        if(progress_callback) progress_callback(progress_ctx, "Typing text...");
        for(size_t i = 0; i < strlen(text); i++) {
            uint16_t hid_key = char_to_hid(text[i]);
            if(hid_key != HID_KEYBOARD_NONE) {
                furi_hal_hid_kb_press(hid_key);
                furi_delay_ms(30);
                furi_hal_hid_kb_release(hid_key);
                furi_delay_ms(30);
            }
        }
        if(add_enter) {
            press_and_release(HID_KEYBOARD_RETURN, 50, 100);
        }
        return true;
    }

    // GUI / WINDOWS / SUPER — with any key argument
    if(strncmp(line, "GUI", 3) == 0 || strncmp(line, "WINDOWS", 7) == 0 ||
       strncmp(line, "SUPER", 5) == 0) {
        const char* arg = strchr(line, ' ');
        furi_hal_hid_kb_release_all();
        furi_delay_ms(50);
        if(arg && *(arg + 1)) {
            arg++;
            uint16_t key = keyword_to_hid(arg);
            if(key != HID_KEYBOARD_NONE) {
                press_and_release(key | KEY_MOD_LEFT_GUI, 150, 100);
            }
        } else {
            press_and_release(HID_KEYBOARD_L_GUI | KEY_MOD_LEFT_GUI, 150, 100);
        }
        return true;
    }

    // CTRL / CONTROL — with optional additional modifiers and key
    if(strncmp(line, "CTRL ", 5) == 0 || strncmp(line, "CONTROL ", 8) == 0) {
        const char* rest = strchr(line, ' ') + 1;
        uint16_t modifier = KEY_MOD_LEFT_CTRL;

        // Check for CTRL SHIFT, CTRL ALT combinations
        if(strncmp(rest, "SHIFT ", 6) == 0) {
            modifier |= KEY_MOD_LEFT_SHIFT;
            rest += 6;
        } else if(strncmp(rest, "ALT ", 4) == 0) {
            modifier |= KEY_MOD_LEFT_ALT;
            rest += 4;
        }

        uint16_t key = keyword_to_hid(rest);
        if(key != HID_KEYBOARD_NONE) {
            press_modified_key(key, modifier);
        }
        return true;
    }

    // ALT — with key argument
    if(strncmp(line, "ALT ", 4) == 0) {
        const char* arg = line + 4;
        uint16_t key = keyword_to_hid(arg);
        if(key != HID_KEYBOARD_NONE) {
            press_modified_key(key, KEY_MOD_LEFT_ALT);
        }
        return true;
    }

    // SHIFT — with key argument
    if(strncmp(line, "SHIFT ", 6) == 0) {
        const char* arg = line + 6;
        uint16_t key = keyword_to_hid(arg);
        if(key != HID_KEYBOARD_NONE) {
            press_modified_key(key, KEY_MOD_LEFT_SHIFT);
        }
        return true;
    }

    // Standalone key commands (ENTER, TAB, ESC, arrows, F-keys, etc.)
    uint16_t standalone = keyword_to_hid(line);
    if(standalone != HID_KEYBOARD_NONE) {
        FURI_LOG_D(TAG, "Key: %s", line);
        press_and_release(standalone, 50, 100);
        return true;
    }

    FURI_LOG_W(TAG, "Unknown DuckyScript command: %s", line);
    return true;
}

// Internal function to execute payload without USB mode management
static bool execute_payload_file(
    const char* payload_path,
    void (*progress_view_set_status_callback)(void* ctx, const char* status),
    void* progress_view_ctx) {
    if(!payload_path || strlen(payload_path) == 0) {
        FURI_LOG_E(TAG, "Empty payload path");
        return false;
    }

    FURI_LOG_I(TAG, "Executing DuckyScript payload: %s", payload_path);

    // Open and read the payload file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage");
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to allocate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = false;
    if(storage_file_open(file, payload_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "Successfully opened payload file");

        furi_hal_hid_kb_release_all();

        char line_buffer[256];
        char last_line_buffer[256] = {0};
        const char* last_line_ptr = NULL;
        int default_delay_ms = 0;
        success = true;
        int line_count = 0;

        while(read_line_from_file(file, line_buffer, sizeof(line_buffer))) {
            line_count++;
            FURI_LOG_D(TAG, "Line %d: '%s'", line_count, line_buffer);

            if(!execute_ducky_line(
                   line_buffer,
                   progress_view_set_status_callback,
                   progress_view_ctx,
                   &default_delay_ms,
                   &last_line_ptr)) {
                FURI_LOG_E(TAG, "Failed at line %d: %s", line_count, line_buffer);
                success = false;
                break;
            }

            // Store last line for REPEAT (skip REM/REPEAT/DEFAULT_DELAY)
            if(strncmp(line_buffer, "REM", 3) != 0 && strncmp(line_buffer, "REPEAT", 6) != 0 &&
               strncmp(line_buffer, "DEFAULT_DELAY", 13) != 0 &&
               strncmp(line_buffer, "DEFAULTDELAY", 12) != 0) {
                strncpy(last_line_buffer, line_buffer, sizeof(last_line_buffer) - 1);
                last_line_ptr = last_line_buffer;
            }

            if(default_delay_ms > 0) furi_delay_ms(default_delay_ms);
        }

        FURI_LOG_I(TAG, "File reading completed. Total lines processed: %d", line_count);

        // Release all keys when done
        furi_hal_hid_kb_release_all();

        storage_file_close(file);

        if(success) {
            FURI_LOG_I(TAG, "DuckyScript execution completed successfully");
            if(progress_view_set_status_callback) {
                progress_view_set_status_callback(
                    progress_view_ctx, "DuckyScript executed successfully");
            }
        } else {
            FURI_LOG_E(TAG, "DuckyScript execution failed");
            if(progress_view_set_status_callback) {
                progress_view_set_status_callback(
                    progress_view_ctx, "DuckyScript execution failed");
            }
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open payload file: %s", payload_path);
        if(progress_view_set_status_callback) {
            progress_view_set_status_callback(
                progress_view_ctx, "Error: Could not open payload file");
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

bool payload_manager_execute_primary_phase(
    PayloadManager* manager,
    bool use_usb,
    void (*progress_view_set_status_callback)(void* ctx, const char* status),
    void* progress_view_ctx) {
    furi_assert(manager);

    if(!use_usb) {
        FURI_LOG_W(TAG, "Bluetooth BadUSB not supported in this implementation");
        if(progress_view_set_status_callback) {
            progress_view_set_status_callback(progress_view_ctx, "Error: BT not supported");
        }
        return false;
    }

    if(furi_string_empty(manager->primary_payload_path_str)) {
        FURI_LOG_E(TAG, "No primary payload path set");
        if(progress_view_set_status_callback) {
            progress_view_set_status_callback(progress_view_ctx, "Error: No payload file");
        }
        return false;
    }

    // Set up USB HID mode programmatically
    if(progress_view_set_status_callback) {
        progress_view_set_status_callback(progress_view_ctx, "Setting up USB HID mode...");
    }

    if(!setup_usb_hid_mode(manager)) {
        FURI_LOG_E(TAG, "Failed to setup USB HID mode");
        if(progress_view_set_status_callback) {
            progress_view_set_status_callback(progress_view_ctx, "Error: USB HID setup failed");
        }
        return false;
    }

    if(progress_view_set_status_callback) {
        progress_view_set_status_callback(progress_view_ctx, "Executing DuckyScript...");
    }

    // Use the internal function to execute the payload
    return execute_payload_file(
        furi_string_get_cstr(manager->primary_payload_path_str),
        progress_view_set_status_callback,
        progress_view_ctx);
}
