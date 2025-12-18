// Fart Sound Generator for Flipper Zero
// Application ID: mitzi_windbreak
// Entry point: fart_main

#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <notification/notification_messages.h>
#include <furi_hal_speaker.h> // sound output
#include <gui/elements.h> // to access button drawing functions
#include "windbreak_icons.h" // Custom icon definitions

#define TAG "Windbreak"

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} FartEvent;

typedef struct {
    uint8_t wet_dry;      // 1-5: 1=wet, 5=dry
    uint8_t length;       // 1-5: 1=short, 5=long
    uint8_t pitch;        // 1-5: 1=low, 5=high
    uint8_t bubbliness;   // 1-5: 1=smooth, 5=bubbly
    uint8_t selected;     // Currently selected parameter (0-3)
    bool playing;         // Is sound currently playing
} FartState;

typedef struct {
    FartState* state;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
} FartApp;

// Convert parameter value to frequency
static float get_base_frequency(uint8_t pitch) {
    // Logarithmic scale from 16 Hz to 16768 Hz
    // pitch 1->16Hz, 2->63Hz, 3->250Hz, 4->1000Hz, 5->4000Hz (approx)
    return 16.0f * powf(4.0f, (pitch - 1));
}

// Get duration in milliseconds
static uint32_t get_duration_ms(uint8_t length) {
    return 100 + (length - 1) * 200; // 100ms to 900ms
}

// Play a fart sound with given parameters
static void play_fart(FartState* state, NotificationApp* notifications) {
    if(!furi_hal_speaker_acquire(1000)) {
        FURI_LOG_E(TAG, "Could not acquire speaker");
        return;
    }

    state->playing = true;
	notification_message(notifications, &sequence_set_red_255); // turn on status LED
    
    float base_freq = get_base_frequency(state->pitch);
    uint32_t duration = get_duration_ms(state->length);
    float volume = 0.8f;  // Fixed volume at 80%
    
    // Wetness affects frequency variation (wet = more wobble)
    float wet_variation = (6 - state->wet_dry) * 10.0f;
    
    // Bubbliness affects how often we change frequency
    uint32_t bubble_interval = 50 - (state->bubbliness - 1) * 8; // 50ms to 18ms
    
    uint32_t start_time = furi_get_tick();
    uint32_t elapsed = 0;
    uint32_t last_bubble = 0;
    
    float current_freq = base_freq;
    
    while(elapsed < duration) {
        elapsed = furi_get_tick() - start_time;
        
        // Apply envelope (fade in/out)
        float envelope = 1.0f;
        if(elapsed < 50) {
            envelope = (float)elapsed / 50.0f; // Attack
        } else if(elapsed > duration - 100) {
            envelope = (float)(duration - elapsed) / 100.0f; // Release
        }
        
        // Change frequency for bubbliness
        if(elapsed - last_bubble > bubble_interval) {
            // Random frequency variation
            uint32_t random_val = random() % 100;
            float variation = ((float)random_val / 100.0f - 0.5f) * 2.0f;
            current_freq = base_freq + (variation * wet_variation);
            
            // Add some pitch drift downward over time (natural fart behavior)
            float drift = (float)elapsed / (float)duration * 20.0f;
            current_freq -= drift;
            
            // Clamp frequency to reasonable range
            if(current_freq < 30.0f) current_freq = 30.0f;
            if(current_freq > 500.0f) current_freq = 500.0f;
            
            last_bubble = elapsed;
        }
        
        // Play tone with current frequency and envelope
        furi_hal_speaker_start(current_freq, volume * envelope);
        
        furi_delay_ms(5); // Small delay for tone stability
    }
    
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    notification_message(notifications, &sequence_reset_rgb); // Turn of status LED
	state->playing = false;
}

