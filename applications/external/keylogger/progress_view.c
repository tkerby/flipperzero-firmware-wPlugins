#include "progress_view.h"

// Temporarily commented out direct includes to test if progress_view.h handles them:
#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_port.h>
#include <gui/scene_manager.h>

#define TAG "ProgressView"

// Define the internal structure of ProgressView if it_s not already defined elsewhere
// This is the public, opaque type. The actual implementation detail is below.
struct ProgressView {
    View* view;
    // Any other members that the public ProgressView struct might need
};

typedef struct {
    char* status_text;
    bool is_running; // Controlled by the main app or input callback
    uint32_t start_time; // For animation or timeout logic
    uint32_t bytes_transferred; // Example data point
} ProgressViewModel;

// Draw callback for the progress view
static void progress_view_draw_callback(Canvas* canvas, void* model) {
    ProgressViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Consistent header like other views
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Flipper Keylogger");

    // Status subheader
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Executing Payload...");

    // Main content area with proper spacing
    canvas_set_font(canvas, FontSecondary);
    if(m->status_text) {
        // Split status text into lines and display with proper spacing
        const char* text = m->status_text;
        uint8_t y_offset = 28;

        // Simple line splitting by looking for \n
        const char* line_start = text;
        const char* line_end = strchr(line_start, '\n');

        while(line_start && *line_start) {
            if(line_end) {
                // Extract line
                int line_length = line_end - line_start;
                char line_buffer[128];
                if(line_length < 127) {
                    strncpy(line_buffer, line_start, line_length);
                    line_buffer[line_length] = '\0';
                    canvas_draw_str_aligned(
                        canvas, 64, y_offset, AlignCenter, AlignTop, line_buffer);
                }
                line_start = line_end + 1;
                line_end = strchr(line_start, '\n');
            } else {
                // Last line
                canvas_draw_str_aligned(canvas, 64, y_offset, AlignCenter, AlignTop, line_start);
                break;
            }
            y_offset += 8;
            if(y_offset > 50) break; // Don't overflow into footer area
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "Processing...");
    }

    // Simple progress animation in footer area
    uint32_t anim_frame = (furi_get_tick() - m->start_time) / 300 % 4;
    const char* anim_chars[] = {"|", "/", "-", "\\"};
    FuriString* anim_str = furi_string_alloc();
    furi_string_printf(anim_str, "Working %s", anim_chars[anim_frame]);
    canvas_draw_str_aligned(
        canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(anim_str));
    furi_string_free(anim_str);

    // REMOVED: Back button hint during execution to prevent user confusion
    // BadUSB operations cannot be safely interrupted
}

// Input callback for the progress view - DISABLED during execution
static bool progress_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);

    // CRITICAL: Block ALL input during execution to prevent interruption
    // BadUSB operations must complete without interruption
    // The navigation callback in the main app handles this, but we also
    // block it here for extra safety
    return true; // Consume all input events
}

ProgressView* progress_view_alloc() {
    ProgressView* progress_view = malloc(sizeof(ProgressView));
    if(!progress_view) {
        return NULL;
    }

    progress_view->view = view_alloc();
    if(!progress_view->view) {
        free(progress_view);
        return NULL;
    }

    view_set_context(progress_view->view, progress_view);
    view_allocate_model(progress_view->view, ViewModelTypeLocking, sizeof(ProgressViewModel));
    view_set_draw_callback(progress_view->view, progress_view_draw_callback);
    view_set_input_callback(progress_view->view, progress_view_input_callback);

    ProgressViewModel* model = view_get_model(progress_view->view);
    if(!model) {
        view_free(progress_view->view);
        free(progress_view);
        return NULL;
    }

    model->status_text = NULL;
    model->is_running = true;
    model->start_time = furi_get_tick();
    model->bytes_transferred = 0;
    view_commit_model(progress_view->view, false);

    return progress_view;
}

void progress_view_free(ProgressView* progress_view) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    if(model->status_text) {
        free(model->status_text);
        model->status_text = NULL;
    }
    view_commit_model(progress_view->view, false);
    view_free(progress_view->view);
    free(progress_view);
}

View* progress_view_get_view(ProgressView* progress_view) {
    furi_assert(progress_view);
    return progress_view->view;
}

void progress_view_set_status(ProgressView* progress_view, const char* status_text) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    if(model->status_text) {
        free(model->status_text);
        model->status_text = NULL;
    }
    if(status_text) {
        model->status_text = strdup(status_text);
    }
    view_commit_model(progress_view->view, true);
}

bool progress_view_is_running(ProgressView* progress_view) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    bool is_running = model->is_running;
    view_commit_model(progress_view->view, false);
    return is_running;
}

void progress_view_set_bytes_transferred(ProgressView* progress_view, uint32_t bytes) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    model->bytes_transferred = bytes;
    view_commit_model(progress_view->view, true);
}

void progress_view_reset(ProgressView* progress_view) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    if(model->status_text) {
        free(model->status_text);
        model->status_text = NULL;
    }
    model->is_running = true;
    model->start_time = furi_get_tick();
    model->bytes_transferred = 0;
    view_commit_model(progress_view->view, true);
}

void progress_view_set_force_stop(ProgressView* progress_view) {
    furi_assert(progress_view);
    ProgressViewModel* model = view_get_model(progress_view->view);
    model->is_running = false;
    view_commit_model(progress_view->view, true);
}
