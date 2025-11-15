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
    ScreenL1Question,
    ScreenL1_A1,
    ScreenL1_A2,
    ScreenL1_A3, 
	ScreenL2_A1_1,  // Real needles
	ScreenL2_A1_2,  // Scales
	ScreenL2_A2_1,  // Compound leaves
	ScreenL2_A2_2   // Single leaf
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;            // ViewPort for rendering UI
    Gui* gui;                       // GUI instance
    uint8_t current_screen;         // 0 = first screen, 1 = second screen
	int selected_L1_option;         // Selected option for Level 1 
	int selected_L2_option;         // Selected option for Level 2
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
			canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.3");
			
			// Draw button hints at bottom using elements library
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenL1Question: // Second screen -------------------------------------------------
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
			// Level 1 Question
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "My tree has...");
			canvas_draw_frame(canvas, 1, 11, 127, 34); // Menu box: x, y, w, h
            // Level 1 Choices
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str(canvas, 7, 21, ".. needles.");
            canvas_draw_str(canvas, 7, 31, ".. leaves.");
            canvas_draw_str(canvas, 7, 41, "I don't know.");       
            // Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L1_option* 10), ">");
						
			// Navigation hints
			canvas_draw_icon(canvas, 119, 34, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 39, &I_ButtonDown_7x4);				
			canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Press 'back'");	
			canvas_draw_str_aligned(canvas, 1, 56, AlignLeft, AlignTop, "for splash.");	
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenL1_A1: // Level 1: Option 1: Needle Tree
 			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1 		 
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Needle-leaves are..");
            canvas_draw_frame(canvas, 1, 11, 127, 24); // Menu box: x, y, w, h
            // Level 2 Answer 1 Choices
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str(canvas, 7, 21, "long and pointed needles");
            canvas_draw_str(canvas, 7, 31, "are small and flat scales");
			// Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L2_option* 10), ">");
			// Navigation hints		
			canvas_draw_icon(canvas, 119, 24, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 29, &I_ButtonDown_7x4);	
			elements_button_left(canvas, "Prev. ?"); 
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenL1_A2: // Level 2: Option 2: Leave Tree
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
			canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1 	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Broad leaves are..");
            canvas_draw_frame(canvas, 1, 11, 127, 24); // Menu box: x, y, w, h
			// Level 2 Answer 2 Choices
			canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 7, 21, "compound leaves w. leaflets");
            canvas_draw_str(canvas, 7, 31, "single, undivided leaves");
			// Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L2_option* 10), ">");
			// Navigation bar
			canvas_draw_icon(canvas, 119, 24, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 29, &I_ButtonDown_7x4);				
			elements_button_left(canvas, "Prev. ?"); 
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenL1_A3: // Level 2: Option 3: Nothing chosen
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "No leave type chosen");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Wait to next spring :-)");
			elements_button_left(canvas, "Prev. question"); 
			break;
		case ScreenL2_A1_1: // Level 2: Option 1_1: Needles
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1 				
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Real needles");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones.");
			elements_button_left(canvas, "Prev. question"); 
			break;
		case ScreenL2_A1_2: // Level 2: Option 1_2: Scales
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Scales");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones.");
			elements_button_left(canvas, "Prev. question"); 
			break;
		case ScreenL2_A2_1: // Level 2: Option 2_1: Compound leave
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Compound leaves");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones.");
			elements_button_left(canvas, "Prev. question"); 
			break;
		case ScreenL2_A2_2: // Level 2: Option 2_2: Undivided leaf
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Undivided leaf");			
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hic sunt leones.");
			elements_button_left(canvas, "Prev. question"); 
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
	app.selected_L1_option= 0; // Level 1: Start with first line highlighted
	app.selected_L2_option= 0; // Level 2: Start with first line highlighted

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
			if(app.current_screen == ScreenL1Question && input.type == InputTypePress) {
				app.selected_L1_option= (app.selected_L1_option- 1 + 3) % 3;
			}
			if((app.current_screen == ScreenL1_A1 || app.current_screen == ScreenL1_A2)
				&& input.type == InputTypePress) {
				app.selected_L2_option= (app.selected_L2_option- 1 + 2) % 2;
			}
			break;
		case InputKeyDown:
			if(app.current_screen == ScreenL1Question && input.type == InputTypePress) {
				app.selected_L1_option= (app.selected_L1_option+ 1) % 3;
			}
			if((app.current_screen == ScreenL1_A1 || app.current_screen == ScreenL1_A2)
				&& input.type == InputTypePress) {
				app.selected_L2_option= (app.selected_L2_option- 1 + 2) % 2;
			}
			break;
		case InputKeyOk:
			if (input.type == InputTypePress){
				switch (app.current_screen) {
					case ScreenSplash:
						app.current_screen = ScreenL1Question;  // Move to second screen
						break;
					case ScreenL1Question:
						switch (app.selected_L1_option){
							case 0: app.current_screen = ScreenL1_A1; break;
							case 1: app.current_screen = ScreenL1_A2; break;
							case 2: app.current_screen = ScreenL1_A3; break;
						}
						break;
					case ScreenL1_A1: // Needles
						switch (app.selected_L2_option){
							case 0: app.current_screen = ScreenL2_A1_1; break;
							case 1: app.current_screen = ScreenL2_A1_2; break;
						}
						break;
					case ScreenL1_A2: // Leaves
						switch (app.selected_L2_option){
							case 0: app.current_screen = ScreenL2_A2_1; break;
							case 1: app.current_screen = ScreenL2_A2_2; break;
						}
						break;
					default:
						break;
				}
		}
            break;
        case InputKeyBack:
			switch(app.current_screen){
				case ScreenSplash:
					if(input.type == InputTypeLong) {
						exit_loop = 1;  // Exit app after long press
					}
					break;
				case ScreenL1Question: // Back button: return to first screen
					app.current_screen = ScreenSplash;
					break;
	
				
			}
            break;
        case InputKeyLeft: 
			if (input.type == InputTypePress){
				switch(app.current_screen){
					case ScreenL1_A1:
					case ScreenL1_A2:
					case ScreenL1_A3:
						app.current_screen = ScreenL1Question;
						break;
					case ScreenL2_A1_1:
					case ScreenL2_A1_2:
						app.current_screen = ScreenL1_A1;
						break;		
					case ScreenL2_A2_1:
					case ScreenL2_A2_2:
						app.current_screen = ScreenL1_A2;
						break;	
					default:
						break;
				}
				break;
			}
			break;
        case InputKeyRight:
		    switch (app.current_screen) {
				case ScreenL1Question:
					switch (app.selected_L1_option){
						case 0: app.current_screen = ScreenL1_A1; break;
						case 1: app.current_screen = ScreenL1_A2; break;
						case 2: app.current_screen = ScreenL1_A3; break;
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
