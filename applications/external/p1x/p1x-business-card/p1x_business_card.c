#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef struct {
    FuriMutex* mutex;
} AppState;

// Draw callback that renders the UI
static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Cyfrowy Nomada");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, "Krzysztof Krystian Jankowski");
    canvas_draw_str(canvas, 2, 35, "TEL: +48 501-135-709");
    canvas_draw_str(canvas, 2, 45, "NIP: 7822277719");
    canvas_draw_str(canvas, 2, 60, "http://nomada.p1x.in");

    
    furi_mutex_release(state->mutex);
}

// Input callback for button handling
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Application entry point
int32_t p1x_business_card_main(void* p) {
    UNUSED(p);
    
    // Create UI elements
    AppState* state = malloc(sizeof(AppState));
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    // Register callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    
    // Create GUI and register view port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    // Main event loop
    InputEvent event;
    bool running = true;
    
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
    }
    
    // Cleanup
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(state->mutex);
    free(state);
    
    return 0;
}
