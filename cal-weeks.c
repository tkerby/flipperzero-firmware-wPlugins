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

extern const Icon I_splash, I_icon_10x10;

typedef enum {
   ScreenSplash,
   ScreenWeekView
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;            // ViewPort for rendering UI
    Gui* gui;                       // GUI instance
    uint8_t current_screen;        
} AppState;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================
// none yet

// =============================================================================
// SCREEN DRAWING FUNCTIONS
// =============================================================================
static void draw_screen_splash(Canvas* canvas) {
    canvas_draw_icon(canvas, 1, 1, &I_splash); // 51 is a pixel above the buttons
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 1, 1, AlignLeft, AlignTop, "Calendar of weeks);
    canvas_draw_str_aligned(canvas, 88, 56, AlignLeft, AlignTop, "f418.eu");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Hold 'back'");
    canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "to exit.");
    canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.1");
    
    // Draw button hints at bottom using elements library
    elements_button_center(canvas, "OK"); // for the OK button
}

static void draw_screen_weeks(Canvas* canvas, AppState* state, DateTime* datetime) {
    char buffer[64]; // buffer for string concatination
    // App name and icon
    canvas_draw_icon(canvas, 1, -1, &I_icon_10x10);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "Weeks");  
    canvas_set_font(canvas, FontSecondary);
    // Display current date
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",datetime->year, datetime->month, datetime->day);
    canvas_draw_str_aligned(canvas, 60, 2, AlignLeft, AlignTop, buffer);
}

// =============================================================================
// MAIN CALLBACK - called whenever the screen needs to be redrawn
// =============================================================================
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context;  // Get app context to check current screen
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
  	// Get current time
  	DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
	
    switch (state -> current_screen) {
		case ScreenSplash: // Splash screen ===================================
            draw_screen_splash(canvas);
			break;	
		case ScreenWeeksView: // Week View ====================================
            draw_ screen_weeks(canvas, state, &datetime);
			break;	
    }
}

// =============================================================================
// MAIN APPLICATION
// =============================================================================
int32_t cal_weeks_main(void* p) {
  UNUSED(p);
  AppState app; // Application state struct
	app.current_screen = ScreenSplash;  // Start on splash screen
  app.view_port = view_port_alloc(); // for rendering
  
  view_port_draw_callback_set(app.view_port, draw_callback, &app);
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
				if (input.type == InputTypePress){
					switch (app.current_screen) {
						case ScreenSplash:
							app.current_screen = ScreenWeekView;   
						break;
					}
					break;
				}
				break;
			case InputKeyBack:
			default:
				if(input.type == InputTypeLong) {
					exit_loop = 1;  // Exit app after long press
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

    return 0;
}
