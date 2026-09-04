#include "success_failure_view.h"
#include <gui/elements.h>
#include <gui/view.h>
#include <furi.h>

#define MAX_LINE_WIDTH 20 // Reduced for better fit on screen

struct SuccessFailureView {
    View* view;
    SuccessFailureButtonCallback left_callback;
    SuccessFailureButtonCallback center_callback;
    SuccessFailureButtonCallback right_callback;
    void* callback_context;
};

typedef struct {
    SuccessFailureMode mode;
    FuriString* message_text;
    FuriString* left_button_text;
    FuriString* center_button_text;
    FuriString* right_button_text;
    
    // Pagination support for long messages
    char** wrapped_lines;
    uint8_t total_lines;
    uint8_t current_page;
    uint8_t total_pages;
    uint8_t lines_per_page;
} SuccessFailureViewModel;

// Helper function to wrap text to fit screen width
static void success_failure_view_wrap_text(const char* input_text, char*** wrapped_lines, uint8_t* line_count) {
    if(!input_text || !wrapped_lines || !line_count) {
        return;
    }
    
    *line_count = 0;
    *wrapped_lines = NULL;
    
    size_t input_len = strlen(input_text);
    if(input_len == 0) {
        return;
    }
    
    // First, convert literal \n sequences to actual newlines
    char* processed_text = malloc(input_len + 1);
    if(!processed_text) {
        return;
    }
    
    size_t processed_pos = 0;
    for(size_t i = 0; i < input_len; i++) {
        if(i < input_len - 1 && input_text[i] == '\\' && input_text[i + 1] == 'n') {
            // Convert \n to actual newline
            processed_text[processed_pos++] = '\n';
            i++; // Skip the 'n'
        } else {
            processed_text[processed_pos++] = input_text[i];
        }
    }
    processed_text[processed_pos] = '\0';
    
    // Allocate array for line pointers (estimate max lines needed)
    uint8_t max_lines = (processed_pos / MAX_LINE_WIDTH) + 10; // Extra buffer
    *wrapped_lines = malloc(max_lines * sizeof(char*));
    if(!*wrapped_lines) {
        free(processed_text);
        return;
    }
    
    size_t pos = 0;
    while(pos < processed_pos && *line_count < max_lines - 1) {
        // Find end of current line (newline or max width)
        size_t line_start = pos;
        size_t line_end = pos;
        
        // Look for explicit newline first
        const char* newline = strchr(processed_text + pos, '\n');
        if(newline && (newline - processed_text - pos) <= MAX_LINE_WIDTH) {
            line_end = newline - processed_text;
            pos = line_end + 1; // Skip the newline
        } else {
            // No newline within max width, find word boundary
            size_t max_end = pos + MAX_LINE_WIDTH;
            if(max_end >= processed_pos) {
                line_end = processed_pos;
                pos = processed_pos;
            } else {
                // Find last space within max width
                line_end = max_end;
                while(line_end > pos && processed_text[line_end] != ' ' && processed_text[line_end] != '\n') {
                    line_end--;
                }
                
                if(line_end == pos) {
                    // No space found, force break at max width
                    line_end = max_end;
                }
                
                pos = line_end;
                // Skip spaces at start of next line
                while(pos < processed_pos && processed_text[pos] == ' ') {
                    pos++;
                }
            }
        }
        
        // Create line string
        size_t line_len = line_end - line_start;
        if(line_len > 0) {
            (*wrapped_lines)[*line_count] = malloc(line_len + 1);
            if((*wrapped_lines)[*line_count]) {
                strncpy((*wrapped_lines)[*line_count], processed_text + line_start, line_len);
                (*wrapped_lines)[*line_count][line_len] = '\0';
                
                // Trim trailing spaces
                while(line_len > 0 && (*wrapped_lines)[*line_count][line_len - 1] == ' ') {
                    (*wrapped_lines)[*line_count][line_len - 1] = '\0';
                    line_len--;
                }
                
                (*line_count)++;
            }
        }
    }
    
    free(processed_text);
}

