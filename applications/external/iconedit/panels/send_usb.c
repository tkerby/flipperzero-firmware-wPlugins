#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "iconedit.h"
#include "utils/file_utils.h"
#include "utils/draw.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#include <gui/icon_i.h>

#define MAX_LINES       64
#define LINE_COUNT      4 // number of lines visible in the panel
#define END_OF_FILE_STR "EOF\n"
typedef enum {
    State_NONE,
    State_READY,
    State_FILENAME,
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
    SendAsType send_as;
    bool filename_prompt;
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

void send_usb_start(IEIcon* icon, SendAsType send_as) {
    sendModel.send_as = send_as;
    if(send_as == SendAsC) {
        sendModel.filename_prompt = false;
    } else {
        sendModel.filename_prompt = true;
    }

    FURI_LOG_I(TAG, __FUNCTION__);
    if(!sendModel.callback) {
        FURI_LOG_E(TAG, "Callback not set for Send USB");
        return;
    }
    sendModel.icon = icon;
    for(int l = 0; l < MAX_LINES; l++) {
        sendModel.lines[l] = NULL;
    }

    sendModel.usb_if_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, NULL) == true);

    sendModel.callback(sendModel.callback_context);

    sendModel.state = State_READY;
    add_line("Ready to send?");
}

void send_usb_stop() {
    FURI_LOG_I(TAG, __FUNCTION__);
    add_line("Stopping...");
    for(int l = 0; l < MAX_LINES; l++) {
        if(sendModel.lines[l]) {
            free(sendModel.lines[l]);
        }
    }
    sendModel.num_lines = 0;

    furi_hal_usb_set_config(sendModel.usb_if_prev, NULL);
    sendModel.state = State_NONE;
}

void send_usb_send_str(const char* str) {
    for(size_t i = 0; i < strlen(str); i++) {
        uint16_t key = HID_ASCII_TO_KEY(str[i]);
        furi_hal_hid_kb_press(key);
        furi_hal_hid_kb_release(key);
    }
}

void send_usb_set_update_callback(IconEditUpdateCallback callback, void* context) {
    sendModel.callback = callback;
    sendModel.callback_context = context;
}

void send_usb_send_filename() {
    FuriString* filename = furi_string_alloc_set(sendModel.icon->name);
    if(sendModel.send_as == SendAsPNG) {
        furi_string_cat_str(filename, ".png");

    }
    furi_string_cat_str(filename, "\n");
    send_usb_send_str(furi_string_get_cstr(filename));
    send_usb_send_str(END_OF_FILE_STR);
    furi_string_free(filename);

    add_line("Ready to send data?");
    sendModel.state = State_READY;
    sendModel.filename_prompt = false;
}

void send_usb_send_icon() {
    FURI_LOG_I(TAG, __FUNCTION__);
    add_line("Sending...");
    FuriString* icon_text = NULL;
    switch(sendModel.send_as) {
    case SendAsC:
        icon_text = c_file_generate(sendModel.icon);
        break;
    case SendAsPNG:
        icon_text = png_file_generate(sendModel.icon);
        break;
    default:
        break;
    }

    if(icon_text == NULL) {
        FURI_LOG_E(TAG, "NULL icon_text to send");
        add_line("Error!");
        sendModel.state = State_DONE;
        return;
    }

    send_usb_send_str(furi_string_get_cstr(icon_text));
    if(sendModel.send_as != SendAsC) {
        send_usb_send_str(END_OF_FILE_STR);
    }

    furi_hal_hid_kb_release_all();
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

    const int x = 20; // redo this to use canvas_width, etc
    const int y = 10;
    const int pad = 2; // outside padding between frame and line
    const int line_h = 7;
    const int line_w = 90;
    const int line_pad = 2; // the space between

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
            y + pad + line_pad + (l - start) * (line_h + line_pad * 2),
            AlignLeft,
            AlignTop,
            sendModel.lines[l]);
    }
    // if(sendModel.state == State_READY) {
    //     // draw the OK button
    //     canvas_draw_str_aligned(
    //         canvas,
    //         x + pad + line_pad + 1,
    //         y + pad + line_pad + MAX_LINES * (line_h * line_pad * 2),
    //         AlignLeft,
    //         AlignTop,
    //         "OK");
    // }
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
                if(!sendModel.filename_prompt) {
                    sendModel.state = State_SENDING;
                    send_usb_send_icon();
                } else {
                    sendModel.state = State_FILENAME;
                    add_line("Send filename?");
                }
                break;
            case State_FILENAME:
                send_usb_send_filename();
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
        case InputKeyRight: {
            // user wants to skip sending filename
            sendModel.filename_prompt = false;
            sendModel.state = State_SENDING;
            send_usb_send_icon();
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
