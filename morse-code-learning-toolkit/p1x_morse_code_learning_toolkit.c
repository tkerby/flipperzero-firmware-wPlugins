#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <string.h>

#define DOT_DURATION_MS 100
#define DASH_DURATION_MS 300
#define ELEMENT_SPACE_MS 100
#define CHAR_SPACE_MS 300
#define MAX_MORSE_LENGTH 16
#define DEFAULT_FREQUENCY 800

typedef enum {
    MorseStateMenu,
    MorseStateLearn,
    MorseStatePractice,
    MorseStateHelp,
    MorseStateExit
} MorseAppState;

typedef struct {
    char character;
    const char* code;
} MorseCode;

typedef struct {
    // UI elements
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    
    // Application state
    MorseAppState app_state;
    int menu_selection;
    bool is_running;
    
    // Learning
    char current_char;
    char user_input[MAX_MORSE_LENGTH];
    int input_position;
} MorseApp;

// International Morse Code mappings (simplified subset)
static const MorseCode MORSE_TABLE[] = {
    {'A', ".-"},
    {'B', "-..."},
    {'C', "-.-."},
    {'D', "-.."},
    {'E', "."},
    {'F', "..-."},
    {'G', "--."},
    {'H', "...."},
    {'I', ".."},
    {'J', ".---"},
    {'K', "-.-"},
    {'L', ".-.."},
    {'M', "--"},
    {'N', "-."},
    {'O', "---"},
    {'P', ".--."},
    {'Q', "--.-"},
    {'R', ".-."},
    {'S', "..."},
    {'T', "-"},
    {'U', "..-"},
    {'V', "...-"},
    {'W', ".--"},
    {'X', "-..-"},
    {'Y', "-.--"},
    {'Z', "--.."},
    {'1', ".----"},
    {'2', "..---"},
    {'3', "...--"},
    {'4', "....-"},
    {'5', "....."},
};

static void morse_app_draw_callback(Canvas* canvas, void* ctx);
static void morse_app_input_callback(InputEvent* input_event, void* ctx);
static void play_dot(MorseApp* app);
static void play_dash(MorseApp* app);
static void play_character(MorseApp* app, char ch);
static const char* get_morse_for_char(char c);

// Entry point for Morse Master application
int32_t p1x_morse_code_learning_toolkit_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("MorseMaster", "Application starting");
    
    MorseApp* app = malloc(sizeof(MorseApp));
    if(!app) {
        FURI_LOG_E("MorseMaster", "Failed to allocate memory");
        return 255;
    }
    
    // Initialize application structure
    memset(app, 0, sizeof(MorseApp));
    
    // Allocate required resources
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Check if all resources were allocated
    if(!app->view_port || !app->event_queue) {
        FURI_LOG_E("MorseMaster", "Failed to allocate resources");
        if(app->view_port) view_port_free(app->view_port);
        if(app->event_queue) furi_message_queue_free(app->event_queue);
        if(app->gui) furi_record_close(RECORD_GUI);
        if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return 255;
    }
    
    // Set up default values
    app->app_state = MorseStateMenu;
    app->menu_selection = 0;
    app->is_running = true;
    app->current_char = 'E'; // Start with simplest character
    app->input_position = 0;
    memset(app->user_input, 0, sizeof(app->user_input));
    
    // Configure viewport
    view_port_draw_callback_set(app->view_port, morse_app_draw_callback, app);
    view_port_input_callback_set(app->view_port, morse_app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    // Seed RNG
    srand(furi_hal_rtc_get_timestamp());
    
    // Main event loop
    while(app->is_running) {
        InputEvent event;
        // Wait for input with a timeout
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            morse_app_input_callback(&event, app);
        }
        view_port_update(app->view_port);
        furi_delay_ms(5); // Small delay to prevent CPU hogging
    }
    
    // Free resources
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    
    return 0;
}

