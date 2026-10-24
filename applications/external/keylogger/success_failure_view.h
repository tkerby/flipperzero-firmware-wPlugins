#pragma once

#include <gui/view.h>

// Forward declaration
typedef struct SuccessFailureView SuccessFailureView;

typedef enum {
    SuccessFailureModeSuccess, // Retained for potential future use, though current C file marks mode as unused
    SuccessFailureModeFailure,
} SuccessFailureMode; // Renamed from SuccessFailureViewMode for brevity if preferred, or keep as is.

// Callback for individual dialog buttons
typedef void (*SuccessFailureButtonCallback)(void* context);

/**
 * Allocate and initialize a new SuccessFailureView.
 * @return Pointer to the newly allocated SuccessFailureView.
 */
SuccessFailureView* success_failure_view_alloc();

/**
 * Free the SuccessFailureView.
 * @param view Pointer to the SuccessFailureView to free.
 */
void success_failure_view_free(SuccessFailureView* view);

/**
 * Get the underlying View object from the SuccessFailureView.
 * @param view Pointer to the SuccessFailureView.
 * @return Pointer to the DialogEx view object.
 */
View* success_failure_view_get_view(SuccessFailureView* view);

/**
 * Set the mode, message, and button configurations for the view.
 * @param success_failure_view Pointer to the SuccessFailureView.
 * @param mode SuccessFailureModeSuccess or SuccessFailureModeFailure (currently unused in C implementation).
 * @param message The main message text to display.
 * @param left_button_text Text for the left button (can be NULL).
 * @param left_callback Callback for the left button (can be NULL).
 * @param center_button_text Text for the center button (can be NULL).
 * @param center_callback Callback for the center button (can be NULL).
 * @param right_button_text Text for the right button (can be NULL).
 * @param right_callback Callback for the right button (can be NULL).
 * @param context The context to be passed to the callback functions.
 */
void success_failure_view_set_params(
    SuccessFailureView* success_failure_view,
    SuccessFailureMode mode,
    const char* message,
    const char* left_button_text,
    SuccessFailureButtonCallback left_callback,
    const char* center_button_text,
    SuccessFailureButtonCallback center_callback,
    const char* right_button_text,
    SuccessFailureButtonCallback right_callback,
    void* context);
