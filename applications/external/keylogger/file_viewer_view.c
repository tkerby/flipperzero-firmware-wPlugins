#include "file_viewer_view.h"

#include <gui/elements.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/path.h>

#define TAG "FileViewer"

#define MAX_FILES_IN_DIR        50 // Max files to list in a directory
#define MAX_FILENAME_LEN        64 // Max length for a filename display
#define LINES_PER_PAGE_SELECTOR 10 // Lines shown in file selector/directory mode
#define LINES_PER_PAGE_CONTENT  4 // Lines shown when viewing file content
#define FILES_MAX_NAME_LEN      255 // Max length for a filename, from storage service
#define MAX_EXT_LEN             10 // Maximum length for file extension
#define CHUNK_SIZE              1024 // Increased from 256 to handle longer keylog lines
#define MAX_LINE_LENGTH         512 // Increased from 80 to handle long keystroke sequences
#define DISPLAY_CHARS_PER_LINE  20 // Max characters that fit on Flipper screen per line

typedef enum {
    FileViewerModeListDir, // Listing files in a directory
    FileViewerModeViewFile, // Viewing content of a single file
} FileViewerDisplayMode;

// Forward declaration for thread function
static int32_t file_viewer_loading_thread(void* context);

typedef struct {
    FileViewerView* viewer;
    char file_path[256];
    uint8_t target_page;
    bool completed;
    FuriThread* thread;
} FileLoadingContext;

struct FileViewerView {
    View* view;
    FileViewerFileSelectedCallback file_selected_callback;
    void* callback_context;
    FileLoadingContext* loading_context; // For background loading
};

typedef struct {
    FuriString* current_path; // Current directory path or file path being viewed
    FileViewerDisplayMode display_mode;

    // For directory listing
    FuriString* file_list[MAX_FILES_IN_DIR];
    uint8_t file_count;
    uint8_t selected_file_index;
    uint8_t list_offset; // For scrolling through more files than can be shown

    // For file content viewing
    FuriString* file_path_for_reading; // Path to file being read in chunks
    uint32_t current_file_offset; // Current byte position in file
    uint32_t total_file_size; // Total file size for pagination calculation
    uint8_t content_current_page;
    uint8_t content_total_pages;
    char* content_page_lines[LINES_PER_PAGE_SELECTOR];
    bool file_reading_error; // Flag to track if file reading failed
} FileViewerViewModel;

// --- Helper function to wrap long lines for display ---
static char** wrap_line_for_display(const char* line, uint8_t* line_count) {
    if(!line || strlen(line) == 0) {
        *line_count = 0;
        return NULL;
    }

    size_t total_len = strlen(line);
    uint8_t needed_lines = (total_len + DISPLAY_CHARS_PER_LINE - 1) / DISPLAY_CHARS_PER_LINE;
    if(needed_lines > 4) needed_lines = 4; // Limit to available display lines

    char** wrapped_lines = malloc(needed_lines * sizeof(char*));
    if(!wrapped_lines) {
        *line_count = 0;
        return NULL;
    }

    for(uint8_t i = 0; i < needed_lines; i++) {
        wrapped_lines[i] = malloc(DISPLAY_CHARS_PER_LINE + 1);
        if(!wrapped_lines[i]) {
            // Clean up on allocation failure
            for(uint8_t j = 0; j < i; j++) {
                free(wrapped_lines[j]);
            }
            free(wrapped_lines);
            *line_count = 0;
            return NULL;
        }

        size_t start_pos = i * DISPLAY_CHARS_PER_LINE;
        size_t copy_len = (start_pos + DISPLAY_CHARS_PER_LINE > total_len) ?
                              (total_len - start_pos) :
                              DISPLAY_CHARS_PER_LINE;

        strncpy(wrapped_lines[i], line + start_pos, copy_len);
        wrapped_lines[i][copy_len] = '\0';
    }

    *line_count = needed_lines;
    return wrapped_lines;
}

