#include "help_view.h"

// Temporarily commented out direct includes:
#include <gui/elements.h>
#include <gui/view.h>
#include <furi.h>

#define HELP_TEXT_MAX_PAGES 15
#define LINES_PER_PAGE      3 // Increased from 3 to show more content per page
#define MAX_LINE_WIDTH      30 // Increased for better text flow

struct HelpView {
    View* view;
};

typedef struct {
    FuriString* text_content;
    uint8_t current_page;
    uint8_t total_pages;
    char* page_buffer[LINES_PER_PAGE]; // To store lines for the current page
} HelpViewModel;

static void help_view_draw_callback(Canvas* canvas, void* model) {
    HelpViewModel* help_model = model;
    if(!help_model) {
        return;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Simple header like file viewer
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Flipper Keylogger");

    // Version info
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 16, AlignCenter, AlignCenter, "v0.8 - DuckyScript Edition");

    // Content with proper spacing
    canvas_set_font(canvas, FontSecondary);
    uint8_t y_offset = 28;
    for(uint8_t i = 0; i < LINES_PER_PAGE; ++i) {
        if(help_model->page_buffer[i] != NULL) {
            // Check if line starts with special formatting indicators
            const char* line = help_model->page_buffer[i];

            // Handle section headers (lines starting with specific Unicode or symbols)
            if(strstr(line, "🎯") || strstr(line, "📁") || strstr(line, "🔧") ||
               strstr(line, "💾") || strstr(line, "🛠️") || strstr(line, "⚡")) {
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str(canvas, 4, y_offset, line);
                canvas_set_font(canvas, FontSecondary);
            }
            // Handle bullet points and regular content
            else if(strstr(line, "•") || strstr(line, "·")) {
                canvas_draw_str(canvas, 6, y_offset, line);
            }
            // Handle numbered lists
            else if((line[0] >= '1' && line[0] <= '9') && line[1] == '.') {
                canvas_draw_str(canvas, 6, y_offset, line);
            }
            // Regular content with slight indent
            else {
                canvas_draw_str(canvas, 4, y_offset, line);
            }
            y_offset += 8;
        } else {
            break;
        }
    }

    // Simple footer navigation
    if(help_model->total_pages > 1) {
        FuriString* page_str = furi_string_alloc();
        furi_string_printf(
            page_str, "%d/%d", help_model->current_page + 1, help_model->total_pages);
        canvas_draw_str_aligned(
            canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(page_str));
        furi_string_free(page_str);
    }

    // Navigation hints
    if(help_model->current_page > 0) {
        canvas_draw_str(canvas, 2, 58, "< Prev");
    }
    if(help_model->current_page < help_model->total_pages - 1) {
        canvas_draw_str_aligned(canvas, 95, 58, AlignLeft, AlignBottom, "Next >");
    }
}

// Helper function to wrap text to fit screen width
static void
    help_view_wrap_text(const char* input_text, char*** wrapped_lines, uint8_t* line_count) {
    if(!input_text || !wrapped_lines || !line_count) {
        return;
    }

    *line_count = 0;
    *wrapped_lines = NULL;

    size_t input_len = strlen(input_text);
    if(input_len == 0) {
        return;
    }

    // Allocate array for line pointers (estimate max lines needed)
    uint8_t max_lines = (input_len / MAX_LINE_WIDTH) + 10; // Extra buffer
    *wrapped_lines = malloc(max_lines * sizeof(char*));
    if(!*wrapped_lines) {
        return;
    }

    size_t pos = 0;
    while(pos < input_len && *line_count < max_lines - 1) {
        // Find end of current line (newline or max width)
        size_t line_start = pos;
        size_t line_end = pos;

        // Look for explicit newline first
        const char* newline = strchr(input_text + pos, '\n');
        if(newline && (newline - input_text - pos) <= MAX_LINE_WIDTH) {
            line_end = newline - input_text;
            pos = line_end + 1; // Skip the newline
        } else {
            // No newline within max width, find word boundary
            size_t max_end = pos + MAX_LINE_WIDTH;
            if(max_end >= input_len) {
                line_end = input_len;
                pos = input_len;
            } else {
                // Find last space within max width
                line_end = max_end;
                while(line_end > pos && input_text[line_end] != ' ' &&
                      input_text[line_end] != '\n') {
                    line_end--;
                }

                if(line_end == pos) {
                    // No space found, force break at max width
                    line_end = max_end;
                }

                pos = line_end;
                // Skip spaces at start of next line
                while(pos < input_len && input_text[pos] == ' ') {
                    pos++;
                }
            }
        }

        // Create line string
        size_t line_len = line_end - line_start;
        (*wrapped_lines)[*line_count] = malloc(line_len + 1);
        if((*wrapped_lines)[*line_count]) {
            strncpy((*wrapped_lines)[*line_count], input_text + line_start, line_len);
            (*wrapped_lines)[*line_count][line_len] = '\0';
            (*line_count)++;
        }
    }
}

