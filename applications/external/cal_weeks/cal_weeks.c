#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include <stdio.h>
#include <string.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h> // for getting the current date
#include <cal_weeks_icons.h>

#define TAG "cal_weeks" // Tag for logging purposes

// extern const Icon I_icon_10x10, I_splash;

const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

typedef enum {
    ScreenSplash,
    ScreenWeekView
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue; // Queue for handling input events
    ViewPort* view_port; // ViewPort for rendering UI
    Gui* gui; // GUI instance
    uint8_t current_screen;
} AppState;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================
void get_week_days(int year, int month, int day, int week_offset, char out[7][4]) {
    // Simple day-of-week calculation using Zeller's congruence (modified for Monday=0)
    int m = month;
    int y = year;
    if(m < 3) {
        m += 12;
        y -= 1;
    }
    int k = y % 100;
    int j = y / 100;
    int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    int wday = (h + 5) % 7; // Convert to Monday=0

    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // Check for leap year
    if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    // Calculate the 7 days of the week starting from Monday
    int curr_day = day - wday + (week_offset * 7); // Add week offset here
    int curr_month = month;
    int curr_year = year;

    for(int i = 0; i < 7; i++) {
        // Handle month/year boundaries
        if(curr_day < 1) {
            curr_month--;
            if(curr_month < 1) {
                curr_month = 12;
                curr_year--;
            }
            curr_day += days_in_month[curr_month - 1];
        } else if(curr_day > days_in_month[curr_month - 1]) {
            curr_day -= days_in_month[curr_month - 1];
            curr_month++;
            if(curr_month > 12) {
                curr_month = 1;
                curr_year++;
            }
        }

        snprintf(out[i], 4, "%d", curr_day);
        curr_day++;
    }
}

int get_iso_week_number(int year, int month, int day) {
    // Simple ISO 8601 week number calculation
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    // Calculate day of year
    int day_of_year = day;
    for(int i = 0; i < month - 1; i++) {
        day_of_year += days_in_month[i];
    }

    // Simplified week calculation (approximate)
    int week = (day_of_year - 1) / 7 + 1;
    return week;
}

// =============================================================================
// SCREEN DRAWING FUNCTIONS
// =============================================================================
static void draw_screen_splash(Canvas* canvas, DateTime* datetime) {
    char buffer[64]; // buffer for string concatination
    // canvas_draw_icon(canvas, 1, 1, &I_splash); // 51 is a pixel above the buttons
    canvas_draw_icon(canvas, 1, -1, &I_icon_10x10);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    // Display current date as title in bold font face
    snprintf(
        buffer,
        sizeof(buffer),
        "Week of %04d-%02d-%02d",
        datetime->year,
        datetime->month,
        datetime->day);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, buffer);
    canvas_set_font(canvas, FontSecondary);
    // Write day names
    for(int i = 0; i < 7; i++) {
        if(i == 5) canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 3 + i * 18, 12, AlignLeft, AlignTop, days[i]);
    }
    canvas_set_color(canvas, ColorBlack);
    // Print previous week's day numbers
    char days_prev_week[7][4]; // output array to be populated by next function call
    get_week_days(datetime->year, datetime->month, datetime->day, -1, days_prev_week);
    for(int i = 0; i < 7; i++) {
        canvas_draw_str_aligned(canvas, 18 + i * 18, 22, AlignRight, AlignTop, days_prev_week[i]);
    };

    // Print current week day numbers
    char days_current_week[7][4]; // output array to be populated by next function call
    get_week_days(datetime->year, datetime->month, datetime->day, 0, days_current_week);
    for(int i = 0; i < 7; i++) {
        // Highlight current day with inverted colors
        int highlight_col = (datetime->weekday + 6) % 7; // Convert Sun=0 to Mon=0
        if(i == highlight_col) {
            canvas_draw_box(canvas, 2 + i * 18, 30, 18, 11); // Draw filled black box
            canvas_set_color(canvas, ColorWhite); // Switch to white for text
        }
        canvas_draw_str_aligned(
            canvas, 18 + i * 18, 32, AlignRight, AlignTop, days_current_week[i]);
        if(i == highlight_col) {
            canvas_set_color(canvas, ColorBlack); // Switch back to black
        }
    }
    // Print next week day numbers
    char days_next_week[7][4]; // output array
    get_week_days(datetime->year, datetime->month, datetime->day, 1, days_next_week);
    for(int i = 0; i < 7; i++) {
        canvas_draw_str_aligned(canvas, 18 + i * 18, 42, AlignRight, AlignTop, days_next_week[i]);
    };
    // Week number
    int week_num = get_iso_week_number(datetime->year, datetime->month, datetime->day);
    snprintf(buffer, sizeof(buffer), "Week# %02d", week_num);
    canvas_draw_str_aligned(canvas, 1, 64, AlignLeft, AlignBottom, buffer);
    // Version info
    canvas_draw_str_aligned(canvas, 128, 58, AlignRight, AlignBottom, "v0.1");
    // Draw button hints at bottom using elements library
    elements_button_center(canvas, "OK"); // for the OK button
}

