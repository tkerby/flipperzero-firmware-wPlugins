#include <furi.h>         // Core Furi OS functionality
#include <gui/gui.h>      // GUI system
#include <input/input.h>  // Input handling (buttons)
#include <stdint.h>       // Standard integer types
#include <stdlib.h>       // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include "tree_ident_icons.h"  // Custom icon definitions

// Tag for logging purposes
#define TAG "Tree Ident"

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;             // ViewPort for rendering UI
    Gui* gui;                        // GUI instance
    uint8_t current_screen;          // 0 = first screen, 1 = second screen
} Tree_ident;

// Draw callback - called whenever the screen needs to be redrawn
void draw_callback(Canvas* canvas, void* context) {
    Tree_ident* app = context;  // Get app context to check current screen

    // Clear the canvas and set drawing color to black
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    if(app->current_screen == 0) { // First screen ----------------------------
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 1, 5, AlignLeft, AlignTop, "Tree Ident");

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 1, 14, AlignLeft, AlignTop, "Welcome.");

        canvas_draw_icon(canvas, 63, 1, &I_icon_64x64);
		// Draw button hints at bottom using elements library
		elements_button_left(canvas, "Exit"); // for the Back button
		elements_button_center(canvas, "Continue"); // for the OK button
    } else { // Second screen -------------------------------------------------
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Second Screen");
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "You pressed OK!");
        
        // Navigation hint using elements library
        elements_button_left(canvas, "Return");
    }
}

void input_callback(InputEvent* event, void* context) {
    Tree_ident* app = context;
	// Put the input event into the message queue for processing
    // Timeout of 0 means non-blocking
    furi_message_queue_put(app->input_queue, event, 0);
}

int32_t tree_ident_main(void* p) {
    UNUSED(p);

    Tree_ident app; // Application state struct
	app.current_screen = 0;  // Start on first screen

    // Allocate resources for rendering and 
    app.view_port = view_port_alloc(); // for rendering
    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent)); // for 8 input events

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
		case InputKeyOk:
            // OK button: navigate to second screen or stay there
            if(app.current_screen == 0) {
                app.current_screen = 1;  // Move to second screen
            }
            break;
        case InputKeyBack:
            // Back button: return to first screen or exit app
            if(app.current_screen == 1) {
                app.current_screen = 0;  // Return to first screen
            } else { // On first screen
                if(input.type == InputTypeLong) {
                    exit_loop = 1;  // Exit app after long press
                }
            }
            break;
        case InputKeyLeft:
        case InputKeyRight:
        case InputKeyUp:
        case InputKeyDown:
        default:
            break;
        }
		
		// Exit loop if flag is set
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