// Draw application UI based on current state
static void morse_app_draw_callback(Canvas* canvas, void* ctx) {
    MorseApp* app = ctx;
    if(!app || !canvas) return;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    switch(app->app_state) {
        case MorseStateMenu: {
            // Draw menu title
            canvas_draw_str(canvas, 2, 10, "MORSE MASTER");
            canvas_draw_line(canvas, 0, 12, 128, 12);
            
            // Draw menu items
            const char* menu_items[] = {
                "Learn Morse",
                "Practice",
                "Help",
                "Exit"
            };
            
            for(int i = 0; i < 4; i++) {
                canvas_draw_str(canvas, 10, 25 + i * 10, menu_items[i]);
                if(app->menu_selection == i) {
                    canvas_draw_str(canvas, 2, 25 + i * 10, ">");
                }
            }
            break;
        }
        
        case MorseStateLearn: {
            // Draw learning screen
            canvas_draw_str(canvas, 2, 10, "LEARN MORSE");
            canvas_draw_line(canvas, 0, 12, 128, 12);
            
            // Display current character
            char txt[32];
            snprintf(txt, sizeof(txt), "Character: %c", app->current_char);
            canvas_draw_str(canvas, 5, 25, txt);
            
            // Display Morse code
            const char* morse = get_morse_for_char(app->current_char);
            snprintf(txt, sizeof(txt), "Morse: %s", morse ? morse : "");
            canvas_draw_str(canvas, 5, 37, txt);
            
            // Display instructions
            canvas_draw_str(canvas, 5, 55, "OK: Play sound");
            canvas_draw_str(canvas, 5, 64, "DOWN: Next char");
            break;
        }
        
        case MorseStatePractice: {
            // Draw practice screen
            canvas_draw_str(canvas, 2, 10, "PRACTICE");
            canvas_draw_line(canvas, 0, 12, 128, 12);
            
            // Display morse code
            canvas_draw_str(canvas, 5, 25, "Morse Code:");
            canvas_draw_str(canvas, 5, 37, app->user_input);
            
            // Display instructions
            canvas_draw_str(canvas, 5, 55, "OK: Dot   DOWN: Dash");
            canvas_draw_str(canvas, 5, 64, "LEFT: Space  RIGHT: Clear");
            break;
        }
        
        case MorseStateHelp: {
            // Draw help screen
            canvas_draw_str(canvas, 2, 10, "MORSE HELP");
            canvas_draw_line(canvas, 0, 12, 128, 12);
            
            canvas_draw_str(canvas, 5, 24, "Morse Code Basics:");
            canvas_draw_str(canvas, 5, 36, "· = short press (dot)");
            canvas_draw_str(canvas, 5, 48, "— = long press (dash)");
            canvas_draw_str(canvas, 5, 60, "BACK: Return to menu");
            break;
        }
        
        default:
            break;
    }
}

