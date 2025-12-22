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

#define TAG "cal-weeks" // Tag for logging purposes

// Layout constants for week view screen
#define WEEK_VIEW_RIGHT_MARGIN 10
#define WEEK_VIEW_WIDTH (128 - WEEK_VIEW_RIGHT_MARGIN)
#define WEEK_VIEW_CENTER (WEEK_VIEW_WIDTH / 2)
#define WEEK_VIEW_SLOT_WIDTH 17
#define WEEK_VIEW_MAX_X (WEEK_VIEW_WIDTH - 1)

extern const Icon I_icon_10x10, I_arrows, I_back, I_folder, I_splash;

const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

typedef enum {
   ScreenSplash,
   ScreenDayView
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;            // ViewPort for rendering UI
    Gui* gui;                       // GUI instance
    uint8_t current_screen;
    int selected_week_offset;       // Offset from current week (-1, 0, 1, etc.)
    int selected_day;               // Selected day of week (0=Mon, 6=Sun)
} AppState;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Calculate absolute date from base date plus week offset
void calculate_date_with_offset(DateTime* base_datetime, int week_offset, DateTime* result_datetime) {
    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Copy base date
    result_datetime->year = base_datetime->year;
    result_datetime->month = base_datetime->month;
    result_datetime->day = base_datetime->day;
    result_datetime->hour = base_datetime->hour;
    result_datetime->minute = base_datetime->minute;
    result_datetime->second = base_datetime->second;
    result_datetime->weekday = base_datetime->weekday;
    
    // Add week offset (in days)
    int day_offset = week_offset * 7;
    int new_day = result_datetime->day + day_offset;
    
    // Adjust for month/year boundaries
    while (new_day < 1 || new_day > days_in_month[result_datetime->month - 1]) {
        // Check for leap year
        if ((result_datetime->year % 4 == 0 && result_datetime->year % 100 != 0) || 
            (result_datetime->year % 400 == 0)) {
            days_in_month[1] = 29;
        } else {
            days_in_month[1] = 28;
        }
        
        if (new_day < 1) {
            result_datetime->month--;
            if (result_datetime->month < 1) {
                result_datetime->month = 12;
                result_datetime->year--;
            }
            new_day += days_in_month[result_datetime->month - 1];
        } else if (new_day > days_in_month[result_datetime->month - 1]) {
            new_day -= days_in_month[result_datetime->month - 1];
            result_datetime->month++;
            if (result_datetime->month > 12) {
                result_datetime->month = 1;
                result_datetime->year++;
            }
        }
    }
    
    result_datetime->day = new_day;
}

