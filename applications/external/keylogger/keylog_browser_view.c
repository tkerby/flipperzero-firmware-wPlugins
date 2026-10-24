#include "keylog_browser_view.h"
#include <gui/elements.h>
#include <storage/storage.h>
#include <toolbox/path.h>

#define MAX_FILES        100
#define LINES_PER_PAGE   3
#define MAX_FILENAME_LEN 64

typedef struct {
    FuriString* directory_path;
    FuriString* file_paths[MAX_FILES];
    FuriString* file_display_names[MAX_FILES];
    uint8_t file_count;
    uint8_t selected_index;
    uint8_t current_page;
    uint8_t total_pages;
} KeylogBrowserModel;

struct KeylogBrowserView {
    View* view;
    KeylogBrowserFileSelectedCallback file_selected_callback;
    void* callback_context;
    FuriThread* scan_thread;
};

static void keylog_browser_view_draw_callback(Canvas* canvas, void* model) {
    KeylogBrowserModel* browser_model = model;
    if(!browser_model) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Header
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "Keylog Files");

    // Draw separator line under header
    canvas_draw_line(canvas, 0, 16, 128, 16);

    // Content area - show 3 files per page (moved higher)
    canvas_set_font(canvas, FontSecondary);
    uint8_t y_offset = 20;

    uint8_t start_index = browser_model->current_page * LINES_PER_PAGE;
    for(uint8_t i = 0; i < LINES_PER_PAGE && (start_index + i) < browser_model->file_count; i++) {
        uint8_t file_index = start_index + i;

        // Highlight selected file with rounded rectangle
        if(file_index == browser_model->selected_index) {
            canvas_draw_rbox(canvas, 2, y_offset - 1, 124, 9, 2);
            canvas_set_color(canvas, ColorWhite);
        }

        // Draw filename with better padding
        if(browser_model->file_display_names[file_index]) {
            canvas_draw_str(
                canvas,
                6,
                y_offset + 6,
                furi_string_get_cstr(browser_model->file_display_names[file_index]));
        }

        canvas_set_color(canvas, ColorBlack);
        y_offset += 10;
    }

    // Footer with navigation and controls (moved down)
    canvas_draw_line(canvas, 0, 52, 128, 52);

    if(browser_model->file_count > 0) {
        // Left side: OK button
        canvas_draw_str(canvas, 4, 60, "OK: View File");

        // Right side: Page indicator (only if multiple pages)
        if(browser_model->total_pages > 1) {
            FuriString* page_str = furi_string_alloc();
            furi_string_printf(
                page_str, "%d/%d", browser_model->current_page + 1, browser_model->total_pages);
            canvas_draw_str_aligned(
                canvas, 124, 60, AlignRight, AlignBottom, furi_string_get_cstr(page_str));
            furi_string_free(page_str);
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "No keylog files found");
    }
}

static bool keylog_browser_view_input_callback(InputEvent* event, void* context) {
    KeylogBrowserView* browser_view = context;
    if(!browser_view || !event) return false;

    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        with_view_model(
            browser_view->view,
            KeylogBrowserModel * model,
            {
                if(event->key == InputKeyUp) {
                    if(model->selected_index > 0) {
                        model->selected_index--;
                        // Update page if needed
                        model->current_page = model->selected_index / LINES_PER_PAGE;
                        consumed = true;
                    }
                } else if(event->key == InputKeyDown) {
                    if(model->selected_index < model->file_count - 1) {
                        model->selected_index++;
                        // Update page if needed
                        model->current_page = model->selected_index / LINES_PER_PAGE;
                        consumed = true;
                    }
                } else if(event->key == InputKeyOk) {
                    if(model->file_count > 0 && browser_view->file_selected_callback) {
                        consumed = true;
                        // Call callback outside of model lock
                    }
                } else if(event->key == InputKeyBack) {
                    consumed = false; // Let navigation handle this
                }
            },
            consumed);

        // Handle file selection outside of model lock
        if(consumed && event->key == InputKeyOk) {
            char* file_copy = NULL;
            with_view_model(
                browser_view->view,
                KeylogBrowserModel * model,
                {
                    if(model->file_count > 0 && model->file_paths[model->selected_index]) {
                        file_copy =
                            strdup(furi_string_get_cstr(model->file_paths[model->selected_index]));
                    }
                },
                false);
            if(file_copy && browser_view->file_selected_callback) {
                browser_view->file_selected_callback(browser_view->callback_context, file_copy);
            }
            free(file_copy);
        }
    }

    return consumed;
}

static void keylog_browser_join_scan_thread(KeylogBrowserView* browser_view) {
    if(browser_view->scan_thread) {
        furi_thread_join(browser_view->scan_thread);
        furi_thread_free(browser_view->scan_thread);
        browser_view->scan_thread = NULL;
    }
}

