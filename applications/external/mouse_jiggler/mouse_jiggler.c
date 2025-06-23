#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#define MOUSE_MOVE_SHORT 5
#define MOUSE_MOVE_LONG  20

const uint32_t mouse_jiggler_interval_values[] = {500, 1000, 2000, 5000, 10000, 15000, 30000, 60000, 90000, 120000};
const char* mouse_jiggler_interval_strings[] = {"500m", "1", "2", "5", "10", "15", "30", "60", "90", "120"};
const uint8_t mouse_jiggler_interval_count = 10;

typedef enum {
    EventTypeInput,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} UsbMouseEvent;

typedef struct {
    bool running;
    bool processing;
    bool update_viewport;
    uint8_t interval;
    uint32_t delay_value;
    int8_t mouse_move_dx;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
} MouseJigglerState;

static void update_interval_value(Canvas* canvas, const MouseJigglerState* plugin_state);

static void mouse_jiggler_render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    const MouseJigglerState* plugin_state = ctx;
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
    if(plugin_state == NULL) {
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "USB Mouse Jiggler");
    if(!plugin_state->running) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 27, " -> STOPPED");
        canvas_draw_str(canvas, 2, 51, "[ok] start / [back] exit");
        canvas_draw_str(canvas, 2, 63, "[up/down] adjust interval");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 27, " -> RUNNING");
        canvas_draw_str(canvas, 2, 51, "[back] stop");
    }
    update_interval_value(canvas, plugin_state);

    furi_mutex_release(plugin_state->mutex);
}

void update_interval_value(Canvas* canvas, const MouseJigglerState* plugin_state) {
    canvas_set_font(canvas, FontSecondary);
    char interval_string[20];
    snprintf(
        interval_string,
        20,
        "Interval: %ss",
        mouse_jiggler_interval_strings[plugin_state->interval]);
    canvas_draw_str(canvas, 2, 39, interval_string);
}

static void mouse_jiggler_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    UsbMouseEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void mouse_jiggler_state_init(MouseJigglerState* const plugin_state) {
    plugin_state->running = false;
    plugin_state->processing = true;
    plugin_state->update_viewport = false;
    plugin_state->interval = 0;
    plugin_state->delay_value = 500;
    plugin_state->mouse_move_dx = MOUSE_MOVE_SHORT;
}

void set_interval(MouseJigglerState* const plugin_state, bool increase) {
    if(increase) {
        plugin_state->interval = MIN(plugin_state->interval + 1, mouse_jiggler_interval_count - 1);
    } else {
        plugin_state->interval = MAX(plugin_state->interval - 1, 0);
    }
    plugin_state->delay_value = mouse_jiggler_interval_values[(plugin_state->interval)];
    plugin_state->update_viewport = true;
}

void check_key_events(MouseJigglerState* const plugin_state) {
    UsbMouseEvent event;
    FuriStatus event_status = furi_message_queue_get(plugin_state->event_queue, &event, 100);
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
    if(event_status == FuriStatusOk) {
        if(event.type == EventTypeKey) {
            if(event.input.type == InputTypePress) {
                switch(event.input.key) {
                case InputKeyOk:
                    if(!plugin_state->running) {
                        plugin_state->running = true;
                        plugin_state->update_viewport = true;
                    }
                    break;
                case InputKeyBack:
                    if(!plugin_state->running) {
                        plugin_state->processing = false;
                        plugin_state->update_viewport = true;
                    } else {
                        plugin_state->running = false;
                        plugin_state->update_viewport = true;
                    }
                    break;
                case InputKeyUp:
                    if(!plugin_state->running) {
                        set_interval(plugin_state, true);
                    }
                    break;
                case InputKeyDown:
                    if(!plugin_state->running) {
                        set_interval(plugin_state, false);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
    furi_mutex_release(plugin_state->mutex);
}

int32_t mouse_jiggler_app(void* p) {
    UNUSED(p);

    MouseJigglerState* plugin_state = malloc(sizeof(MouseJigglerState));
    if(plugin_state == NULL) {
        FURI_LOG_E("MouseJiggler", "MouseJigglerState: malloc error\r\n");
        return 255;
    }
    mouse_jiggler_state_init(plugin_state);

    plugin_state->event_queue = furi_message_queue_alloc(8, sizeof(UsbMouseEvent));
    FuriMessageQueue* event_queue = plugin_state->event_queue;

    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!plugin_state->mutex) {
        furi_message_queue_free(event_queue);
        free(plugin_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, mouse_jiggler_render_callback, plugin_state);
    view_port_input_callback_set(view_port, mouse_jiggler_input_callback, event_queue);

    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    furi_hal_usb_set_config(&usb_hid, NULL);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(plugin_state->processing) {
        check_key_events(plugin_state);
        if(plugin_state->running) {
            furi_hal_hid_mouse_move(plugin_state->mouse_move_dx, 0);
            plugin_state->mouse_move_dx = -(plugin_state->mouse_move_dx);
            for(uint32_t i = 0; i < plugin_state->delay_value / 2 && plugin_state->processing; i += 50) {
                check_key_events(plugin_state);
                furi_delay_ms(50);
            }
        }
        if (plugin_state->update_viewport) {
            view_port_update(view_port);
            plugin_state->update_viewport = false;
        }
        furi_delay_ms(50);
    }
    furi_hal_usb_set_config(usb_mode_prev, NULL);
    // remove & free all stuff created by app
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(plugin_state->mutex);
    free(plugin_state);
    return 0;
}
