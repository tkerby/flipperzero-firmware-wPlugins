#pragma once

#include <furi.h> // For FuriCore types
#include <gui/canvas.h> // For Canvas type (used by elements)
#include <gui/elements.h> // For elements_go_back_button
#include <gui/view.h> // For View system, and view_is_active

// Forward declaration
typedef struct HelpView HelpView;

/**
 * Allocate and initialize a new HelpView.
 * @return Pointer to the newly allocated HelpView.
 */
HelpView* help_view_alloc();

/**
 * Free the HelpView.
 * @param help_view Pointer to the HelpView to free.
 */
void help_view_free(HelpView* help_view);

/**
 * Get the underlying View object from the HelpView.
 * @param help_view Pointer to the HelpView.
 * @return Pointer to the View object.
 */
View* help_view_get_view(HelpView* help_view);

/**
 * Set the text content for the HelpView.
 * The view will handle pagination of long text.
 * @param help_view Pointer to the HelpView.
 * @param text The help text to display.
 */
void help_view_set_text(HelpView* help_view, const char* text);
