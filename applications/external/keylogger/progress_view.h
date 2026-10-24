#pragma once

#include <furi.h>          // For FuriCore types
#include <gui/canvas.h>    // For Canvas type
#include <gui/elements.h>  // Should declare elements_go_back_button
#include <gui/view.h>      // For View type

// Forward declaration of the ProgressView structure
typedef struct ProgressView ProgressView;

/**
 * Allocate and initialize a new ProgressView.
 * @return Pointer to the newly allocated ProgressView.
 */
ProgressView* progress_view_alloc();

/**
 * Free the ProgressView.
 * @param progress_view Pointer to the ProgressView to free.
 */
void progress_view_free(ProgressView* progress_view);

/**
 * Get the underlying View object from the ProgressView.
 * @param progress_view Pointer to the ProgressView.
 * @return Pointer to the View object.
 */
View* progress_view_get_view(ProgressView* progress_view);

/**
 * Set the status text to be displayed on the ProgressView.
 * @param progress_view Pointer to the ProgressView.
 * @param status_text The text to display as the current status.
 */
void progress_view_set_status(ProgressView* progress_view, const char* status_text);

/**
 * Check if the operation associated with the ProgressView is still running (i.e., not aborted by user).
 * @param progress_view Pointer to the ProgressView.
 * @return True if running, false if aborted.
 */
bool progress_view_is_running(ProgressView* progress_view);

/**
 * Update the number of bytes transferred, to be displayed or used by the ProgressView.
 * @param progress_view Pointer to the ProgressView.
 * @param bytes The number of bytes transferred.
 */
void progress_view_set_bytes_transferred(ProgressView* progress_view, uint32_t bytes);

/**
 * Reset the progress view to its initial state for a new operation.
 * This typically involves resetting the start time, status text, and running state.
 * @param progress_view Pointer to the ProgressView.
 */
void progress_view_reset(ProgressView* progress_view);

/**
 * Signal the progress view (and any associated worker thread) to stop its current operation.
 * @param progress_view Pointer to the ProgressView.
 */
void progress_view_set_force_stop(ProgressView* progress_view);

