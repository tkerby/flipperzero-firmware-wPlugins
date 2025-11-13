#include <furi.h>         // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h>      // GUI system
#include <input/input.h>  // Input handling (buttons)
#include <stdint.h>       // Standard integer types
#include <stdlib.h>       // Standard library functions
#include <gui/elements.h> // to access button drawing functions

#include "tree_ident_icons.h"  // Custom icon definitions

// Tag for logging purposes
#define TAG "Tree Ident"

typedef enum {
    ScreenSplash,
    ScreenQuestion,
    ScreenA1,
    ScreenA2,
    ScreenA3,
    ScreenA4
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;             // ViewPort for rendering UI
    Gui* gui;                        // GUI instance
    uint8_t current_screen;          // 0 = first screen, 1 = second screen
	int selected_option;
} AppState;

// Draw callback - called whenever the screen needs to be redrawn
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context;  // Get app context to check current screen

    // Clear the canvas and set drawing color to black
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    switch (state -> current_screen) {
		case ScreenSplash: // First screen ----------------------------
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 1, 5, AlignLeft, AlignTop, "Identify a tree");
			
			canvas_draw_icon(canvas, 75, 1, &I_icon_52x52); // 51 is a pixel above the buttons

			canvas_set_color(canvas, ColorBlack);
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str_aligned(canvas, 1, 16, AlignLeft, AlignTop, "Just answer a few");
			canvas_draw_str_aligned(canvas, 1, 25, AlignLeft, AlignTop, "questions.");
			
			canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Hold 'back'");
			canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "to exit.");
			
			canvas_draw_str_aligned(canvas, 100, 54, AlignLeft, AlignTop, "F. Greil");
			canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.9");
			
			// Draw button hints at bottom using elements library
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenQuestion: // Second screen -------------------------------------------------
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);		
			// Draw menu box and options
            canvas_draw_frame(canvas, 13, 5, 108, 44); // Menu box
            canvas_draw_str(canvas, 20, 15, "Screen A1");
            canvas_draw_str(canvas, 20, 25, "Screen A2");
            canvas_draw_str(canvas, 20, 35, "Screen A3");
            canvas_draw_str(canvas, 20, 45, "Screen A4");
            
            // Draw selection indicator
            canvas_draw_str(canvas, 15, 15 + (state->selected_option * 10), ">");
			
			
			// Navigation hints 
			canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Press 'back' for");	
			canvas_draw_str_aligned(canvas, 1, 56, AlignLeft, AlignTop, "splash screen.");	

			canvas_draw_icon(canvas, 64, 54, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 64, 59, &I_ButtonDown_7x4);
			// The elements library is helpful
			elements_button_right(canvas, "Select");
			break;
		case ScreenA1:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Screen A1 Content");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones");
			elements_button_left(canvas, "Menu"); 

        break;
		case ScreenA2:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Screen A2 Content");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones");
			
			elements_button_left(canvas, "Menu"); 
        break;
		case ScreenA3:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Screen A3 Content");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones");
			
			elements_button_left(canvas, "Menu"); 
        break;
		case ScreenA4:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Screen A4 Content");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones");
			
			elements_button_left(canvas, "Menu"); 
        break;
    }
}

void input_callback(InputEvent* event, void* context) {
    AppState* app = context;
	// Put the input event into the message queue for processing
    // Timeout of 0 means non-blocking
    furi_message_queue_put(app->input_queue, event, 0);
	
	// furi_assert(context);
    // FuriMessageQueue* event_queue = context;
    // furi_message_queue_put(event_queue, event, FuriWaitForever);
}

int32_t tree_ident_main(void* p) {
    UNUSED(p);

    AppState app; // Application state struct
	app.current_screen = ScreenSplash;  // Start on first screen (0)
	app.selected_option = 0; // Start with first line highlighted

    // Allocate resources for rendering and 
    app.view_port = view_port_alloc(); // for rendering
    app.input_queue = furi_message_queue_alloc(1, sizeof(InputEvent)); // Only one element in queue

    // Callbacks
    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);

    // Initialize GUI
    app.gui = furi_record_open("gui");
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    // Input handling
    InputEvent input;
    uint8_t exit_loop = 0; // Flag to exit main loop

    FURI_LOG_I(TAG, "Start the main loop.");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);
		// Handle button presses based on current screen
        switch(input.key) {
		case InputKeyUp:
			if(app.current_screen == ScreenQuestion) {
				app.selected_option = (app.selected_option - 1 + 4) % 4;
				furi_delay_ms(500);
			}
			break;
		case InputKeyDown:
			if(app.current_screen == ScreenQuestion) {
				app.selected_option = (app.selected_option + 1) % 4;
				furi_delay_ms(500);
			}
			break;
		case InputKeyOk:
            switch (app.current_screen) {
				case ScreenSplash:
					app.current_screen = ScreenQuestion;  // Move to second screen
					break;
				default:
					break;
            }
            break;
        case InputKeyBack:
			switch(app.current_screen){
				case ScreenSplash:
					if(input.type == InputTypeLong) {
						exit_loop = 1;  // Exit app after long press
					}
					break;
				case ScreenQuestion: // Back button: return to first screen
					app.current_screen = ScreenSplash;
					break;
			}
            break;
        case InputKeyLeft: 
			switch(app.current_screen){
				case ScreenA1:
					app.current_screen = ScreenQuestion;
					break;
				case ScreenA2:
					app.current_screen = ScreenQuestion;
					break;
				case ScreenA3:
					app.current_screen = ScreenQuestion;
					break;
				case ScreenA4:
					app.current_screen = ScreenQuestion;
					break;
				default:
					break;
            }
			break;
        case InputKeyRight:
		    switch (app.current_screen) {
				case ScreenQuestion:
					switch (app.selected_option){
						case 0: app.current_screen = ScreenA1; break;
						case 1: app.current_screen = ScreenA2; break;
						case 2: app.current_screen = ScreenA3; break;
						case 3: app.current_screen = ScreenA4; break;
					}
					break;
				default: break;
            }
            break;
        default:
            break;
        }
		
		// Exit main app loop if exit flag is set
        if(exit_loop) {
            break;
        }

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