// --- Helper function to filter and process keylog content ---
static char* process_keylog_line(const char* raw_line) {
    if(!raw_line || strlen(raw_line) == 0) {
        return NULL;
    }

    // Look for the keylog format: "[HH:MM:SS] text" - this is our actual format
    const char* bracket_start = strchr(raw_line, '[');
    const char* bracket_end = strchr(raw_line, ']');
    if(bracket_start && bracket_end && bracket_end > bracket_start) {
        // Found timestamp in brackets - extract the content after it
        const char* content_start = bracket_end + 1;
        while(*content_start == ' ')
            content_start++; // Skip spaces after ]

        if(strlen(content_start) == 0) {
            return NULL; // Empty content
        }

        // Create output buffer - keep full content for wrapping
        char* processed = malloc(512);
        if(!processed) return NULL;

        // Copy the full content - we'll wrap it for display later
        strncpy(processed, content_start, 500);
        processed[500] = '\0';

        // Remove trailing newlines and carriage returns
        int len = strlen(processed);
        while(len > 0 && (processed[len - 1] == '\n' || processed[len - 1] == '\r')) {
            processed[--len] = '\0';
        }

        // Only return if there's actual content
        if(strlen(processed) > 0) {
            return processed;
        } else {
            free(processed);
            return NULL;
        }
    }

    // If no timestamp bracket found, this might not be a keylog line or might be header
    // Check if it contains meaningful content anyway
    if(strstr(raw_line, "FLIPPER ZERO KEYLOGGER") || strstr(raw_line, "Start Time:") ||
       strstr(raw_line, "User:") || strstr(raw_line, "Job started successfully")) {
        // This is a header or info line - show it in full
        char* processed = malloc(512);
        if(!processed) return NULL;

        strncpy(processed, raw_line, 500);
        processed[500] = '\0';

        // Remove trailing newlines
        int len = strlen(processed);
        while(len > 0 && (processed[len - 1] == '\n' || processed[len - 1] == '\r')) {
            processed[--len] = '\0';
        }

        return processed;
    }

    return NULL; // Not a recognized format
}

// --- Helper function to start background loading ---
static void
    file_viewer_start_background_loading(FileViewerView* file_viewer_view, uint8_t target_page) {
    if(!file_viewer_view) return;

    // Clean up any existing loading context
    if(file_viewer_view->loading_context) {
        if(file_viewer_view->loading_context->thread) {
            // Wait for thread to complete if still running
            furi_thread_join(file_viewer_view->loading_context->thread);
            furi_thread_free(file_viewer_view->loading_context->thread);
        }
        free(file_viewer_view->loading_context);
        file_viewer_view->loading_context = NULL;
    }

    // Get file path from view model
    char file_path_copy[256] = {0};
    bool should_load = false;

    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        {
            if(model->file_path_for_reading && !furi_string_empty(model->file_path_for_reading)) {
                strncpy(
                    file_path_copy,
                    furi_string_get_cstr(model->file_path_for_reading),
                    sizeof(file_path_copy) - 1);
                should_load = true;
            }
        },
        false);

    if(!should_load) {
        // Show error state immediately
        with_view_model(
            file_viewer_view->view,
            FileViewerViewModel * model,
            {
                for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
                    if(model->content_page_lines[i]) {
                        free(model->content_page_lines[i]);
                        model->content_page_lines[i] = NULL;
                    }
                }
                model->content_page_lines[0] = strdup("No file loaded");
                model->content_total_pages = 1;
                model->content_current_page = 0;
                model->file_reading_error = false;
            },
            true);
        return;
    }

    // Show loading message immediately (non-blocking)
    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        {
            for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
                if(model->content_page_lines[i]) {
                    free(model->content_page_lines[i]);
                    model->content_page_lines[i] = NULL;
                }
            }
            model->content_page_lines[0] = strdup("Loading...");
            model->content_page_lines[1] = strdup("Please wait...");
            model->content_total_pages = 1;
            model->content_current_page = 0;
        },
        true);

    // Create loading context and start background thread
    file_viewer_view->loading_context = malloc(sizeof(FileLoadingContext));
    if(!file_viewer_view->loading_context) {
        FURI_LOG_E(TAG, "Failed to allocate loading context");
        return;
    }

    file_viewer_view->loading_context->viewer = file_viewer_view;
    strncpy(
        file_viewer_view->loading_context->file_path,
        file_path_copy,
        sizeof(file_viewer_view->loading_context->file_path) - 1);
    file_viewer_view->loading_context->target_page = target_page;
    file_viewer_view->loading_context->completed = false;

    // Create and start background thread
    file_viewer_view->loading_context->thread = furi_thread_alloc_ex(
        "FileViewerLoader", 8192, file_viewer_loading_thread, file_viewer_view->loading_context);

    if(!file_viewer_view->loading_context->thread) {
        FURI_LOG_E(TAG, "Failed to allocate loading thread");
        free(file_viewer_view->loading_context);
        file_viewer_view->loading_context = NULL;
        return;
    }

    FURI_LOG_D(TAG, "Starting background loading thread for page %d", target_page);
    furi_thread_start(file_viewer_view->loading_context->thread);
}