static void success_failure_view_draw_callback(Canvas* canvas, void* model) {
    SuccessFailureViewModel* sf_model = model;
    if(!sf_model) {
        return;
    }
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    // Simple header like other views
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Flipper Keylogger");
    
    // Status subheader based on mode
    canvas_set_font(canvas, FontSecondary);
    const char* status_text = (sf_model->mode == SuccessFailureModeSuccess) ? 
        "Operation Complete" : "Operation Failed";
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, status_text);
    
    // Main content area with proper pagination to prevent overflow
    canvas_set_font(canvas, FontSecondary);
    if(sf_model->wrapped_lines && sf_model->total_lines > 0) {
        uint8_t y_offset = 28; // Match help view spacing
        uint8_t lines_per_page = 3; // Show 3 lines to leave room for pagination
        
        // Calculate current page lines
        uint8_t start_line = sf_model->current_page * lines_per_page;
        uint8_t end_line = start_line + lines_per_page;
        if(end_line > sf_model->total_lines) {
            end_line = sf_model->total_lines;
        }
        
        // Draw lines for current page
        for(uint8_t i = start_line; i < end_line; i++) {
            if(sf_model->wrapped_lines[i] && strlen(sf_model->wrapped_lines[i]) > 0) {
                canvas_draw_str_aligned(canvas, 64, y_offset, AlignCenter, AlignTop, sf_model->wrapped_lines[i]);
                y_offset += 8;
            }
                }
            }
            
    // Footer with navigation AND OK button all on same line
    canvas_set_font(canvas, FontSecondary);
    
    // Calculate total pages for footer display
    uint8_t lines_per_page = 3;
    uint8_t total_pages = (sf_model->total_lines + lines_per_page - 1) / lines_per_page;
    if(total_pages == 0) total_pages = 1;
    
    if(total_pages > 1) {
        // Left: Prev button
        if(sf_model->current_page > 0) {
            canvas_draw_str(canvas, 2, 58, "< Prev");
    }
    
        // Center: OK button
    if(sf_model->center_button_text && !furi_string_empty(sf_model->center_button_text)) {
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(sf_model->center_button_text));
    }
    
        // Right: Next button
        if(sf_model->current_page < total_pages - 1) {
            canvas_draw_str_aligned(canvas, 125, 58, AlignRight, AlignBottom, "Next >");
        }
    } else {
        // Single page - just OK button in center
        if(sf_model->center_button_text && !furi_string_empty(sf_model->center_button_text)) {
            canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(sf_model->center_button_text));
        }
    }
}

static bool success_failure_view_input_callback(InputEvent* event, void* context) {
    SuccessFailureView* sf_view = context;
    if(!sf_view) {
        return false;
    }
    
    bool consumed = false;
    
    if(event->type == InputTypeShort) {
        SuccessFailureViewModel* model = view_get_model(sf_view->view);
        
        // Calculate total pages for navigation
        uint8_t lines_per_page = 3;
        uint8_t total_pages = (model->total_lines + lines_per_page - 1) / lines_per_page;
        if(total_pages == 0) total_pages = 1;
        
        // Handle pagination if multiple pages
        if(total_pages > 1) {
            if(event->key == InputKeyLeft && model->current_page > 0) {
                model->current_page--;
                view_commit_model(sf_view->view, true);
                consumed = true;
            } else if(event->key == InputKeyRight && model->current_page < total_pages - 1) {
                model->current_page++;
                view_commit_model(sf_view->view, true);
            consumed = true;
        } else if(event->key == InputKeyOk && sf_view->center_callback && 
                  model->center_button_text && !furi_string_empty(model->center_button_text)) {
            view_commit_model(sf_view->view, false);
            sf_view->center_callback(sf_view->callback_context);
            consumed = true;
            } else {
            view_commit_model(sf_view->view, false);
            }
        } else {
            // Single page - just handle OK button
            if(event->key == InputKeyOk && sf_view->center_callback && 
               model->center_button_text && !furi_string_empty(model->center_button_text)) {
                view_commit_model(sf_view->view, false);
                sf_view->center_callback(sf_view->callback_context);
            consumed = true;
        } else {
            view_commit_model(sf_view->view, false);
            }
        }
    }
    
    return consumed;
}

