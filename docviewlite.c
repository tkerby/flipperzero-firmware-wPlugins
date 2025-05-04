#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <flipper_format/flipper_format.h>
#include "docviewlite_icons.h"

#define TAG "docviewlite"

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

// Maximum file content size - adjust based on available memory
#define MAX_FILE_SIZE   8192
#define MAX_LINES       100
#define MAX_LINE_LENGTH 128

// Our application menu items
typedef enum {
    DocViewSubmenuIndexBrowse,
    DocViewSubmenuIndexSettings,
    DocViewSubmenuIndexAbout,
} DocViewSubmenuIndex;

// Each view is a screen we show the user
typedef enum {
    DocViewViewSubmenu, // The main menu
    DocViewViewFileBrowser, // File browser screen
    DocViewViewTextViewer, // Document viewing screen
    DocViewViewSettings, // Settings screen
    DocViewViewAbout, // About screen
    DocViewViewTextInput, // For search or other text input
} DocViewView;

// Custom events
typedef enum {
    DocViewEventIdRedraw = 0,
    DocViewEventIdLoadFile,
    DocViewEventIdNextPage,
    DocViewEventIdPrevPage,
} DocViewEventId;

// Settings for the document viewer
typedef struct {
    uint8_t font_size; // Font size setting (small, medium, large)
    bool word_wrap; // Word wrap enabled or not
    uint8_t scroll_speed; // Auto-scroll speed if implemented
} DocViewSettings;

// Model for document content
typedef struct {
    FuriString* file_path; // Current file path
    FuriString* content; // File content as string
    size_t content_length; // Length of content
    uint16_t current_line; // Current line being viewed
    uint16_t total_lines; // Total lines in document
    uint16_t lines_per_screen; // How many lines fit on screen
    uint16_t line_offsets[MAX_LINES]; // Offset of each line in content
    DocViewSettings settings; // Viewer settings
} DocViewModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    Widget* widget_about;
    Widget* text_viewer;
    VariableItemList* settings_menu;
    DialogsApp* dialogs;
    Storage* storage;
    FuriString* file_path;
    char* file_buffer;

    DocViewModel* doc_model; // Document content and state
    FuriTimer* timer; // For any timed operations
} DocViewApp;

/**
 * Navigation callbacks
 */
static uint32_t docview_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

static uint32_t docview_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return DocViewViewSubmenu;
}

/**
 * Submenu callbacks
 */
static void docview_submenu_callback(void* context, uint32_t index) {
    DocViewApp* app = (DocViewApp*)context;
    switch(index) {
    case DocViewSubmenuIndexBrowse:
        // Open file browser dialog
        {
            FuriString* selected_file = furi_string_alloc();
            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(&browser_options, ".txt", NULL);

            bool result = dialog_file_browser_show(
                app->dialogs, app->file_path, app->file_path, &browser_options);

            if(result) {
                // Load selected file
                app->doc_model->file_path = furi_string_duplicate(app->file_path);

                // Create text viewer to display file
                view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewTextViewer);
                view_dispatcher_send_custom_event(app->view_dispatcher, DocViewEventIdLoadFile);
            }

            furi_string_free(selected_file);
        }
        break;
    case DocViewSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewSettings);
        break;
    case DocViewSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewAbout);
        break;
    default:
        break;
    }
}

/**
 * Load and parse a text file
 */
