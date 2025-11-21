#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include "tree_ident_icons.h" // Custom icon definitions

#define TAG "Tree Ident" // Tag for logging purposes

typedef enum {
    ScreenSplash,
    ScreenL1Question,
    ScreenL1_A1,
    ScreenL1_A2,
    ScreenL1_A3, 
	ScreenL2_A1_1,  // Real needles
	ScreenL2_A1_2,  // Scales
	ScreenL2_A2_1,  // Compound leaves
	ScreenL2_A2_2,  // Single leaf
	ScreenL3_A1_1_1,  // Needles only on long shoots
	ScreenL3_A1_1_2,  // Needles on short shoots
	ScreenL3_A1_1_3,  // Needles on long and short shoots
	ScreenL3_A2_1_1,  // Odd # of leaflets, terminal leaflet
	ScreenL3_A2_1_2,  // Even # of leaflets, no terminal leaflet
	ScreenL3_A2_1_3,  // Leaflets radiate from single point
	ScreenL3_A2_2_1,  // Big rounded or pointed bumps
	ScreenL3_A2_2_2,  // Leaf margin is smooth
	ScreenL3_A2_2_3,  // Small teeth or cuts
	Screen_Fruit_Unknown,
    Screen_Fruit_Nut,
    Screen_Fruit_Samara,
    Screen_Fruit_Drupe,
    Screen_Fruit_Cone,
    Screen_Fruit_Berry,
    Screen_Fruit_Pome,
    Screen_Fruit_Other
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue;  // Queue for handling input events
    ViewPort* view_port;            // ViewPort for rendering UI
    Gui* gui;                       // GUI instance
    uint8_t current_screen;         // 0 = first screen, 1 = second screen
	int selected_L1_option;         // Selected option for Level 1 
	int selected_L2_option;         // Selected option for Level 2
	int selected_L3_option;         // Selected option for Level 3
	int selected_fruit_option;      // 0-7 for the 8 options
	int fruit_scroll_offset;        // Track scroll position (0-4)
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
						
			canvas_set_font_direction(canvas, CanvasDirectionBottomToTop); // Set text rotation to 90 degrees 
			canvas_draw_str(canvas, 128, 45, "2025-11");		
			canvas_set_font_direction(canvas, CanvasDirectionLeftToRight); // Reset to normal text direction
			
			canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Hold 'back'");
			canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "to exit.");
			
			canvas_draw_str_aligned(canvas, 100, 54, AlignLeft, AlignTop, "F. Greil");
			canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.5");
			
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
		case ScreenL1_A3: // Level 2: Option 3: No leave type chosen. Fruit identification with scrolling
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
			// canvas_draw_icon(canvas, 118, 1, &I_IconNotReady_10x10); 	
            canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "My tree's fruit are..");
            canvas_draw_frame(canvas, 1, 11, 127, 44); // Menu box for 4 visible items
			canvas_set_font(canvas, FontSecondary);
			const char* fruit_options[8] = {
				"not present.",
				"hard nut with shell",
				"winged fruit (samara)",
				"fleshy w. hard stone",
				"woody cone",
				"soft berries",
				"fleshy, w. seeds inside",
				"something else"
			};
			// Calculate which 4 items to display
			int display_start = state->fruit_scroll_offset;
			int display_end = display_start + 4;
			
			// Write the 4 visible options onto the screen
			canvas_set_font(canvas, FontSecondary);
			for(int i = 0; i < 4 && (display_start + i) < 8; i++) {
				int option_index = display_start + i;
				canvas_draw_str(canvas, 7, 21 + (i * 10), fruit_options[option_index]);
			}
			
			// Draw selection indicator at the correct position
			if(state->selected_fruit_option >= display_start && 
			   state->selected_fruit_option < display_end) {
				int indicator_line = state->selected_fruit_option - display_start;
				canvas_draw_str(canvas, 3, 21 + (indicator_line * 10), ">");
			}
			canvas_draw_icon(canvas, 119, 44, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 49, &I_ButtonDown_7x4);					
			elements_button_left(canvas, "Prev. ?"); 
			elements_button_center(canvas, "OK"); // for the OK button
			break;
		case ScreenL2_A1_1: // Level 2: Option 1_1: Needles
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1 				
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Where are needles?");
            canvas_draw_frame(canvas, 1, 11, 127, 34); // Menu box: x, y, w, h
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str(canvas, 7, 21, "Only on long shoots");
            canvas_draw_str(canvas, 7, 31, "On short shoots");
            canvas_draw_str(canvas, 7, 41, "Both on long& short shoots");
			// Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L3_option* 10), ">");
			// Navigation hints		
			canvas_draw_icon(canvas, 119, 34, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 39, &I_ButtonDown_7x4);	
			elements_button_left(canvas, "Prev.?"); 
			elements_button_center(canvas, "OK");
			break;
		case ScreenL2_A1_2: // Level 2: Option 1_2: Scales
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Scales");
            canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop, "Cypress, Ccedar, Giant sequoia, Redwood, Juniper (most species), Arbor vitae, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL2_A2_1: // Level 2: Option 2_1: Compound leave
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Leaflets' arrangement?");
            canvas_draw_frame(canvas, 1, 11, 127, 34); // Menu box: x, y, w, h
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str(canvas, 7, 21, "Odd #, single terminal leaflet");
            canvas_draw_str(canvas, 7, 31, "Even #, no terminal leaflet");
            canvas_draw_str(canvas, 7, 41, "Leaflets radiate from point");
			// Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L3_option* 10), ">");
			// Navigation hints		
			canvas_draw_icon(canvas, 119, 34, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 39, &I_ButtonDown_7x4);	
			elements_button_left(canvas, "Prev.?"); 
			elements_button_center(canvas, "OK");
			break;
		case ScreenL2_A2_2: // Level 2: Option 2_2: Undivided leaf
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1 	
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Type of leaf margin?");
            canvas_draw_frame(canvas, 1, 11, 127, 34); // Menu box: x, y, w, h
			canvas_set_font(canvas, FontSecondary);
			canvas_draw_str(canvas, 7, 21, "Big rounded o.pointed bumps");
            canvas_draw_str(canvas, 7, 31, "Leaf margin is smooth");
            canvas_draw_str(canvas, 7, 41, "Small teeth or cuts");
			// Draw selection indicator
            canvas_draw_str(canvas, 3, 21 + (state->selected_L3_option* 10), ">");
			// Navigation hints		
			canvas_draw_icon(canvas, 119, 34, &I_ButtonUp_7x4);
			canvas_draw_icon(canvas, 119, 39, &I_ButtonDown_7x4);	
			elements_button_left(canvas, "Prev.?"); 
			elements_button_center(canvas, "OK");
			break;
		case ScreenL3_A1_1_1: // Level 3: Needles only on long shoots
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "On long shoots");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Fir, Spruce, Hemlock, Yew.");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A1_1_2: // Level 3: Needles on short shoots
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "On short shoots");
            canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Larch, Cedar.");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A1_1_3: // Level 3: Needles on long and short shoots
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_NeedleTree_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "On all shoots");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Pines.");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_1_1: // Level 3: Odd-pinnate leaves / unpaarig gefiedert
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_Imparipinnate_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Odd-pinnate leaves");
            canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "European ash, Rowan, Common walnut, Elder, False acacia, Black walnut, Hickory, Sumac, Yellowwood, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_1_2: // Level 3: Even-pinnate leaves / paarig gefiedert
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); 
			canvas_draw_icon(canvas, 118, 1, &I_Paripinnate_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Even-pinnate leaves");
            canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Locust, Carob tree, Laburnum, Mimosa, Judas tree, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_1_3: // Level 3: Palmate leaves / gefingert
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_Palmate_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Palmate leaves");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Horse chestnut, Maple, Sycamore, Vine, Fig, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_2_1: // Level 3: Lobed leaves / Gelappte Laubblätter
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_Lobed_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Lobed leaf");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Oak, Hawthorn, White polar, London plane, Mulberry, Ginkgo, Sassafras, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_2_2: // Level 3: Entire leaves / Ganzrandige Laubblätter
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_Entire_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Entire leaf");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "European beech, Common box, Laurel, Holly, Magnolia, Olive, Pear, Dogwood, ..)");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case ScreenL3_A2_2_3: // Level 3: Serrated or notched leaves
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			canvas_draw_icon(canvas, 118, 1, &I_Notched_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Notched leaf");
            canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas,1,11,AlignLeft,AlignTop,
			  "Silver birch, European hornbeam, Com.hazel, Elm, Sweet chestnut, Alder, Aspen, Apple, Cherry, Plum, ..");
			elements_button_left(canvas, "Prev.?"); 
			break;
		case Screen_Fruit_Unknown:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Unknown fruit");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Nut:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Nut");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Samara:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Samara");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Drupe:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Drupe");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Cone:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Cone");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Berry:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Berry");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Pome:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Pome");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;
		case Screen_Fruit_Other:
			canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
			// canvas_draw_icon(canvas, 118, 1, &I_TBD_10x10);
			canvas_set_font(canvas, FontPrimary);
			canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Other fruit");
			canvas_set_font(canvas, FontSecondary);
			elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop,
				"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
			elements_button_left(canvas, "Prev.?");
			break;		
    }
}