static void help_view_update_page_buffer(HelpView* help_view) {
    furi_assert(help_view);
    HelpViewModel* help_model = view_get_model(help_view->view);
    if(!help_model) {
        return;
    }

    // Clear previous page buffer
    for(uint8_t i = 0; i < LINES_PER_PAGE; ++i) {
        if(help_model->page_buffer[i]) {
            free(help_model->page_buffer[i]);
            help_model->page_buffer[i] = NULL;
        }
    }

    if(!help_model->text_content || furi_string_empty(help_model->text_content)) {
        help_model->total_pages = 1;
        help_model->current_page = 0;
        help_model->page_buffer[0] = strdup("No help text.");
        if(!help_model->page_buffer[0]) {
            help_model->page_buffer[0] = NULL;
        }
        view_commit_model(help_view->view, true);
        return;
    }

    const char* full_text = furi_string_get_cstr(help_model->text_content);

    // Use word wrapping to get all lines
    char** wrapped_lines = NULL;
    uint8_t total_lines = 0;
    help_view_wrap_text(full_text, &wrapped_lines, &total_lines);

    if(!wrapped_lines || total_lines == 0) {
        help_model->total_pages = 1;
        help_model->current_page = 0;
        help_model->page_buffer[0] = strdup("Text wrapping failed.");
        view_commit_model(help_view->view, true);
        return;
    }

    // Calculate total pages
    help_model->total_pages = (total_lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
    if(help_model->total_pages == 0) help_model->total_pages = 1;
    if(help_model->current_page >= help_model->total_pages) {
        help_model->current_page = help_model->total_pages - 1;
    }

    // Fill current page buffer
    uint8_t start_line = help_model->current_page * LINES_PER_PAGE;
    for(uint8_t i = 0; i < LINES_PER_PAGE; i++) {
        uint8_t line_idx = start_line + i;
        if(line_idx < total_lines) {
            help_model->page_buffer[i] = strdup(wrapped_lines[line_idx]);
            if(!help_model->page_buffer[i]) {
                help_model->page_buffer[i] = strdup("(Memory error)");
            }
        } else {
            help_model->page_buffer[i] = NULL;
        }
    }

    // Free wrapped lines
    for(uint8_t i = 0; i < total_lines; i++) {
        if(wrapped_lines[i]) {
            free(wrapped_lines[i]);
        }
    }
    free(wrapped_lines);

    view_commit_model(help_view->view, true);
}

static bool help_view_input_callback(InputEvent* event, void* context) {
    HelpView* help_view = context;
    if(!help_view) {
        return false;
    }

    if(!event) {
        return false;
    }

    HelpViewModel* help_model = view_get_model(help_view->view);
    if(!help_model) {
        return false;
    }

    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyBack) {
            // Don't consume Back button - let navigation callback handle it
            view_commit_model(help_view->view, false);
            return false;
        } else if(event->key == InputKeyLeft) {
            if(help_model->current_page > 0) {
                help_model->current_page--;
                view_commit_model(help_view->view, false);
                help_view_update_page_buffer(help_view);
                consumed = true;
            }
        } else if(event->key == InputKeyRight || event->key == InputKeyOk) {
            if(help_model->current_page < help_model->total_pages - 1) {
                help_model->current_page++;
                view_commit_model(help_view->view, false);
                help_view_update_page_buffer(help_view);
                consumed = true;
            }
        } else if(event->key == InputKeyDown) {
            if(help_model->current_page < help_model->total_pages - 1) {
                help_model->current_page++;
                view_commit_model(help_view->view, false);
                help_view_update_page_buffer(help_view);
                consumed = true;
            }
        } else if(event->key == InputKeyUp) {
            if(help_model->current_page > 0) {
                help_model->current_page--;
                view_commit_model(help_view->view, false);
                help_view_update_page_buffer(help_view);
                consumed = true;
            }
        }
    }

    if(!consumed) {
        view_commit_model(help_view->view, false);
    }

    return consumed;
}

