#include "mouse_jiggler_worker.h"

static const uint8_t image_back_btn_0_bits[] =
    {0x04, 0x00, 0x06, 0x00, 0xff, 0x00, 0x06, 0x01, 0x04, 0x02, 0x00, 0x02, 0x00, 0x01, 0xf8, 0x00};
static const uint8_t image_ButtonCenter_0_bits[] = {0x1c, 0x22, 0x5d, 0x5d, 0x5d, 0x22, 0x1c};
static const uint8_t image_ButtonDown_0_bits[] = {0x7f, 0x3e, 0x1c, 0x08};
static const uint8_t image_ButtonUp_0_bits[] = {0x08, 0x1c, 0x3e, 0x7f};
static const char* mouse_jiggler_interval_strings[] = {
    "500 ms",
    "1 s",
    "2 s",
    "5 s",
    "10 s",
    "15 s",
    "30 s",
    "1 min",
    "1.5 min",
    "2 min",
    "5 min",
    "10 min"};

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    MouseJigglerWorker* worker;
} MouseJiggler;

static void mouse_jiggler_render_callback(Canvas* canvas, void* ctx) {
    const MouseJiggler* mouse_jiggler = ctx;
    furi_check(furi_mutex_acquire(mouse_jiggler->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 13, "USB Mouse Jiggler");
    canvas_draw_str(canvas, 5, 41, "interval");
    canvas_draw_str(canvas, 5, 28, "status");

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(
        canvas, 60, 41, mouse_jiggler_interval_strings[mouse_jiggler->worker->delay_index]);

    if(!mouse_jiggler->worker->is_jiggle) {
        canvas_draw_str(canvas, 60, 28, "stopped");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 14, 60, "start");
        canvas_draw_str(canvas, 108, 60, "exit");

        canvas_draw_xbm(canvas, 5, 53, 7, 7, image_ButtonCenter_0_bits);
        canvas_draw_xbm(canvas, 107, 32, 7, 4, image_ButtonUp_0_bits);
        canvas_draw_xbm(canvas, 107, 38, 7, 4, image_ButtonDown_0_bits);
    } else {
        canvas_draw_str(canvas, 60, 28, "RUNNING");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 108, 60, "stop");
    }

    canvas_draw_xbm(canvas, 95, 52, 10, 8, image_back_btn_0_bits);
    furi_mutex_release(mouse_jiggler->mutex);
}

static void mouse_jiggler_input_callback(InputEvent* input_event, void* ctx) {
    furi_check(ctx);
    MouseJiggler* mouse_jiggler = ctx;
    furi_message_queue_put(mouse_jiggler->event_queue, input_event, FuriWaitForever);
}

MouseJiggler* mouse_jiggler_alloc() {
    MouseJiggler* instance = malloc(sizeof(MouseJiggler));

    instance->worker = mouse_jiggler_worker_alloc();
    instance->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    instance->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, mouse_jiggler_render_callback, instance);
    view_port_input_callback_set(instance->view_port, mouse_jiggler_input_callback, instance);

    instance->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);

    return instance;
}

void mouse_jiggler_free(MouseJiggler* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(instance->view_port);
    mouse_jiggler_worker_free(instance->worker);
    furi_message_queue_free(instance->event_queue);
    furi_mutex_free(instance->mutex);
    free(instance->worker);
    free(instance);
}

int32_t mouse_jiggler_app() {
    MouseJiggler* mouse_jiggler = mouse_jiggler_alloc();
    MouseJigglerWorker* worker = mouse_jiggler->worker;
    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    InputEvent event;
    furi_hal_usb_set_config(&usb_hid, NULL);
    mouse_jiggler_worker_start(worker);

    while(furi_message_queue_get(mouse_jiggler->event_queue, &event, FuriWaitForever) ==
          FuriStatusOk) {
        furi_check(furi_mutex_acquire(mouse_jiggler->mutex, FuriWaitForever) == FuriStatusOk);
        if(event.type == InputTypePress) {
            if(event.key == InputKeyBack && !(worker->is_jiggle)) {
                furi_mutex_release(mouse_jiggler->mutex);
                break;
            } else if(
                event.key == InputKeyUp && !worker->is_jiggle &&
                worker->delay_index < mouse_jiggler_interval_count - 1) {
                worker->delay_index++;
                mouse_jiggler_worker_set_delay(worker);
                view_port_update(mouse_jiggler->view_port);
            } else if(event.key == InputKeyDown && !worker->is_jiggle && worker->delay_index > 0) {
                worker->delay_index--;
                mouse_jiggler_worker_set_delay(worker);
                view_port_update(mouse_jiggler->view_port);
            } else if(
                (event.key == InputKeyBack && worker->is_jiggle) ||
                (event.key == InputKeyOk && !worker->is_jiggle)) {
                mouse_jiggler_worker_toggle_jiggle(worker);
                view_port_update(mouse_jiggler->view_port);
            }
        }
        furi_mutex_release(mouse_jiggler->mutex);
    }
    mouse_jiggler_worker_stop(worker);
    mouse_jiggler_free(mouse_jiggler);
    furi_delay_ms(10);
    furi_hal_usb_set_config(usb_mode_prev, NULL);
    furi_delay_ms(10);
    return 0;
}