static bool docview_load_file(DocViewApp* app) {
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* content = furi_string_alloc();

    File* file = storage_file_alloc(storage);
    if(storage_file_open(
           file, furi_string_get_cstr(app->doc_model->file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Get file size
        uint64_t file_size = storage_file_size(file);
        if(file_size > MAX_FILE_SIZE) {
            file_size = MAX_FILE_SIZE; // Limit file size
        }

        if(app->file_buffer) {
            free(app->file_buffer);
        }
        app->file_buffer = malloc(file_size + 1);

        if(app->file_buffer) {
            // Read file content
            uint16_t bytes_read = storage_file_read(file, app->file_buffer, file_size);
            app->file_buffer[bytes_read] = '\0';

            furi_string_set(app->doc_model->content, app->file_buffer);
            app->doc_model->content_length = bytes_read;

            // Parse line positions
            uint16_t line_count = 0;
            app->doc_model->line_offsets[line_count++] = 0;

            for(uint16_t i = 0; i < bytes_read && line_count < MAX_LINES; i++) {
                if(app->file_buffer[i] == '\n') {
                    app->doc_model->line_offsets[line_count++] = i + 1;
                }
            }

            app->doc_model->total_lines = line_count;
            app->doc_model->current_line = 0;

            // Default to about 8 lines per screen, will be adjusted based on font size
            app->doc_model->lines_per_screen = 8;

            success = true;
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(content);

    return success;
}

/**
 * Draw document text on screen
 */
static void docview_text_viewer_draw_callback(Canvas* canvas, void* model) {
    DocViewModel* doc_model = (DocViewModel*)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Header with file name and position indicator
    canvas_set_font(canvas, FontPrimary);
    FuriString* header = furi_string_alloc();

    // Extract filename from path
    const char* path = furi_string_get_cstr(doc_model->file_path);
    const char* filename = strrchr(path, '/');
    if(filename) {
        filename++; // Skip the slash
    } else {
        filename = path;
    }

    // Show filename and position in file
    furi_string_printf(
        header,
        "%s (%d/%d)",
        filename,
        doc_model->current_line / doc_model->lines_per_screen + 1,
        (doc_model->total_lines + doc_model->lines_per_screen - 1) / doc_model->lines_per_screen);

    canvas_draw_str(canvas, 2, 10, furi_string_get_cstr(header));
    furi_string_free(header);

    // Draw separator line
    canvas_draw_line(canvas, 0, 13, 128, 13);

    // Display text content
    canvas_set_font(canvas, FontSecondary);
    uint8_t y_offset = 24;
    uint8_t line_height = 10;

    for(uint16_t i = 0; i < doc_model->lines_per_screen; i++) {
        uint16_t line_idx = doc_model->current_line + i;
        if(line_idx >= doc_model->total_lines) {
            break;
        }

        // Get line content
        uint16_t start = doc_model->line_offsets[line_idx];
        uint16_t end;

        if(line_idx + 1 < doc_model->total_lines) {
            end = doc_model->line_offsets[line_idx + 1] - 1; // -1 to exclude newline
        } else {
            end = doc_model->content_length;
        }

        // Copy line content to temporary buffer for display
        if(end > start && (end - start) < MAX_LINE_LENGTH) {
            char line_buf[MAX_LINE_LENGTH];
            size_t line_len = end - start;
            if(line_len > 0) {
                memcpy(line_buf, furi_string_get_cstr(doc_model->content) + start, line_len);
                line_buf[line_len] = '\0';

                // Remove CR if present
                if(line_len > 0 && line_buf[line_len - 1] == '\r') {
                    line_buf[line_len - 1] = '\0';
                }

                canvas_draw_str(canvas, 2, y_offset, line_buf);
            }
        }

        y_offset += line_height;
    }

    // Draw scrollbar if document has more than one page
    if(doc_model->total_lines > doc_model->lines_per_screen) {
        uint8_t scrollbar_x = 124;
        uint8_t scrollbar_height = 48;
        uint8_t scrollbar_y = 15;

        // Draw scrollbar background
        canvas_draw_line(
            canvas, scrollbar_x, scrollbar_y, scrollbar_x, scrollbar_y + scrollbar_height);

        // Draw scrollbar position indicator
        float position_ratio = (float)doc_model->current_line /
                               (float)(doc_model->total_lines - doc_model->lines_per_screen);
        if(position_ratio > 1.0f) position_ratio = 1.0f;

        uint8_t indicator_pos = scrollbar_y + (position_ratio * scrollbar_height);
        uint8_t indicator_height = 5;
        canvas_draw_box(canvas, scrollbar_x - 1, indicator_pos, 3, indicator_height);
    }
}

/**
 * Handle input events for text viewer
 */
static bool docview_text_viewer_input_callback(InputEvent* event, void* context) {
    DocViewApp* app = (DocViewApp*)context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            // Scroll up
            if(app->doc_model->current_line > 0) {
                app->doc_model->current_line--;
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyDown) {
            // Scroll down
            if(app->doc_model->current_line + app->doc_model->lines_per_screen <
               app->doc_model->total_lines) {
                app->doc_model->current_line++;
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyLeft) {
            // Page up
            if(app->doc_model->current_line > 0) {
                app->doc_model->current_line -= app->doc_model->lines_per_screen;
                if(app->doc_model->current_line < 0) {
                    app->doc_model->current_line = 0;
                }
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        } else if(event->key == InputKeyRight) {
            // Page down
            if(app->doc_model->current_line + app->doc_model->lines_per_screen <
               app->doc_model->total_lines) {
                app->doc_model->current_line += app->doc_model->lines_per_screen;
                if(app->doc_model->current_line + app->doc_model->lines_per_screen >
                   app->doc_model->total_lines) {
                    app->doc_model->current_line =
                        app->doc_model->total_lines - app->doc_model->lines_per_screen;
                    if(app->doc_model->current_line < 0) {
                        app->doc_model->current_line = 0;
                    }
                }
                with_view_model(
                    app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
            }
            consumed = true;
        }
    }

    return consumed;
}

/**
 * Handle custom events for text viewer
 */
static bool docview_text_viewer_custom_event_callback(uint32_t event, void* context) {
    DocViewApp* app = (DocViewApp*)context;
    bool handled = false;

    switch(event) {
    case DocViewEventIdLoadFile:
        docview_load_file(app);
        with_view_model(
            app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
        handled = true;
        break;
    case DocViewEventIdNextPage:
        if(app->doc_model->current_line + app->doc_model->lines_per_screen <
           app->doc_model->total_lines) {
            app->doc_model->current_line += app->doc_model->lines_per_screen;
            with_view_model(
                app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
        }
        handled = true;
        break;
    case DocViewEventIdPrevPage:
        if(app->doc_model->current_line > 0) {
            app->doc_model->current_line -= app->doc_model->lines_per_screen;
            if(app->doc_model->current_line < 0) {
                app->doc_model->current_line = 0;
            }
            with_view_model(
                app->text_viewer, DocViewModel * model, { *model = *app->doc_model; }, true);
        }
        handled = true;
        break;
    }

    return handled;
}

// Settings for font sizes
static const char* const font_size_names[] = {"Small", "Medium", "Large"};
static void docview_font_size_changed(VariableItem* item) {
    DocViewApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, font_size_names[index]);

    app->doc_model->settings.font_size = index;
    // Adjust lines per screen based on font size
    switch(index) {
    case 0: // Small
        app->doc_model->lines_per_screen = 8;
        break;
    case 1: // Medium
        app->doc_model->lines_per_screen = 6;
        break;
    case 2: // Large
        app->doc_model->lines_per_screen = 4;
        break;
    }
}

// Settings for word wrap
static const char* const word_wrap_names[] = {"Off", "On"};
static void docview_word_wrap_changed(VariableItem* item) {
    DocViewApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, word_wrap_names[index]);

    app->doc_model->settings.word_wrap = (index == 1);
}

/**
 * Allocate and initialize the application
 */
static DocViewApp* docview_app_alloc() {
    DocViewApp* app = malloc(sizeof(DocViewApp));

    // Open required records
    Gui* gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    // Initialize the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Allocate file path string
    app->file_path = furi_string_alloc_set("/ext");
    app->file_buffer = NULL;

    // Initialize document model
    app->doc_model = malloc(sizeof(DocViewModel));
    app->doc_model->file_path = furi_string_alloc();
    app->doc_model->content = furi_string_alloc();
    app->doc_model->content_length = 0;
    app->doc_model->current_line = 0;
    app->doc_model->total_lines = 0;
    app->doc_model->lines_per_screen = 8;
    app->doc_model->settings.font_size = 0;
    app->doc_model->settings.word_wrap = false;
    app->doc_model->settings.scroll_speed = 0;

    // Initialize main menu
    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Browse Files", DocViewSubmenuIndexBrowse, docview_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", DocViewSubmenuIndexSettings, docview_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", DocViewSubmenuIndexAbout, docview_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), docview_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, DocViewViewSubmenu, submenu_get_view(app->submenu));

    // Initialize text viewer
    app->text_viewer = view_alloc();
    view_set_context(app->text_viewer, app);
    view_set_draw_callback(app->text_viewer, docview_text_viewer_draw_callback);
    view_set_input_callback(app->text_viewer, docview_text_viewer_input_callback);
    view_set_previous_callback(app->text_viewer, docview_navigation_submenu_callback);
    view_set_custom_callback(app->text_viewer, docview_text_viewer_custom_event_callback);
    view_allocate_model(app->text_viewer, ViewModelTypeLocking, sizeof(DocViewModel));
    view_dispatcher_add_view(app->view_dispatcher, DocViewViewTextViewer, app->text_viewer);

    // Initialize settings menu
    app->settings_menu = variable_item_list_alloc();

    // Font size setting
    VariableItem* item = variable_item_list_add(
        app->settings_menu,
        "Font Size",
        3, // small, medium, large
        docview_font_size_changed,
        app);
    variable_item_set_current_value_index(item, app->doc_model->settings.font_size);
    variable_item_set_current_value_text(
        item, font_size_names[app->doc_model->settings.font_size]);

    // Word wrap setting
    item = variable_item_list_add(
        app->settings_menu,
        "Word Wrap",
        2, // on/off
        docview_word_wrap_changed,
        app);
    variable_item_set_current_value_index(item, app->doc_model->settings.word_wrap ? 1 : 0);
    variable_item_set_current_value_text(
        item, word_wrap_names[app->doc_model->settings.word_wrap ? 1 : 0]);

    view_set_previous_callback(
        variable_item_list_get_view(app->settings_menu), docview_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        DocViewViewSettings,
        variable_item_list_get_view(app->settings_menu));

    // Initialize about screen
    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Doc Viewer Lite\n"
        "Version 0.1\n"
        "-----------------\n"
        "A simple document viewer\n"
        "for Flipper Zero.\n\n"
        "Controls:\n"
        "Up/Down: Scroll line by line\n"
        "Left/Right: Page up/down\n"
        "Back: Return to menu");
    view_set_previous_callback(
        widget_get_view(app->widget_about), docview_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, DocViewViewAbout, widget_get_view(app->widget_about));

    // Open and configure notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    // Start with the main menu
    view_dispatcher_switch_to_view(app->view_dispatcher, DocViewViewSubmenu);

    return app;
}

/**
 * Free application resources
 */
static void docview_app_free(DocViewApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, DocViewViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, DocViewViewTextViewer);
    view_free(app->text_viewer);

    view_dispatcher_remove_view(app->view_dispatcher, DocViewViewSettings);
    variable_item_list_free(app->settings_menu);

    view_dispatcher_remove_view(app->view_dispatcher, DocViewViewAbout);
    widget_free(app->widget_about);

    // Free file resources
    if(app->file_buffer) {
        free(app->file_buffer);
    }

    // Free document model
    furi_string_free(app->doc_model->file_path);
    furi_string_free(app->doc_model->content);
    free(app->doc_model);

    // Free file path
    furi_string_free(app->file_path);

    // Free view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    // Free the app
    free(app);
}

/**
 * Application entry point
 */
int32_t docviewlite_app(void* p) {
    UNUSED(p);

    DocViewApp* app = docview_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    docview_app_free(app);

    return 0;
}