// Handle user input
static void morse_app_input_callback(InputEvent* input_event, void* ctx) {
    MorseApp* app = ctx;
    if(!app || !input_event) return;
    
    // Only handle press/hold events
    if(input_event->type != InputTypeShort && input_event->type != InputTypeLong) {
        return;
    }
    
    switch(app->app_state) {
        case MorseStateMenu:
            if(input_event->key == InputKeyUp && input_event->type == InputTypeShort) {
                app->menu_selection = (app->menu_selection > 0) ? 
                    app->menu_selection - 1 : 3;
            }
            else if(input_event->key == InputKeyDown && input_event->type == InputTypeShort) {
                app->menu_selection = (app->menu_selection < 3) ? 
                    app->menu_selection + 1 : 0;
            }
            else if(input_event->key == InputKeyOk && input_event->type == InputTypeShort) {
                switch(app->menu_selection) {
                    case 0: // Learn
                        app->app_state = MorseStateLearn;
                        break;
                        
                    case 1: // Practice
                        app->app_state = MorseStatePractice;
                        memset(app->user_input, 0, sizeof(app->user_input));
                        break;
                        
                    case 2: // Help
                        app->app_state = MorseStateHelp;
                        break;
                        
                    case 3: // Exit
                        app->is_running = false;
                        break;
                }
            }
            else if(input_event->key == InputKeyBack && input_event->type == InputTypeShort) {
                app->is_running = false;
            }
            break;
            
        case MorseStateLearn:
            if(input_event->key == InputKeyOk && input_event->type == InputTypeShort) {
                // Play the character's morse code
                play_character(app, app->current_char);
            }
            else if(input_event->key == InputKeyDown && input_event->type == InputTypeShort) {
                // Show next character
                if(app->current_char == 'Z') 
                    app->current_char = 'A';
                else if(app->current_char == '5')
                    app->current_char = 'A';
                else if(app->current_char >= 'A' && app->current_char < 'Z')
                    app->current_char++;
                else
                    app->current_char = '1';
            }
            else if(input_event->key == InputKeyUp && input_event->type == InputTypeShort) {
                // Show previous character
                if(app->current_char == 'A')
                    app->current_char = '5';
                else if(app->current_char == '1')
                    app->current_char = 'Z';
                else if(app->current_char > 'A' && app->current_char <= 'Z')
                    app->current_char--;
                else
                    app->current_char = '5';
            }
            else if(input_event->key == InputKeyBack && input_event->type == InputTypeShort) {
                app->app_state = MorseStateMenu;
            }
            break;
            
        case MorseStatePractice:
            if(input_event->key == InputKeyOk && input_event->type == InputTypeShort) {
                // Add dot to input if there's room
                if(strlen(app->user_input) < MAX_MORSE_LENGTH - 1) {
                    app->user_input[app->input_position++] = '.';
                    app->user_input[app->input_position] = '\0';
                    play_dot(app);
                }
            }
            else if(input_event->key == InputKeyDown && 
                   (input_event->type == InputTypeShort || input_event->type == InputTypeLong)) {
                // Add dash to input if there's room
                if(strlen(app->user_input) < MAX_MORSE_LENGTH - 1) {
                    app->user_input[app->input_position++] = '-';
                    app->user_input[app->input_position] = '\0';
                    play_dash(app);
                }
            }
            else if(input_event->key == InputKeyLeft && input_event->type == InputTypeShort) {
                // Add space if there's room
                if(strlen(app->user_input) < MAX_MORSE_LENGTH - 1) {
                    app->user_input[app->input_position++] = ' ';
                    app->user_input[app->input_position] = '\0';
                }
            }
            else if(input_event->key == InputKeyRight && input_event->type == InputTypeShort) {
                // Clear input
                memset(app->user_input, 0, sizeof(app->user_input));
                app->input_position = 0;
            }
            else if(input_event->key == InputKeyBack && input_event->type == InputTypeShort) {
                app->app_state = MorseStateMenu;
            }
            break;
            
        case MorseStateHelp:
            if(input_event->key == InputKeyBack && input_event->type == InputTypeShort) {
                app->app_state = MorseStateMenu;
            }
            break;
            
        default:
            break;
    }
    
    view_port_update(app->view_port);
}

// Play dot sound and visual
static void play_dot(MorseApp* app) {
    furi_hal_speaker_start(DEFAULT_FREQUENCY, 1.0f);
    notification_message(app->notifications, &sequence_set_only_red_255);
    furi_delay_ms(DOT_DURATION_MS);
    furi_hal_speaker_stop();
    notification_message(app->notifications, &sequence_reset_red);
    furi_delay_ms(ELEMENT_SPACE_MS);
}

// Play dash sound and visual
static void play_dash(MorseApp* app) {
    furi_hal_speaker_start(DEFAULT_FREQUENCY, 1.0f);
    notification_message(app->notifications, &sequence_set_only_blue_255);
    furi_delay_ms(DASH_DURATION_MS);
    furi_hal_speaker_stop();
    notification_message(app->notifications, &sequence_reset_blue);
    furi_delay_ms(ELEMENT_SPACE_MS);
}

// Play a complete morse character
static void play_character(MorseApp* app, char ch) {
    const char* morse = get_morse_for_char(ch);
    if(!morse) return;
    
    for(size_t i = 0; morse[i] != '\0'; i++) {
        if(morse[i] == '.') {
            play_dot(app);
        } else if(morse[i] == '-') {
            play_dash(app);
        }
    }
    
    furi_delay_ms(CHAR_SPACE_MS);
}

// Get morse code for a character
static const char* get_morse_for_char(char c) {
    c = toupper(c);
    
    for(size_t i = 0; i < sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]); i++) {
        if(MORSE_TABLE[i].character == c) {
            return MORSE_TABLE[i].code;
        }
    }
    
    return NULL;
}