// --- Helper function to populate content page buffer with CHUNKED READING ---
static void file_viewer_update_content_page_buffer(FileViewerView* file_viewer_view) {
    // BACKGROUND THREAD APPROACH: Use background loading for all content updates
    if(!file_viewer_view) return;

    FURI_LOG_D(TAG, "Content update: Using background thread for page navigation");

    // Get current page from model
    uint8_t target_page = 0;
    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        { target_page = model->content_current_page; },
        false);

    // Start background loading for the target page
    file_viewer_start_background_loading(file_viewer_view, target_page);

    FURI_LOG_D(TAG, "Content update: Background loading started for page %d", target_page);
}

// --- Draw Callback ---
static void file_viewer_view_draw_callback(Canvas* canvas, void* model) {
    FileViewerViewModel* file_model = model;
    if(!file_model) {
        return;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(file_model->display_mode == FileViewerModeListDir) {
        // Simple header like help view
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Keylog Data");

        canvas_set_font(canvas, FontSecondary);
        uint8_t y_offset = 28; // Match help_view.c positioning - content starts after header
        for(uint8_t i = 0;
            i < LINES_PER_PAGE_SELECTOR && (file_model->list_offset + i) < file_model->file_count;
            ++i) {
            uint8_t current_file_idx = file_model->list_offset + i;
            if(current_file_idx == file_model->selected_file_index) {
                canvas_draw_box(canvas, 0, y_offset - 1, 128, 8);
                canvas_set_color(canvas, ColorWhite);
            }
            if(file_model->file_list[current_file_idx]) {
                canvas_draw_str(
                    canvas,
                    4,
                    y_offset,
                    furi_string_get_cstr(file_model->file_list[current_file_idx]));
            }
            canvas_set_color(canvas, ColorBlack);
            y_offset += 8; // Match help_view.c spacing
        }

        // Footer navigation at proper Y position like help_view.c
        if(file_model->file_count > 1) {
            FuriString* status_str = furi_string_alloc();
            furi_string_printf(
                status_str, "%d/%d", file_model->selected_file_index + 1, file_model->file_count);
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(status_str));
            furi_string_free(status_str);

            // Show scroll indicators at footer level
            if(file_model->list_offset > 0) {
                canvas_draw_str(canvas, 2, 58, "↑");
            }
            if(file_model->list_offset + LINES_PER_PAGE_SELECTOR < file_model->file_count) {
                canvas_draw_str_aligned(canvas, 125, 58, AlignRight, AlignBottom, "↓");
            }
        }

    } else if(file_model->display_mode == FileViewerModeViewFile) {
        // Simple header with filename extracted from the actual file path
        FuriString* temp_filename_str = furi_string_alloc();
        if(file_model->file_path_for_reading &&
           !furi_string_empty(file_model->file_path_for_reading)) {
            path_extract_filename(file_model->file_path_for_reading, temp_filename_str, true);
        } else {
            furi_string_set_str(temp_filename_str, "File Content");
        }
        const char* filename_display = furi_string_get_cstr(temp_filename_str);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, filename_display);
        furi_string_free(temp_filename_str);

        // Content with proper spacing like help_view.c - between header and footer
        canvas_set_font(canvas, FontSecondary);
        uint8_t y_offset = 28; // Match help_view.c - content starts after header space
        for(uint8_t i = 0; i < LINES_PER_PAGE_CONTENT; ++i) {
            if(file_model->content_page_lines[i]) {
                canvas_draw_str(
                    canvas,
                    4,
                    y_offset,
                    file_model->content_page_lines[i]); // Match help_view.c indent
                y_offset += 8; // Match help_view.c spacing
            } else
                break;
        }

        // Footer navigation at proper Y position like help_view.c
        if(file_model->content_total_pages > 1) {
            FuriString* page_str = furi_string_alloc();
            furi_string_printf(
                page_str,
                "Page %d/%d",
                file_model->content_current_page + 1,
                file_model->content_total_pages);
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, furi_string_get_cstr(page_str));
            furi_string_free(page_str);

            // Navigation hints at footer level
            if(file_model->content_current_page > 0) {
                canvas_draw_str(canvas, 2, 58, "←Prev");
            }
            if(file_model->content_current_page < file_model->content_total_pages - 1) {
                canvas_draw_str_aligned(canvas, 125, 58, AlignRight, AlignBottom, "Next→");
            }
        } else {
            // Show helpful hint when there's only one page at footer level
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, "Use ← → to navigate");
        }
    }
}