static void draw_screen_weeks(Canvas* canvas, DateTime* datetime) {
    char buffer[64]; // buffer for string concatination
    canvas_set_font(canvas, FontPrimary);
    char days_current_week[7][4]; // output array to be populated by next function call
    get_week_days(datetime->year, datetime->month, datetime->day, 0, days_current_week);
    for(int i = 0; i < 7; i++) {
        // Highlight current day with inverted colors
        int highlight_col = (datetime->weekday + 6) % 7; // Convert Sun=0 to Mon=0
        if(i == highlight_col) {
            canvas_draw_box(canvas, 2 + i * 18, 1, 18, 11); // Draw filled black box
            canvas_set_color(canvas, ColorWhite); // Switch to white for text
        }
        canvas_draw_str_aligned(
            canvas, 18 + i * 18, 2, AlignRight, AlignTop, days_current_week[i]);
        if(i == highlight_col) {
            canvas_set_color(canvas, ColorBlack); // Switch back to black
        }
    }
    canvas_draw_line(canvas, 0, 0, 127, 0); // upper line of header
    canvas_draw_line(canvas, 0, 11, 127, 11); // lower line of header
    canvas_draw_line(canvas, 0, 63, 127, 63); // footer line
    canvas_draw_line(canvas, 91, 0, 91, 63); // vertical weekend separator
    // int dy = canvas_current_font_height(canvas);
    int dy = 17;
    for(uint8_t y = 11; y <= 61; y += dy) {
        canvas_draw_line(canvas, 8, y, 127, y);
    }
    // Print month
    canvas_set_font(canvas, FontSecondary);
    snprintf(buffer, sizeof(buffer), "%04d-%02d", datetime->year, datetime->month);
    canvas_set_font_direction(
        canvas, CanvasDirectionBottomToTop); // Set text rotation to 90 degrees
    canvas_draw_str(canvas, 7, 61, buffer);
    canvas_set_font_direction(
        canvas, CanvasDirectionLeftToRight); // Reset to normal text direction
}

// =============================================================================
// MAIN CALLBACKS
// =============================================================================
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context; // Get app context to check current screen
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    // Get current time
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    switch(state->current_screen) {
    case ScreenSplash: // Splash screen ===================================
        draw_screen_splash(canvas, &datetime);
        break;
    case ScreenWeekView: // Week View ====================================
        draw_screen_weeks(canvas, &datetime);
        break;
    }
}

// Input callback - puts input events into the queue
static void input_callback(InputEvent* input_event, void* context) {
    AppState* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

// =============================================================================
// MAIN APPLICATION
// =============================================================================
int32_t cal_weeks_main(void* p) {
    UNUSED(p);
    AppState app; // Application state struct
    app.current_screen = ScreenSplash; // Start on splash screen
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.view_port = view_port_alloc(); // for rendering

    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);
    app.gui = furi_record_open("gui"); // Initialize GUI
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    // Input handling
    InputEvent input;
    uint8_t exit_loop = 0; // Flag to exit main loop

    FURI_LOG_I(TAG, "Starting main app loop.");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);

        // Handle button presses and holds based on selected screen
        switch(input.key) {
        case InputKeyUp:
            break;
        case InputKeyDown:
            break;
        case InputKeyLeft:
        case InputKeyRight:
            break;
        case InputKeyOk:
            if(input.type == InputTypePress) {
                switch(app.current_screen) {
                case ScreenSplash:
                    app.current_screen = ScreenWeekView;
                    break;
                }
                break;
            }
            break;
        case InputKeyBack:
        default:
            if((input.type == InputTypePress) && (app.current_screen = ScreenWeekView)) {
                app.current_screen = ScreenSplash;
            };
            if(input.type == InputTypeLong) {
                exit_loop = 1; // Exit app after long press
            }
            break;
        }
        // Exit main app loop if exit flag is set
        if(exit_loop) break;
        // Trigger screen redraw
        view_port_update(app.view_port);
    }

    // Cleanup: Free all allocated resources
    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close("gui");
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    return 0;
}
