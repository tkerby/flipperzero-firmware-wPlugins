#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <infrared_transmit.h>

#include "onkyo_commands.h"
#include "onkyo_layout.h"

#define TAG "OnkyoRemote"

/** Frames sent per press -- roughly what a short press on the original remote produces. */
#define ONKYO_TX_FRAMES 2

/** How long the run loop waits for input before re-checking `running`. */
#define ONKYO_QUEUE_TIMEOUT 100

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notifications;
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    OnkyoViewModel model;
    bool running;
} OnkyoApp;

static void onkyo_draw_callback(Canvas* canvas, void* ctx) {
    OnkyoApp* app = ctx;

    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    const OnkyoViewModel model = app->model;
    furi_mutex_release(app->mutex);

    onkyo_layout_draw(canvas, &model);
}

/**
 * The loader raises FuriSignalExit when something asks the app to close --
 * `loader close` over the CLI, or `ufbt launch` replacing a running instance.
 * Without a handler the loader gives up and reports that the app has to be
 * closed manually on the device. The run loop notices the flag within one
 * queue timeout.
 */
static bool onkyo_signal_callback(uint32_t signal, void* arg, void* ctx) {
    UNUSED(arg);
    OnkyoApp* app = ctx;

    if(signal == FuriSignalExit) {
        app->running = false;
        return true;
    }
    return false;
}

static void onkyo_input_callback(InputEvent* event, void* ctx) {
    OnkyoApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static void onkyo_move(OnkyoApp* app, int8_t d_row, int8_t d_col) {
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    onkyo_layout_move(&app->model, d_row, d_col);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void onkyo_set_sending(OnkyoApp* app, bool sending) {
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    app->model.sending = sending;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void onkyo_transmit(OnkyoApp* app) {
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    const OnkyoCmdId id = onkyo_layout_focused_command(&app->model);
    furi_mutex_release(app->mutex);

    const OnkyoCommand* cmd = &onkyo_commands[id];
    const InfraredMessage message = {
        .protocol = cmd->protocol,
        .address = cmd->address,
        .command = cmd->command,
        .repeat = false,
    };

    FURI_LOG_I(TAG, "tx %s addr=0x%04lX cmd=0x%04lX", cmd->name, cmd->address, cmd->command);

    onkyo_set_sending(app, true);
    notification_message(app->notifications, &sequence_blink_start_magenta);

    infrared_send(&message, ONKYO_TX_FRAMES);

    notification_message(app->notifications, &sequence_blink_stop);
    onkyo_set_sending(app, false);

    /* Presses made while the send was blocking are dropped, so an impatient
     * double tap cannot queue a second burst. */
    furi_message_queue_reset(app->input_queue);
}

static void onkyo_handle_input(OnkyoApp* app, const InputEvent* event) {
    /* Holding a direction keeps moving; holding OK does not keep transmitting. */
    const bool navigate = (event->type == InputTypeShort) || (event->type == InputTypeRepeat);

    switch(event->key) {
    case InputKeyUp:
        if(navigate) onkyo_move(app, -1, 0);
        break;
    case InputKeyDown:
        if(navigate) onkyo_move(app, 1, 0);
        break;
    case InputKeyLeft:
        if(navigate) onkyo_move(app, 0, -1);
        break;
    case InputKeyRight:
        if(navigate) onkyo_move(app, 0, 1);
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) onkyo_transmit(app);
        break;
    case InputKeyBack:
        if(event->type == InputTypeShort) app->running = false;
        break;
    default:
        break;
    }
}

static OnkyoApp* onkyo_app_alloc(void) {
    OnkyoApp* app = malloc(sizeof(OnkyoApp));

    onkyo_layout_init(&app->model);
    app->running = true;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    furi_thread_set_signal_callback(furi_thread_get_current(), onkyo_signal_callback, app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, onkyo_draw_callback, app);
    view_port_input_callback_set(app->view_port, onkyo_input_callback, app);
    /* Portrait: the Flipper is turned so the D-pad sits right of the screen. */
    view_port_set_orientation(app->view_port, ViewPortOrientationVertical);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    return app;
}

static void onkyo_app_free(OnkyoApp* app) {
    /* Before the context it points at goes away. */
    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t onkyo_remote_app(void* p) {
    UNUSED(p);

    OnkyoApp* app = onkyo_app_alloc();

    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &event, ONKYO_QUEUE_TIMEOUT) == FuriStatusOk) {
            onkyo_handle_input(app, &event);
        }
    }

    onkyo_app_free(app);
    return 0;
}