SuccessFailureView* success_failure_view_alloc() {
    SuccessFailureView* sf_view = malloc(sizeof(SuccessFailureView));
    if(!sf_view) {
        return NULL;
    }
    
    sf_view->view = view_alloc();
    if(!sf_view->view) {
        free(sf_view);
        return NULL;
    }
    
    view_set_context(sf_view->view, sf_view);
    view_allocate_model(sf_view->view, ViewModelTypeLocking, sizeof(SuccessFailureViewModel));
    view_set_draw_callback(sf_view->view, success_failure_view_draw_callback);
    view_set_input_callback(sf_view->view, success_failure_view_input_callback);
    
    SuccessFailureViewModel* model = view_get_model(sf_view->view);
    if(!model) {
        view_free(sf_view->view);
        free(sf_view);
        return NULL;
    }
    
    model->mode = SuccessFailureModeSuccess;
    model->message_text = furi_string_alloc();
    model->left_button_text = furi_string_alloc();
    model->center_button_text = furi_string_alloc();
    model->right_button_text = furi_string_alloc();
    
    // Simple initialization - no complex pagination
    model->wrapped_lines = NULL;
    model->total_lines = 0;
    model->current_page = 0;
    model->total_pages = 1;
    model->lines_per_page = 4; // Show 4 lines like other views
    
    if(!model->message_text || !model->left_button_text || 
       !model->center_button_text || !model->right_button_text) {
        if(model->message_text) furi_string_free(model->message_text);
        if(model->left_button_text) furi_string_free(model->left_button_text);
        if(model->center_button_text) furi_string_free(model->center_button_text);
        if(model->right_button_text) furi_string_free(model->right_button_text);
        view_free(sf_view->view);
        free(sf_view);
        return NULL;
    }
    
    view_commit_model(sf_view->view, false);
    
    sf_view->left_callback = NULL;
    sf_view->center_callback = NULL;
    sf_view->right_callback = NULL;
    sf_view->callback_context = NULL;
    
    return sf_view;
}

void success_failure_view_free(SuccessFailureView* sf_view) {
    furi_assert(sf_view);
    
    SuccessFailureViewModel* model = view_get_model(sf_view->view);
    if(model) {
        // Free wrapped lines
        if(model->wrapped_lines) {
            for(uint8_t i = 0; i < model->total_lines; i++) {
                if(model->wrapped_lines[i]) {
                    free(model->wrapped_lines[i]);
                }
            }
            free(model->wrapped_lines);
        }
        
        if(model->message_text) furi_string_free(model->message_text);
        if(model->left_button_text) furi_string_free(model->left_button_text);
        if(model->center_button_text) furi_string_free(model->center_button_text);
        if(model->right_button_text) furi_string_free(model->right_button_text);
    }
    
    view_free(sf_view->view);
    free(sf_view);
}

View* success_failure_view_get_view(SuccessFailureView* sf_view) {
    furi_assert(sf_view);
    return sf_view->view;
}

void success_failure_view_set_params(
    SuccessFailureView* sf_view,
    SuccessFailureMode mode,
    const char* message,
    const char* left_button_text,
    SuccessFailureButtonCallback left_callback,
    const char* center_button_text,
    SuccessFailureButtonCallback center_callback,
    const char* right_button_text,
    SuccessFailureButtonCallback right_callback,
    void* context) {
    furi_assert(sf_view);

    sf_view->callback_context = context;
    sf_view->left_callback = left_callback;
    sf_view->center_callback = center_callback;
    sf_view->right_callback = right_callback;

    SuccessFailureViewModel* model = view_get_model(sf_view->view);
    
    model->mode = mode;
    
    // Clean up any existing wrapped lines
    if(model->wrapped_lines) {
        for(uint8_t i = 0; i < model->total_lines; i++) {
            if(model->wrapped_lines[i]) {
                free(model->wrapped_lines[i]);
            }
        }
        free(model->wrapped_lines);
        model->wrapped_lines = NULL;
    }
    
    furi_string_set(model->message_text, message ? message : "");
    furi_string_set(model->left_button_text, left_button_text ? left_button_text : "");
    furi_string_set(model->center_button_text, center_button_text ? center_button_text : "");
    furi_string_set(model->right_button_text, right_button_text ? right_button_text : "");
    
    // Process message text for simple display
    if(message && strlen(message) > 0) {
        success_failure_view_wrap_text(message, &model->wrapped_lines, &model->total_lines);
        model->current_page = 0; // Not used anymore but keep for compatibility
    } else {
        model->total_lines = 0;
        model->current_page = 0;
    }
    
    view_commit_model(sf_view->view, true);
}