static int32_t keylog_browser_scan_thread_func(void* context) {
    KeylogBrowserView* browser_view = context;
    if(!browser_view) return -1;

    char dir_path[256] = {0};
    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        {
            if(!furi_string_empty(model->directory_path)) {
                strncpy(
                    dir_path, furi_string_get_cstr(model->directory_path), sizeof(dir_path) - 1);
            }
        },
        false);

    if(dir_path[0] == '\0') return -1;

    FuriString* scanned_paths[MAX_FILES] = {0};
    FuriString* scanned_names[MAX_FILES] = {0};
    uint8_t scanned_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* directory = storage_file_alloc(storage);

    if(storage_dir_open(directory, dir_path)) {
        FileInfo file_info;
        char name_buffer[256];

        while(storage_dir_read(directory, &file_info, name_buffer, 255) &&
              scanned_count < MAX_FILES) {
            if(strlen(name_buffer) > 0 && !(file_info.flags & FSF_DIRECTORY)) {
                scanned_paths[scanned_count] = furi_string_alloc();
                furi_string_printf(scanned_paths[scanned_count], "%s/%s", dir_path, name_buffer);

                scanned_names[scanned_count] = furi_string_alloc();
                if(file_info.size > 1024) {
                    furi_string_printf(
                        scanned_names[scanned_count],
                        "%s (%lluKB)",
                        name_buffer,
                        file_info.size / 1024);
                } else {
                    furi_string_printf(
                        scanned_names[scanned_count], "%s (%llub)", name_buffer, file_info.size);
                }
                scanned_count++;
            }
        }
        storage_dir_close(directory);
    }

    storage_file_free(directory);
    furi_record_close(RECORD_STORAGE);

    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        {
            for(uint8_t i = 0; i < model->file_count; i++) {
                if(model->file_paths[i]) {
                    furi_string_free(model->file_paths[i]);
                    model->file_paths[i] = NULL;
                }
                if(model->file_display_names[i]) {
                    furi_string_free(model->file_display_names[i]);
                    model->file_display_names[i] = NULL;
                }
            }
            for(uint8_t i = 0; i < scanned_count; i++) {
                model->file_paths[i] = scanned_paths[i];
                model->file_display_names[i] = scanned_names[i];
            }
            model->file_count = scanned_count;
            model->selected_index = 0;
            model->current_page = 0;
            model->total_pages = (scanned_count + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
            if(model->total_pages == 0) model->total_pages = 1;
        },
        true);

    return 0;
}

static void keylog_browser_view_enter_callback(void* context) {
    KeylogBrowserView* browser_view = context;
    if(!browser_view) return;

    keylog_browser_join_scan_thread(browser_view);

    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        {
            for(uint8_t i = 0; i < model->file_count; i++) {
                if(model->file_paths[i]) {
                    furi_string_free(model->file_paths[i]);
                    model->file_paths[i] = NULL;
                }
                if(model->file_display_names[i]) {
                    furi_string_free(model->file_display_names[i]);
                    model->file_display_names[i] = NULL;
                }
            }
            model->file_count = 0;
            model->selected_index = 0;
            model->current_page = 0;
            model->total_pages = 1;
        },
        true);

    browser_view->scan_thread = furi_thread_alloc_ex(
        "KeylogBrowserScan", 4096, keylog_browser_scan_thread_func, browser_view);
    if(browser_view->scan_thread) {
        furi_thread_start(browser_view->scan_thread);
    }
}

static void keylog_browser_view_exit_callback(void* context) {
    UNUSED(context);
    // Nothing needed for exit
}

KeylogBrowserView* keylog_browser_view_alloc(void) {
    KeylogBrowserView* browser_view = malloc(sizeof(KeylogBrowserView));
    if(!browser_view) return NULL;

    browser_view->view = view_alloc();
    if(!browser_view->view) {
        free(browser_view);
        return NULL;
    }

    view_set_context(browser_view->view, browser_view);
    view_allocate_model(browser_view->view, ViewModelTypeLocking, sizeof(KeylogBrowserModel));
    view_set_draw_callback(browser_view->view, keylog_browser_view_draw_callback);
    view_set_input_callback(browser_view->view, keylog_browser_view_input_callback);
    view_set_enter_callback(browser_view->view, keylog_browser_view_enter_callback);
    view_set_exit_callback(browser_view->view, keylog_browser_view_exit_callback);

    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        {
            model->directory_path = furi_string_alloc();
            model->file_count = 0;
            model->selected_index = 0;
            model->current_page = 0;
            model->total_pages = 1;
            for(uint8_t i = 0; i < MAX_FILES; i++) {
                model->file_paths[i] = NULL;
                model->file_display_names[i] = NULL;
            }
        },
        false);

    browser_view->file_selected_callback = NULL;
    browser_view->callback_context = NULL;
    browser_view->scan_thread = NULL;

    return browser_view;
}

void keylog_browser_view_free(KeylogBrowserView* browser_view) {
    if(!browser_view) return;

    keylog_browser_join_scan_thread(browser_view);

    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        {
            furi_string_free(model->directory_path);
            for(uint8_t i = 0; i < model->file_count; i++) {
                if(model->file_paths[i]) furi_string_free(model->file_paths[i]);
                if(model->file_display_names[i]) furi_string_free(model->file_display_names[i]);
            }
        },
        false);

    view_free(browser_view->view);
    free(browser_view);
}

View* keylog_browser_view_get_view(KeylogBrowserView* browser_view) {
    if(!browser_view) return NULL;
    return browser_view->view;
}

void keylog_browser_view_set_directory(KeylogBrowserView* browser_view, const char* directory_path) {
    if(!browser_view || !directory_path) return;

    with_view_model(
        browser_view->view,
        KeylogBrowserModel * model,
        { furi_string_set(model->directory_path, directory_path); },
        false);
}

void keylog_browser_view_set_file_selected_callback(
    KeylogBrowserView* browser_view,
    KeylogBrowserFileSelectedCallback callback,
    void* context) {
    if(!browser_view) return;
    browser_view->file_selected_callback = callback;
    browser_view->callback_context = context;
}