static void help_view_enter_callback(void* context) {
    HelpView* help_view = context;
    if(!help_view) {
        return;
    }
    help_view_update_page_buffer(help_view);
}

static void help_view_exit_callback(void* context) {
    HelpView* help_view = context;
    if(!help_view) {
        return;
    }

    HelpViewModel* help_model = view_get_model(help_view->view);
    if(!help_model) {
        return;
    }

    for(uint8_t i = 0; i < LINES_PER_PAGE; ++i) {
        if(help_model->page_buffer[i]) {
            free(help_model->page_buffer[i]);
            help_model->page_buffer[i] = NULL;
        }
    }
    view_commit_model(help_view->view, true);
}

HelpView* help_view_alloc() {
    HelpView* help_view = malloc(sizeof(HelpView));
    if(!help_view) {
        return NULL;
    }

    help_view->view = view_alloc();
    if(!help_view->view) {
        free(help_view);
        return NULL;
    }

    view_set_context(help_view->view, help_view);
    view_allocate_model(help_view->view, ViewModelTypeLocking, sizeof(HelpViewModel));
    view_set_draw_callback(help_view->view, help_view_draw_callback);
    view_set_input_callback(help_view->view, help_view_input_callback);
    view_set_enter_callback(help_view->view, help_view_enter_callback);
    view_set_exit_callback(help_view->view, help_view_exit_callback);

    HelpViewModel* help_model = view_get_model(help_view->view);
    if(!help_model) {
        view_free(help_view->view);
        free(help_view);
        return NULL;
    }

    help_model->text_content = furi_string_alloc();
    if(!help_model->text_content) {
        view_free(help_view->view);
        free(help_view);
        return NULL;
    }

    help_model->current_page = 0;
    help_model->total_pages = 1;
    for(uint8_t i = 0; i < LINES_PER_PAGE; ++i) {
        help_model->page_buffer[i] = NULL;
    }
    view_commit_model(help_view->view, false);

    return help_view;
}

void help_view_free(HelpView* help_view) {
    furi_assert(help_view);

    HelpViewModel* help_model = view_get_model(help_view->view);
    if(help_model) {
        for(uint8_t i = 0; i < LINES_PER_PAGE; ++i) {
            if(help_model->page_buffer[i]) {
                free(help_model->page_buffer[i]);
            }
        }
        if(help_model->text_content) {
            furi_string_free(help_model->text_content);
        }
    }

    view_free(help_view->view);
    free(help_view);
}

View* help_view_get_view(HelpView* help_view) {
    furi_assert(help_view);
    return help_view->view;
}

void help_view_set_text(HelpView* help_view, const char* text) {
    furi_assert(help_view);

    HelpViewModel* help_model = view_get_model(help_view->view);
    if(!help_model) {
        return;
    }

    furi_string_set(help_model->text_content, text);
    help_model->current_page = 0;
    view_commit_model(help_view->view, false);
    help_view_update_page_buffer(help_view);
}