void get_week_days(int year, int month, int day, int week_offset, char out[7][4]) {
    // Simple day-of-week calculation using Zeller's congruence (modified for Monday=0)
    int m = month;
    int y = year;
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    int k = y % 100;
    int j = y / 100;
    int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    int wday = (h + 5) % 7;  // Convert to Monday=0
    
    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // Check for leap year
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }
    
    // Calculate the 7 days of the week starting from Monday
    int curr_day = day - wday + (week_offset * 7);  // Add week offset here
    int curr_month = month;
    int curr_year = year;
    
    for (int i = 0; i < 7; i++) {
        // Handle month/year boundaries
        if (curr_day < 1) {
            curr_month--;
            if (curr_month < 1) {
                curr_month = 12;
                curr_year--;
            }
            curr_day += days_in_month[curr_month - 1];
        } else if (curr_day > days_in_month[curr_month - 1]) {
            curr_day -= days_in_month[curr_month - 1];
            curr_month++;
            if (curr_month > 12) {
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
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
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

// Helper function to determine if today appears in a week with given offset
// Returns column index (0-6) if today is in that week, -1 otherwise
int get_today_column(DateTime* today, DateTime* ref_date, int week_offset) {
    int ref_wday = (ref_date->weekday + 6) % 7;  // Day of week for reference date (Mon=0)
    
    // Calculate approximate day difference (sufficient for week-range checking)
    int days_diff = (today->year - ref_date->year) * 365 + 
                    (today->month - ref_date->month) * 30 + 
                    (today->day - ref_date->day);
    
    // Adjust for the week offset
    int week_start = week_offset * 7 - ref_wday;
    int week_end = week_start + 6;
    
    if (days_diff >= week_start && days_diff <= week_end) {
        return (days_diff - week_start);
    }
    return -1;
}

// =============================================================================
// SCREEN DRAWING FUNCTIONS
// =============================================================================
static void draw_screen_splash(Canvas* canvas, DateTime* datetime, AppState* state) {
	char buffer[64]; // buffer for string concatination
	
	// Calculate the reference date based on selected week offset
	DateTime ref_datetime;
	calculate_date_with_offset(datetime, state->selected_week_offset, &ref_datetime);
	
	canvas_draw_icon(canvas, 1, 1, &I_splash); // 51 is a pixel above the buttons
    canvas_draw_icon(canvas, 1, -1, &I_icon_10x10); // App icon 
 	canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
	// Display selected week number and year as title
	int week_num = get_iso_week_number(ref_datetime.year, ref_datetime.month, ref_datetime.day);
    snprintf(buffer, sizeof(buffer), "%04d Week %02d", ref_datetime.year, week_num);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, buffer);
    canvas_set_font(canvas, FontSecondary);
	// Write day names
	for(int i = 0; i < 7; i++) {
		if (i == 5) canvas_set_color(canvas, ColorWhite);
		canvas_draw_str_aligned(canvas, 3 + i * 18, 12, AlignLeft, AlignTop, days[i]);
	}
	canvas_set_color(canvas, ColorBlack);
	
	// Print previous week's day numbers (relative to selected week)
	int prev_week_today_col = get_today_column(datetime, &ref_datetime, -1);
	char days_prev_week[7][4]; // output array to be populated by next function call
	get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, -1, days_prev_week);
	for(int i = 0; i < 7; i++) {
		if(i == prev_week_today_col) {
			canvas_set_font(canvas, FontPrimary);  // Bold font if today is in this week
		}
		canvas_draw_str_aligned(canvas, 18 + i*18, 22, AlignRight, AlignTop, days_prev_week[i]);
		if(i == prev_week_today_col) {
			canvas_set_font(canvas, FontSecondary);  // Back to normal font
		}
	};
	
	// Print current/selected week day numbers
	char days_current_week[7][4]; // output array to be populated by next function call
	get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, 0, days_current_week);
	int current_week_today_col = get_today_column(datetime, &ref_datetime, 0);			
	
	for(int i = 0; i < 7; i++) {
		// Draw cursor box for selected day
		if(i == state->selected_day) {
			canvas_draw_box(canvas, 2 + i*18, 30, 18, 11);  // Draw filled black box
			canvas_set_color(canvas, ColorWhite);  // Switch to white for text
		}
		// Make actual current day bold
		if(i == current_week_today_col) {
			canvas_set_font(canvas, FontPrimary);  // Switch to bold font
		}
		
		canvas_draw_str_aligned(canvas, 18 + i*18, 32, AlignRight, AlignTop, days_current_week[i]);
		
		if(i == state->selected_day) {
			canvas_set_color(canvas, ColorBlack);  // Switch back to black
		}
		// Reset font if we made it bold
		if(i == current_week_today_col) {
			canvas_set_font(canvas, FontSecondary);  // Reset to normal font
		}
	}
	
	// Print week day numbers of succeeding week (relative to selected week)
	int next_week_today_col = get_today_column(datetime, &ref_datetime, 1);
	char days_next_week[7][4]; // output array
	get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, 1, days_next_week);
	for(int i = 0; i < 7; i++) {
		if(i == next_week_today_col) {
			canvas_set_font(canvas, FontPrimary);  // Bold for today
		}
		canvas_draw_str_aligned(canvas, 18 + i*18, 42, AlignRight, AlignTop, days_next_week[i]);
		if(i == next_week_today_col) {
			canvas_set_font(canvas, FontSecondary);  // Back to normal
		}
	};
	// Navigation hint
	canvas_draw_icon(canvas, 1, 55, &I_arrows);
	canvas_draw_str_aligned(canvas, 11, 63, AlignLeft, AlignBottom, "Choose");
	// Only show "Today" hint if cursor is not already on today
	int today_day = (datetime->weekday + 6) % 7;
			canvas_draw_icon(canvas, 121, 55, &I_back);
	if (state->selected_week_offset != 0 || state->selected_day != today_day) {	
		canvas_draw_str_aligned(canvas, 120, 62, AlignRight, AlignBottom, "Today");	
	} else {
	    canvas_draw_str_aligned(canvas, 120, 62, AlignRight, AlignBottom, "Hold: exit");	
	}
	// Version info
    canvas_draw_str_aligned(canvas, 105, 1, AlignRight, AlignTop, "v0.2");    
    // Draw OK button using elements library
    elements_button_center(canvas, "OK"); // for the OK button
}