// --- Input Callback ---
static bool file_viewer_view_input_callback(InputEvent* event, void* context) {
    FileViewerView* file_viewer_view = context;
    if(!file_viewer_view) {
        return false;
    }

    bool consumed = false;
    bool needs_file_update = false;
    bool needs_callback_call = false;
    char callback_file_path[256] = {0};

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        // CRITICAL FIX: Separate model updates from file operations completely
        with_view_model(
            file_viewer_view->view,
            FileViewerViewModel * model,
            {
                // Handle Back button for both modes
                if(event->key == InputKeyBack) {
                    consumed = true; // Always consume Back button
                    // Back button will be handled by navigation callback, not here
                } else if(model->display_mode == FileViewerModeListDir) {
                    if(event->key == InputKeyUp) {
                        if(model->selected_file_index > 0) model->selected_file_index--;
                        if(model->selected_file_index < model->list_offset)
                            model->list_offset = model->selected_file_index;
                        consumed = true;
                    } else if(event->key == InputKeyDown) {
                        if(model->selected_file_index < model->file_count - 1)
                            model->selected_file_index++;
                        if(model->selected_file_index >=
                           model->list_offset + LINES_PER_PAGE_SELECTOR)
                            model->list_offset =
                                model->selected_file_index - LINES_PER_PAGE_SELECTOR + 1;
                        consumed = true;
                    } else if(event->key == InputKeyOk) {
                        if(model->file_count > 0 && file_viewer_view->file_selected_callback &&
                           model->file_list[model->selected_file_index]) {
                            // Prepare callback data without doing file operations
                            FuriString* selected_file_path_str = furi_string_alloc();
                            path_concat(
                                furi_string_get_cstr(model->current_path),
                                furi_string_get_cstr(model->file_list[model->selected_file_index]),
                                selected_file_path_str);

                            strncpy(
                                callback_file_path,
                                furi_string_get_cstr(selected_file_path_str),
                                sizeof(callback_file_path) - 1);
                            furi_string_free(selected_file_path_str);

                            needs_callback_call = true;
                            consumed = true;
                        }
                    }
                } else if(model->display_mode == FileViewerModeViewFile) {
                    if(event->key == InputKeyLeft || event->key == InputKeyUp) {
                        if(model->content_current_page > 0) {
                            model->content_current_page--;
                            needs_file_update = true;
                            consumed = true;
                        }
                    } else if(
                        event->key == InputKeyRight || event->key == InputKeyDown ||
                        event->key == InputKeyOk) {
                        if(model->content_current_page < model->content_total_pages - 1) {
                            model->content_current_page++;
                            needs_file_update = true;
                            consumed = true;
                        }
                    }
                }
            },
            consumed); // Only commit if something changed

        // Handle file operations OUTSIDE of any view model locks
        if(needs_callback_call && callback_file_path[0] != '\0') {
            file_viewer_view->file_selected_callback(
                file_viewer_view->callback_context, callback_file_path);
        }

        if(needs_file_update) {
            // Update file content outside of all model locks to prevent ViewPort lockups
            file_viewer_update_content_page_buffer(file_viewer_view);
        }
    }

    return consumed;
}

