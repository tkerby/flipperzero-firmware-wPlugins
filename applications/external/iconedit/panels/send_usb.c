#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "iconedit.h"
#include "utils/file_utils.h"
#include "utils/draw.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#include <gui/icon_i.h>

#define MAX_LINES  64
#define LINE_COUNT 4 // number of lines visible in the panel

typedef enum {
    State_NONE,
    State_CONNECTING,
    State_READY,
    State_SENDING,
    State_DONE
} SendUSBState;

typedef struct {
    IEIcon* icon;
    SendUSBState state;
    char* lines[MAX_LINES];
    int num_lines;
    IconEditUpdateCallback callback;
    void* callback_context;
    // FuriThread* thread;

    FuriHalUsbInterface* usb_if_prev;
} SendUSBModel;
static SendUSBModel sendModel = {.state = State_NONE};

void add_line(char* line) {
    if(!line) return;
    // deal with max lines
    char* new_line = malloc(strlen(line) + 1);
    strcpy(new_line, line);
    new_line[strlen(line)] = 0;
    sendModel.lines[sendModel.num_lines] = new_line;
    sendModel.num_lines++;
    if(sendModel.callback) {
        sendModel.callback(sendModel.callback_context);
    }
}

// void send_usb_timer_callback(void* context) {
//     SendUSBModel* model = (SendUSBModel*)context;
//     if(model->callback) {
//         model->callback(model->callback_context);
//     }
// }

// static int32_t usb_worker(void* context) {
//     SendUSBModel* model = context;

//     // define our update timer
//     FuriTimer* timer =
//         furi_timer_alloc(send_usb_timer_callback, FuriTimerTypePeriodic, &sendModel);
//     furi_timer_start(timer, 500);

//     model->usb_if_prev = furi_hal_usb_get_config();
//     furi_hal_usb_unlock();
//     furi_check(furi_hal_usb_set_config(NULL, NULL) == true);

//     model->interface = BadUsbHidInterfaceUsb;
//     model->api = bad_usb_hid_get_interface(model->interface);
//     add_line("Initializing USB");
//     model->hid_inst = model->api->init(NULL);

//     while(model->state != State_DONE) {
//         // generate text buffer
//         char buffer[] = "This is a test";
//         // We should be connected to USB and can send keys
//         for(size_t i = 0; i < strlen(buffer); i++) {
//             furi_hal_hid_kb_press(HID_ASCII_TO_KEY(buffer[i]));
//             furi_delay_ms(50); // slight pause between bytes?
//         }
//     }

//     model->api->deinit(model->hid_inst);
//     if(sendModel.usb_if_prev) {
//         furi_check(furi_hal_usb_set_config(model->usb_if_prev, NULL));
//     }

//     furi_timer_free(timer);

//     FURI_LOG_I(TAG, "usb_worker %p ended", furi_thread_get_id(model->thread));
//     return 0;
// }

void send_usb_start(IEIcon* icon) {
    FURI_LOG_I(TAG, __FUNCTION__);
    if(!sendModel.callback) {
        FURI_LOG_E(TAG, "Callback not set for Send USB");
        return;
    }
    sendModel.icon = icon;
    for(int l = 0; l < MAX_LINES; l++) {
        sendModel.lines[l] = NULL;
    }
    sendModel.state = State_CONNECTING;

    // define our update timer
    // FuriTimer* timer =
    //     furi_timer_alloc(send_usb_timer_callback, FuriTimerTypePeriodic, &sendModel);
    // furi_timer_start(timer, 500);

    sendModel.usb_if_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);

    sendModel.state = State_READY;
    // add_line("Starting USB worker");
    // sendModel.thread = furi_thread_alloc_ex("USBWorker", 2048, usb_worker, &sendModel);
    // furi_thread_start(sendModel.thread);
    add_line("Ready to send?");
    sendModel.callback(sendModel.callback_context);
}

void send_usb_stop() {
    FURI_LOG_I(TAG, __FUNCTION__);
    add_line("Stopping...");
    // uhh... do we need to stop the thread?
    for(int l = 0; l < MAX_LINES; l++) {
        if(sendModel.lines[l]) {
            free(sendModel.lines[l]);
        }
    }
    sendModel.num_lines = 0;

    furi_hal_usb_set_config(sendModel.usb_if_prev, NULL);
    sendModel.state = State_NONE;
    // furi_thread_join(sendModel.thread);
    // furi_thread_free(sendModel.thread);
}

void send_usb_set_update_callback(IconEditUpdateCallback callback, void* context) {
    sendModel.callback = callback;
    sendModel.callback_context = context;
}

void send_usb_send_icon() {
    FURI_LOG_I(TAG, __FUNCTION__);
    FuriString* icon_text = c_file_generate(sendModel.icon);

    for(size_t i = 0; i < furi_string_size(icon_text); i++) {
        uint16_t key = HID_ASCII_TO_KEY(furi_string_get_char(icon_text, i));
        furi_hal_hid_kb_press(key);
        furi_hal_hid_kb_release_all();
    }
    furi_string_free(icon_text);
    add_line("Sent!");
    sendModel.state = State_DONE;
}

void send_usb_draw(Canvas* canvas, void* context) {
    FURI_LOG_I(TAG, __FUNCTION__);
    UNUSED(context);

    // draw a large panel that will contain (maybe scrolling) text
    // showing the current status of the send
    // also maybe pause at certain actions, prompting user to OK to continue

    int x = 20; // redo this to use canvas_width, etc
    int y = 10;
    int pad = 2; // outside padding between frame and line
    int line_h = 7;
    int line_w = 80;
    int line_pad = 1; // the space between

    // draw an empty panel frame
    int rw = line_w + (pad * 2) + (line_pad * 2);
    int rh = (line_h + (line_pad * 2)) * LINE_COUNT + (pad * 2);
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    int start = sendModel.num_lines <= LINE_COUNT ? 0 : sendModel.num_lines - LINE_COUNT;
    int end = sendModel.num_lines <= LINE_COUNT ? sendModel.num_lines : start + LINE_COUNT;

    for(int l = start; l < end; l++) {
        FURI_LOG_I(TAG, "usb: %s", sendModel.lines[l]);
        canvas_draw_str_aligned(
            canvas,
            x + pad + line_pad + 1,
            y + pad + line_pad + l * (line_h + line_pad * 2),
            AlignLeft,
            AlignTop,
            sendModel.lines[l]);
    }
    if(sendModel.state == State_READY) {
        // draw the OK button
        canvas_draw_str_aligned(
            canvas,
            x + pad + line_pad + 1,
            y + pad + line_pad + MAX_LINES * (line_h * line_pad * 2),
            AlignLeft,
            AlignTop,
            "OK");
    }
    // draw a scrollbar / scroll indicator?
}

bool send_usb_input(InputEvent* event, void* context) {
    FURI_LOG_I(TAG, __FUNCTION__);
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyBack: {
            consumed = false; // send control back to File panel

            app->panel = Panel_File;
            send_usb_stop();
            break;
        }
        case InputKeyOk: {
            switch(sendModel.state) {
            case State_READY:
                // Our USB connection is ready and we're prompting user
                // if they are ready to transmit
                sendModel.state = State_SENDING;
                send_usb_send_icon();
                break;
            case State_DONE:
                app->panel = Panel_File;
                send_usb_stop();
                consumed = false;
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