static void draw_screen_day(Canvas* canvas, DateTime* datetime, AppState* state) {
	char buffer[64]; // buffer for string concatenation
	canvas_draw_icon(canvas, 118, -1, &I_folder); // Icon "open file" 
	
	// Calculate the selected date
	DateTime selected_datetime;
	calculate_date_with_offset(datetime, state->selected_week_offset, &selected_datetime);
	
	// Get the start of the selected week
	char days_selected_week[7][4];
	get_week_days(selected_datetime.year, selected_datetime.month, selected_datetime.day, 0, days_selected_week);
	
	// Adjust to get the actual selected day's date
	int selected_day_num = atoi(days_selected_week[state->selected_day]);
	
	canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
	
	// Draw title with selected date
	snprintf(buffer, sizeof(buffer), "%s, %04d-%02d-%02d", 
	         days[state->selected_day], 
	         selected_datetime.year, 
	         selected_datetime.month, 
	         selected_day_num);
	canvas_draw_str_aligned(canvas, 1, 1, AlignLeft, AlignTop, buffer);
	
	// Draw header and footer lines
	canvas_draw_line(canvas, 0, 11, WEEK_VIEW_MAX_X, 11); // upper line of header
	canvas_draw_line(canvas, 0, 21, WEEK_VIEW_MAX_X, 21); // lower line of header
	
	// Time slots header (X axis)
	canvas_set_font(canvas, FontSecondary);
	const char* time_slots[] = {"08", "10", "12", "14", "16", "18", "20"};
	
	for(int i = 0; i < 7; i++) {
		int x = i * WEEK_VIEW_SLOT_WIDTH;
		// Draw hours
		canvas_draw_str(canvas, x, 20, time_slots[i]);
		// Draw vertical grid lines
		// if (i > 0) canvas_draw_line(canvas, x - 1, 11, x - 1, 63);
	}
	
	// Draw horizontal time grid
	int y_start = 23;
	int row_height = 13;
	for(int row = 0; row < 3; row++) {
		int y = y_start + row * row_height;
		canvas_draw_line(canvas, 0, y, WEEK_VIEW_MAX_X, y);
	}
	
	// Placeholder text for tasks/events
	canvas_set_font(canvas, FontSecondary);
	canvas_draw_str_aligned(canvas, WEEK_VIEW_CENTER, 26, AlignCenter, AlignTop, "No events available");
	
	canvas_draw_icon(canvas, 1, 55, &I_back);
	canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Choose other day");	
}

// =============================================================================
// MAIN CALLBACKS 
// =============================================================================
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context;  // Get app context to check current screen
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
  	// Get current time
  	DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
	
    switch (state -> current_screen) {
		case ScreenSplash: // Weeks screen =====================================
            draw_screen_splash(canvas, &datetime, state);
			break;	
		case ScreenDayView: // Day View ========================================
            draw_screen_day(canvas, &datetime, state);
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
  app.current_screen = ScreenSplash;  // Start on splash screen
  app.selected_week_offset = 0;       // Start with current week
  app.selected_day = 0;               // Start with Monday
  
  // Initialize selected_day to current day
  DateTime datetime;
  furi_hal_rtc_get_datetime(&datetime);
  app.selected_day = (datetime.weekday + 6) % 7;  // Convert Sun=0 to Mon=0
  
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
    if(input.type == InputTypePress) {
        switch(input.key) {
            case InputKeyUp:
                if(app.current_screen == ScreenSplash) {
                    // Shift to previous week
                    app.selected_week_offset--;
                }
                break;
                
            case InputKeyDown:
                if(app.current_screen == ScreenSplash) {
                    // Shift to next week
                    app.selected_week_offset++;
                }
                break;
                
            case InputKeyLeft:
                if(app.current_screen == ScreenSplash) {
                    // Move cursor left (with wrapping)
                    app.selected_day--;
                    if(app.selected_day < 0) {
                        app.selected_day = 6; // Wrap to Sunday
                    }
                }
                break;
                
            case InputKeyRight:
                if(app.current_screen == ScreenSplash) {
                    // Move cursor right (with wrapping)
                    app.selected_day++;
                    if(app.selected_day > 6) {
                        app.selected_day = 0; // Wrap to Monday
                    }
                }
                break;
                
            case InputKeyOk:
                switch (app.current_screen) {
                    case ScreenSplash:
                        app.current_screen = ScreenDayView;   
                        break;
                    case ScreenDayView:
                        // Could add functionality here
                        break;
                }
                break;
                
            case InputKeyBack:
                if(app.current_screen == ScreenDayView) {
                    app.current_screen = ScreenSplash;
                } else if(app.current_screen == ScreenSplash) {
                    // Reset to current week and today
                    app.selected_week_offset = 0;
                    DateTime current_datetime;
                    furi_hal_rtc_get_datetime(&current_datetime);
                    app.selected_day = (current_datetime.weekday + 6) % 7;  // Convert Sun=0 to Mon=0
                 }
                break;
                
            default:
                break;
        }
    } else if(input.type == InputTypeLong && input.key == InputKeyBack) {
        exit_loop = 1;  // Exit app after long press of Back
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