// Render callback for GUI
static void fart_render_callback(Canvas* canvas, void* ctx) {
    FartApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
	canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);	
	canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Windbreak");
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.1");
	
	canvas_set_font_direction(canvas, CanvasDirectionBottomToTop); // Set text rotation to 90 degrees 
	canvas_draw_str(canvas, 128, 47, "2025-12");		
	canvas_set_font_direction(canvas, CanvasDirectionLeftToRight); // Reset to normal text direction
	canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "Back: exit");
	canvas_draw_str_aligned(canvas, 99, 57, AlignLeft, AlignTop, "f418.eu");
	elements_button_center(canvas, "Fart");
	
    char buffer[64];
    uint8_t y = 19;
    
    // Draw parameters
    const char* params[] = {"Wet/Dry", "Length", "Pitch", "Bubbles"};
    uint8_t values[] = {
        app->state->wet_dry,
        app->state->length,
        app->state->pitch,
        app->state->bubbliness
    };
    // Render parameter name and value
    for(uint8_t i = 0; i < 4; i++) {
        snprintf(buffer, sizeof(buffer), "%s: %d", params[i], values[i]);
        
        if(app->state->selected == i) {
            canvas_draw_str(canvas, 0, y, ">");
        }
        
        canvas_draw_str(canvas, 10, y, buffer);
        
        // Draw bar indicators to display the values of the various parameters
        uint8_t bar_x = 66;
		uint8_t bar_y = 7; 
        uint8_t bar_width = values[i] * 10;
        canvas_draw_frame(canvas, bar_x, y - bar_y - 1, 52, 8);
        canvas_draw_box(canvas, bar_x + 1, y - bar_y, bar_width, 6);
        
        y += 10;
    }
       
    furi_mutex_release(app->mutex);
}

// Input callback
static void fart_input_callback(InputEvent* input_event, void* ctx) {
    FartApp* app = ctx;
    FartEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// Main application entry point
int32_t fart_main(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "Windbreak fart generator starting...");
    
    // Allocate app structure
    FartApp* app = malloc(sizeof(FartApp));
    app->state = malloc(sizeof(FartState));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(FartEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    // Initialize default state
    app->state->wet_dry = 3;
    app->state->length = 3;
    app->state->pitch = 2;
    app->state->bubbliness = 3;
    app->state->selected = 0;
    app->state->playing = false;
    
    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, fart_render_callback, app);
    view_port_input_callback_set(view_port, fart_input_callback, app);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    FURI_LOG_I(TAG, "GUI initialized, entering main loop");
    
    // Main event loop
    FartEvent event;
    bool running = true;
    
    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress || event.input.type == InputTypeRepeat) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(app->state->selected > 0) {
                            app->state->selected--;
                        }
                        break;
                        
                    case InputKeyDown:
                        if(app->state->selected < 3) {
                            app->state->selected++;
                        }
                        break;
                        
                    case InputKeyRight:
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0: param = &app->state->wet_dry; break;
                            case 1: param = &app->state->length; break;
                            case 2: param = &app->state->pitch; break;
                            case 3: param = &app->state->bubbliness; break;
                            }
                            if(param && *param < 5) (*param)++;
                        }
                        break;
                        
                    case InputKeyLeft:
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0: param = &app->state->wet_dry; break;
                            case 1: param = &app->state->length; break;
                            case 2: param = &app->state->pitch; break;
                            case 3: param = &app->state->bubbliness; break;
                            }
                            if(param && *param > 1) (*param)--;
                        }
                        break;
                        
                    case InputKeyOk:
                        if(!app->state->playing) {
                            FURI_LOG_I(TAG, "Playing fart: W/D=%d L=%d P=%d B=%d",
                                app->state->wet_dry, app->state->length, app->state->pitch,
                                app->state->bubbliness);
                            // Play fart in separate thread to not block UI
                            furi_mutex_release(app->mutex);
                            play_fart(app->state, app-> notifications);
                            furi_mutex_acquire(app->mutex, FuriWaitForever);
                        }
                        break;
                        
                    case InputKeyBack:
                        FURI_LOG_I(TAG, "Exit requested");
                        running = false;
                        break;
                        
                    default:
                        break;
                    }
                    
                    furi_mutex_release(app->mutex);
                    view_port_update(view_port);
                }
            }
        }
    }
    
    FURI_LOG_I(TAG, "Cleaning up...");
    
    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app->state);
    free(app);
    
    FURI_LOG_I(TAG, "Windbreak fart generator terminated");
    
    return 0;
}
