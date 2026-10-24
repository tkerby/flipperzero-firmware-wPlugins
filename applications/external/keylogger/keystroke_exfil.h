#pragma once

#include <furi.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>
#include "progress_view.h" // For ProgressView type

#define EXFIL_BUFFER_SIZE 2048 // Max size of exfiltrated data in one go
#define DATA_TIMEOUT_MS \
    300000 // Timeout for no new data during exfil (5 minutes for LED transmission)

// Structure to hold exfiltration process state
typedef struct {
    Storage* storage;
    FuriString* target_file_path; // Path to the current exfiltration file
    uint8_t exfil_buffer[EXFIL_BUFFER_SIZE];
    uint16_t exfil_data_len;
    uint32_t last_led_state_time;
} KeystrokeExfilContext;

/**
 * Allocate and initialize a new KeystrokeExfilContext.
 * @param storage_instance Pointer to the Storage instance.
 * @return Pointer to the newly allocated KeystrokeExfilContext.
 */
KeystrokeExfilContext* keystroke_exfil_alloc(Storage* storage_instance);

/**
 * Free the KeystrokeExfilContext.
 * @param context Pointer to the KeystrokeExfilContext to free.
 */
void keystroke_exfil_free(KeystrokeExfilContext* context);

/**
 * Starts the keylogger data capture process.
 * This function will be responsible for polling LED states, decoding data,
 * and saving it to a file. The loop continues as long as progress_view_is_running() is true.
 * 
 * @param context Pointer to the KeystrokeExfilContext.
 * @param output_filename The desired name for the output file in the exfil directory.
 * @param progress_view_set_status_callback Function pointer to update progress view status.
 * @param progress_view_cb_context Context for the progress view callback (typically the ProgressView* itself).
 * @param progress_view Pointer to the ProgressView instance to check for abort status.
 * @return True if exfiltration was successful (any data written), false otherwise (error, timeout, or abort with no data).
 */
bool keystroke_exfil_start_capture(
    KeystrokeExfilContext* context,
    const char* output_filename,
    void (*progress_view_set_status_callback)(void* cb_ctx, const char* status),
    void* progress_view_cb_context,
    ProgressView* progress_view);

/**
 * Get the target file path from the exfil context.
 * @param context Pointer to the KeystrokeExfilContext.
 * @return Const char pointer to the target file path string.
 */
const char* keystroke_exfil_get_target_path(KeystrokeExfilContext* context);
