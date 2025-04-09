#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <furi_hal_speaker.h>
#include <string.h>

#define DOT_DURATION_MS 100
#define DASH_DURATION_MS 300
#define ELEMENT_SPACE_MS 100
#define CHAR_SPACE_MS 300
#define MAX_MORSE_LENGTH 16
#define DEFAULT_FREQUENCY 800

// Application states
typedef enum {
    MorseStateMenu,
    MorseStateLearn,
    MorseStatePractice,
    MorseStateHelp,
    MorseStateExit
} MorseAppState;

// Sound command types
typedef enum {
    SoundCommandNone,
    SoundCommandDot,
    SoundCommandDash,
    SoundCommandCharacter
} SoundCommand;

// Morse code structure
typedef struct {
    char character;
    const char* code;
} MorseCode;

// Main application structure
typedef struct {
    // UI elements
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    
    // Sound processing
    FuriThread* sound_thread;
    FuriMessageQueue* sound_queue;
    bool sound_running;
    SoundCommand current_sound;
    char sound_character;
    
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

// Function prototypes
static void morse_app_draw_callback(Canvas* canvas, void* ctx);
static void morse_app_input_callback(InputEvent* input_event, void* ctx);
static void play_dot(MorseApp* app);
static void play_dash(MorseApp* app);
static void play_character(MorseApp* app, char ch);
static const char* get_morse_for_char(char c);
static int32_t sound_worker_thread(void* context);

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

// Sound worker thread function - handles all audio output
static int32_t sound_worker_thread(void* context) {
    MorseApp* app = (MorseApp*)context;
    SoundCommand command;
    
    while(app->sound_running) {
        // Wait for a sound command
        if(furi_message_queue_get(app->sound_queue, &command, 100) == FuriStatusOk) {
            // Process sound command
            switch(command) {
                case SoundCommandDot:
                    // Play dot
                    if(furi_hal_speaker_acquire(1000)) {
                        furi_hal_speaker_start(DEFAULT_FREQUENCY, 1.0f);
                        notification_message(app->notifications, &sequence_set_only_red_255);
                        furi_delay_ms(DOT_DURATION_MS);
                        furi_hal_speaker_stop();
                        notification_message(app->notifications, &sequence_reset_red);
                        furi_hal_speaker_release();
                    }
                    furi_delay_ms(ELEMENT_SPACE_MS);
                    break;
                    
                case SoundCommandDash:
                    // Play dash
                    if(furi_hal_speaker_acquire(1000)) {
                        furi_hal_speaker_start(DEFAULT_FREQUENCY, 1.0f);
                        notification_message(app->notifications, &sequence_set_only_blue_255);
                        furi_delay_ms(DASH_DURATION_MS);
                        furi_hal_speaker_stop();
                        notification_message(app->notifications, &sequence_reset_blue);
                        furi_hal_speaker_release();
                    }
                    furi_delay_ms(ELEMENT_SPACE_MS);
                    break;
                    
                case SoundCommandCharacter:
                    // Play a full character
                    {
                        const char* morse = get_morse_for_char(app->sound_character);
                        if(morse) {
                            for(size_t i = 0; morse[i] != '\0'; i++) {
                                if(morse[i] == '.') {
                                    // Send dot sound command
                                    SoundCommand dot_cmd = SoundCommandDot;
                                    furi_message_queue_put(app->sound_queue, &dot_cmd, 0);
                                    furi_delay_ms(DOT_DURATION_MS + ELEMENT_SPACE_MS);
                                } else if(morse[i] == '-') {
                                    // Send dash sound command
                                    SoundCommand dash_cmd = SoundCommandDash;
                                    furi_message_queue_put(app->sound_queue, &dash_cmd, 0);
                                    furi_delay_ms(DASH_DURATION_MS + ELEMENT_SPACE_MS);
                                }
                            }
                        }
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        furi_delay_ms(10);
    }
    
    return 0;
}

// Redefined sound functions that queue sound commands instead of playing directly

// Play dot sound and visual
static void play_dot(MorseApp* app) {
    SoundCommand cmd = SoundCommandDot;
    furi_message_queue_put(app->sound_queue, &cmd, 0);
}

// Play dash sound and visual
static void play_dash(MorseApp* app) {
    SoundCommand cmd = SoundCommandDash;
    furi_message_queue_put(app->sound_queue, &cmd, 0);
}

// Play a complete morse character
static void play_character(MorseApp* app, char ch) {
    app->sound_character = ch;
    SoundCommand cmd = SoundCommandCharacter;
    furi_message_queue_put(app->sound_queue, &cmd, 0);
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
    app->sound_queue = furi_message_queue_alloc(8, sizeof(SoundCommand));
    
    // Check if all resources were allocated
    if(!app->view_port || !app->event_queue || !app->sound_queue) {
        FURI_LOG_E("MorseMaster", "Failed to allocate resources");
        if(app->view_port) view_port_free(app->view_port);
        if(app->event_queue) furi_message_queue_free(app->event_queue);
        if(app->sound_queue) furi_message_queue_free(app->sound_queue);
        if(app->gui) furi_record_close(RECORD_GUI);
        if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
        free(app);
        return 255;
    }
    
    // Set up default values
    app->app_state = MorseStateMenu;
    app->menu_selection = 0;
    app->is_running = true;
    app->sound_running = true;
    app->current_char = 'E'; // Start with simplest character
    app->input_position = 0;
    memset(app->user_input, 0, sizeof(app->user_input));
    
    // Configure viewport
    view_port_draw_callback_set(app->view_port, morse_app_draw_callback, app);
    view_port_input_callback_set(app->view_port, morse_app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    // Create and start sound worker thread
    app->sound_thread = furi_thread_alloc_ex("MorseSoundWorker", 1024, sound_worker_thread, app);
    furi_thread_start(app->sound_thread);
    
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
    
    // Signal sound thread to stop and wait for it to finish
    app->sound_running = false;
    furi_thread_join(app->sound_thread);
    furi_thread_free(app->sound_thread);
    
    // Free resources
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_message_queue_free(app->sound_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    
    return 0;
}