// --- Enter/Exit Callbacks ---
static void file_viewer_view_enter_callback(void* context) {
    FileViewerView* file_viewer_view = context;
    if(!file_viewer_view) {
        return;
    }

    // MOMENTUM FIRMWARE FIX: Do absolutely NOTHING in enter callback
    // Let the app's custom event system handle everything
    FURI_LOG_D(TAG, "Enter callback: Passive mode - no operations performed");
}

static void file_viewer_view_exit_callback(void* context) {
    FileViewerView* file_viewer_view = context;
    if(!file_viewer_view) {
        return;
    }

    // CRITICAL FIX: Use WITH_VIEW_MODEL pattern to avoid immediate exit
    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        {
            for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
                if(model->content_page_lines[i]) {
                    free(model->content_page_lines[i]);
                    model->content_page_lines[i] = NULL;
                }
            }
        },
        true); // Commit changes
}

// --- Background Loading Thread ---
static int32_t file_viewer_loading_thread(void* context) {
    FileLoadingContext* load_ctx = context;
    if(!load_ctx || !load_ctx->viewer) {
        return -1;
    }

    FURI_LOG_D(TAG, "Background loading thread started for: %s", load_ctx->file_path);

    // Do ALL file I/O operations in background thread
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    char* page_lines[LINES_PER_PAGE_CONTENT] = {0}; // Temporary storage
    uint32_t total_file_size = 0;
    uint8_t total_pages = 1;
    bool reading_error = false;

    if(!storage_file_open(file, load_ctx->file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        reading_error = true;
    } else {
        total_file_size = storage_file_size(file);

        // Ultra-conservative page estimation
        uint32_t bytes_per_page = 200;
        uint32_t calculated_pages = (total_file_size + bytes_per_page - 1) / bytes_per_page;
        if(calculated_pages == 0) calculated_pages = 1;
        if(calculated_pages > 255) calculated_pages = 255;
        total_pages = (uint8_t)calculated_pages;

        // Ensure target page is valid
        if(load_ctx->target_page >= total_pages) {
            load_ctx->target_page = total_pages - 1;
        }

        // Calculate offset for target page
        uint32_t target_offset = load_ctx->target_page * bytes_per_page;

        if(storage_file_seek(file, target_offset, true)) {
            // Skip to line boundary if not at beginning
            if(target_offset > 0) {
                char byte;
                uint32_t skip_count = 0;
                while(skip_count < 100 && storage_file_read(file, &byte, 1) == 1 && byte != '\n') {
                    skip_count++;
                }
            }

            // Heap-allocate large buffers to avoid stack overflow on this thread
            char* large_chunk = malloc(1025);
            char* raw_line = malloc(600);
            if(!large_chunk || !raw_line) {
                free(large_chunk);
                free(raw_line);
                reading_error = true;
            } else {
                uint16_t bytes_read = storage_file_read(file, large_chunk, 1024);
                large_chunk[bytes_read] = '\0';

                char* line_start = large_chunk;
                uint8_t lines_added = 0;

                while(line_start < large_chunk + bytes_read &&
                      lines_added < LINES_PER_PAGE_CONTENT) {
                    char* line_end = strchr(line_start, '\n');
                    memset(raw_line, 0, 600);
                    size_t line_length;

                    if(line_end && line_end < large_chunk + bytes_read) {
                        line_length = line_end - line_start;
                        if(line_length > 590) line_length = 590;
                        strncpy(raw_line, line_start, line_length);
                        raw_line[line_length] = '\0';
                        line_start = line_end + 1;
                    } else {
                        line_length = strlen(line_start);
                        if(line_length == 0) break;
                        if(line_length > 590) line_length = 590;
                        strncpy(raw_line, line_start, line_length);
                        raw_line[line_length] = '\0';
                        line_start += line_length;
                    }

                    char* processed_line = process_keylog_line(raw_line);
                    if(processed_line) {
                        uint8_t wrapped_count = 0;
                        char** wrapped_lines =
                            wrap_line_for_display(processed_line, &wrapped_count);

                        if(wrapped_lines && wrapped_count > 0) {
                            for(uint8_t w = 0;
                                w < wrapped_count && lines_added < LINES_PER_PAGE_CONTENT;
                                w++) {
                                page_lines[lines_added] = strdup(wrapped_lines[w]);
                                lines_added++;
                            }
                            for(uint8_t w = 0; w < wrapped_count; w++) {
                                free(wrapped_lines[w]);
                            }
                            free(wrapped_lines);
                        } else {
                            if(lines_added < LINES_PER_PAGE_CONTENT) {
                                page_lines[lines_added] = strdup(processed_line);
                                lines_added++;
                            }
                        }

                        free(processed_line);
                    }
                }

                if(lines_added == 0) {
                    page_lines[0] = strdup("No keystrokes found");
                    if(load_ctx->target_page == 0) {
                        page_lines[1] = strdup("This may be empty");
                        page_lines[2] = strdup("or timestamp-only");
                        page_lines[3] = strdup("Try other pages →");
                    }
                }
                free(large_chunk);
                free(raw_line);
            }
        } else {
            reading_error = true;
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    // VERY IMPORTANT: Update model from background thread using with_view_model
    // This is safe because with_view_model handles thread synchronization
    with_view_model(
        load_ctx->viewer->view,
        FileViewerViewModel * model,
        {
            // Clear existing lines first
            for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
                if(model->content_page_lines[i]) {
                    free(model->content_page_lines[i]);
                    model->content_page_lines[i] = NULL;
                }
            }

            if(reading_error) {
                model->content_page_lines[0] = strdup("Error reading file");
                model->content_total_pages = 1;
                model->content_current_page = 0;
                model->file_reading_error = true;
            } else {
                // Transfer ownership of pre-processed lines
                for(uint8_t i = 0; i < LINES_PER_PAGE_CONTENT; i++) {
                    model->content_page_lines[i] = page_lines[i];
                }
                model->total_file_size = total_file_size;
                model->content_total_pages = total_pages;
                model->content_current_page = load_ctx->target_page;
                model->file_reading_error = false;
            }
        },
        true); // Commit changes

    // Mark as completed
    load_ctx->completed = true;

    FURI_LOG_D(TAG, "Background loading thread completed");
    return 0;
}

// --- Public API Functions ---
FileViewerView* file_viewer_view_alloc() {
    FileViewerView* file_viewer_view = malloc(sizeof(FileViewerView));
    if(!file_viewer_view) {
        return NULL;
    }

    file_viewer_view->view = view_alloc();
    if(!file_viewer_view->view) {
        free(file_viewer_view);
        return NULL;
    }

    view_set_context(file_viewer_view->view, file_viewer_view);
    view_allocate_model(file_viewer_view->view, ViewModelTypeLocking, sizeof(FileViewerViewModel));
    view_set_draw_callback(file_viewer_view->view, file_viewer_view_draw_callback);
    view_set_input_callback(file_viewer_view->view, file_viewer_view_input_callback);
    view_set_enter_callback(file_viewer_view->view, file_viewer_view_enter_callback);
    view_set_exit_callback(file_viewer_view->view, file_viewer_view_exit_callback);

    FileViewerViewModel* model = view_get_model(file_viewer_view->view);
    if(!model) {
        view_free(file_viewer_view->view);
        free(file_viewer_view);
        return NULL;
    }

    model->current_path = furi_string_alloc();
    if(!model->current_path) {
        view_free(file_viewer_view->view);
        free(file_viewer_view);
        return NULL;
    }

    model->file_path_for_reading = furi_string_alloc();
    if(!model->file_path_for_reading) {
        furi_string_free(model->current_path);
        view_free(file_viewer_view->view);
        free(file_viewer_view);
        return NULL;
    }

    model->display_mode = FileViewerModeListDir;
    model->file_count = 0;
    model->selected_file_index = 0;
    model->list_offset = 0;
    model->content_current_page = 0;
    model->content_total_pages = 1;
    for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i)
        model->content_page_lines[i] = NULL;
    for(uint8_t i = 0; i < MAX_FILES_IN_DIR; ++i)
        model->file_list[i] = NULL;

    model->current_file_offset = 0;
    model->total_file_size = 0;
    model->file_reading_error = false;

    file_viewer_view->file_selected_callback = NULL;
    file_viewer_view->callback_context = NULL;
    file_viewer_view->loading_context = NULL; // Initialize loading context

    view_commit_model(file_viewer_view->view, false);

    return file_viewer_view;
}

void file_viewer_view_free(FileViewerView* file_viewer_view) {
    furi_assert(file_viewer_view);

    // Clean up background loading thread first
    if(file_viewer_view->loading_context) {
        if(file_viewer_view->loading_context->thread) {
            // Wait for thread to complete
            furi_thread_join(file_viewer_view->loading_context->thread);
            furi_thread_free(file_viewer_view->loading_context->thread);
        }
        free(file_viewer_view->loading_context);
        file_viewer_view->loading_context = NULL;
    }

    FileViewerViewModel* model = view_get_model(file_viewer_view->view);
    if(model) {
        for(uint8_t i = 0; i < model->file_count; ++i) {
            if(model->file_list[i]) {
                furi_string_free(model->file_list[i]);
            }
        }
        for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
            if(model->content_page_lines[i]) {
                free(model->content_page_lines[i]);
            }
        }
        if(model->current_path) {
            furi_string_free(model->current_path);
        }
        if(model->file_path_for_reading) {
            furi_string_free(model->file_path_for_reading);
        }
    }
    view_commit_model(file_viewer_view->view, false);

    view_free(file_viewer_view->view);
    free(file_viewer_view);
}