void input_callback(InputEvent* event, void* context) {
    AppState* app = context;
	// Put the input event into the message queue for processing
    // Timeout of 0 means non-blocking
    furi_message_queue_put(app->input_queue, event, 0);
}

int32_t tree_ident_main(void* p) {
    UNUSED(p);

    AppState app; // Application state struct
	app.current_screen = ScreenSplash;  // Start on first screen (0)
	app.selected_L1_option= 0; // Level 1: Start with first line highlighted
	app.selected_L2_option= 0; // Level 2: Start with first line highlighted
	app.selected_L3_option= 0; // Level 3: Start with first line highlighted
	app.selected_fruit_option = 0;
	app.fruit_scroll_offset = 0;	

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
			if((app.current_screen == ScreenL2_A1_1 || app.current_screen == ScreenL2_A2_1 || app.current_screen == ScreenL2_A2_2)
				&& input.type == InputTypePress) {
				app.selected_L3_option= (app.selected_L3_option- 1 + 3) % 3;
			}
			if(app.current_screen == ScreenL1_A3 && input.type == InputTypePress) {  // Handle fruit menu scrolling
				if(app.selected_fruit_option > 0) {
					app.selected_fruit_option--;	
					if(app.selected_fruit_option < app.fruit_scroll_offset) { // Adjust scroll if selection goes above visible area
						app.fruit_scroll_offset = app.selected_fruit_option;
					}
				}
			}
			break;
		case InputKeyDown:
			if(app.current_screen == ScreenL1Question && input.type == InputTypePress) {
				app.selected_L1_option= (app.selected_L1_option+ 1) % 3;
			}
			if((app.current_screen == ScreenL1_A1 || app.current_screen == ScreenL1_A2)
				&& input.type == InputTypePress) {
				app.selected_L2_option= (app.selected_L2_option+ 1) % 2;
			}
			if((app.current_screen == ScreenL2_A1_1 || app.current_screen == ScreenL2_A2_1 || app.current_screen == ScreenL2_A2_2)
				&& input.type == InputTypePress) {
				app.selected_L3_option= (app.selected_L3_option+ 1) % 3;
			}
			// NEW: Handle fruit menu scrolling
			if(app.current_screen == ScreenL1_A3 && input.type == InputTypePress) {
				if(app.selected_fruit_option < 7) {  // 0-7 = 8 options
					app.selected_fruit_option++;
					// Adjust scroll if selection goes below visible area
					if(app.selected_fruit_option >= app.fruit_scroll_offset + 4) {
						app.fruit_scroll_offset = app.selected_fruit_option - 3;
					}
				}
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
					case ScreenL1_A3:  // Fruit type selection
					switch (app.selected_fruit_option) {
						case 0: app.current_screen = Screen_Fruit_Unknown; break;
						case 1: app.current_screen = Screen_Fruit_Nut; break;
						case 2: app.current_screen = Screen_Fruit_Samara; break;
						case 3: app.current_screen = Screen_Fruit_Drupe; break;
						case 4: app.current_screen = Screen_Fruit_Cone; break;
						case 5: app.current_screen = Screen_Fruit_Berry; break;
						case 6: app.current_screen = Screen_Fruit_Pome; break;
						case 7: app.current_screen = Screen_Fruit_Other; break;
					}
                break;
					case ScreenL2_A1_1: // Where are the needles?
						switch (app.selected_L3_option){
							case 0: app.current_screen = ScreenL3_A1_1_1; break;
							case 1: app.current_screen = ScreenL3_A1_1_2; break;
							case 2: app.current_screen = ScreenL3_A1_1_3; break;
						}
						break;
					case ScreenL2_A2_1: // How are leaflets arranged?
						switch (app.selected_L3_option){
							case 0: app.current_screen = ScreenL3_A2_1_1; break;
							case 1: app.current_screen = ScreenL3_A2_1_2; break;
							case 2: app.current_screen = ScreenL3_A2_1_3; break;
						}
						break;
					case ScreenL2_A2_2: // Type of leaf margin?
						switch (app.selected_L3_option){
							case 0: app.current_screen = ScreenL3_A2_2_1; break;
							case 1: app.current_screen = ScreenL3_A2_2_2; break;
							case 2: app.current_screen = ScreenL3_A2_2_3; break;
						}
						break;
					case Screen_Fruit_Unknown:
					case Screen_Fruit_Nut:
					case Screen_Fruit_Samara:
					case Screen_Fruit_Drupe:
					case Screen_Fruit_Cone:
					case Screen_Fruit_Berry:
					case Screen_Fruit_Pome:
					case Screen_Fruit_Other:
						if(input.type == InputTypePress) {
							app.current_screen = ScreenL1Question;
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
				case ScreenL2_A1_1:
				case ScreenL2_A2_1:
				case ScreenL2_A2_2:  
				case ScreenL3_A1_1_1:
				case ScreenL3_A1_1_2:
				case ScreenL3_A1_1_3:
				case ScreenL3_A2_1_1:
				case ScreenL3_A2_1_2:
				case ScreenL3_A2_1_3:
				case ScreenL3_A2_2_2:
				case ScreenL3_A2_2_1:
				case ScreenL3_A2_2_3: 
					if(input.type == InputTypePress) {
						app.current_screen = ScreenL1Question;
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
					case ScreenL3_A1_1_1:
					case ScreenL3_A1_1_2:
					case ScreenL3_A1_1_3:
						app.current_screen = ScreenL2_A1_1;
						break;
					case ScreenL3_A2_1_1:
					case ScreenL3_A2_1_2:
					case ScreenL3_A2_1_3:
						app.current_screen = ScreenL2_A2_1;
						break;
					case ScreenL3_A2_2_1:
					case ScreenL3_A2_2_2:
					case ScreenL3_A2_2_3:
						app.current_screen = ScreenL2_A2_2;
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