View* file_viewer_view_get_view(FileViewerView* file_viewer_view) {
    furi_assert(file_viewer_view);
    return file_viewer_view->view;
}

void file_viewer_view_set_directory_path(FileViewerView* file_viewer_view, const char* path) {
    furi_assert(file_viewer_view);

    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        {
            furi_string_set(model->current_path, path);
            model->display_mode = FileViewerModeListDir;
        },
        true);
    // Population will happen on enter
}

void file_viewer_view_set_file_content(
    FileViewerView* file_viewer_view,
    const char* file_path,
    const char* file_name) {
    furi_assert(file_viewer_view);
    UNUSED(file_name); // Mark unused parameter

    // Do some heavy operations outside the model lock
    FuriString* parent_dir = furi_string_alloc();
    path_extract_dirname(file_path, parent_dir);

    // Keep model operations safe and simple
    with_view_model(
        file_viewer_view->view,
        FileViewerViewModel * model,
        {
            // Store file path for chunked reading
            furi_string_set(model->file_path_for_reading, file_path);

            // Set parent directory path (already computed outside lock)
            furi_string_set(model->current_path, parent_dir);

            // Set to file viewing mode
            model->display_mode = FileViewerModeViewFile;
            model->content_current_page = 0;
            model->current_file_offset = 0;
            model->file_reading_error = false;

            // Safe content line setup - don't transfer ownership
            for(uint8_t i = 0; i < LINES_PER_PAGE_SELECTOR; ++i) {
                if(model->content_page_lines[i]) {
                    free(model->content_page_lines[i]);
                    model->content_page_lines[i] = NULL;
                }
            }
            // Safe allocation inside model lock
            model->content_page_lines[0] = strdup("Loading...");
        },
        true); // Safe commit inside with_view_model

    // Clean up temporary allocation
    furi_string_free(parent_dir);

    // Now it's safe to start background loading since we're in custom event handler context
    FURI_LOG_D(TAG, "Starting background loading from safe context");
    file_viewer_start_background_loading(file_viewer_view, 0);
}

void file_viewer_view_set_file_selected_callback(
    FileViewerView* file_viewer_view,
    FileViewerFileSelectedCallback callback,
    void* context) {
    furi_assert(file_viewer_view);
    file_viewer_view->file_selected_callback = callback;
    file_viewer_view->callback_context = context;
}
