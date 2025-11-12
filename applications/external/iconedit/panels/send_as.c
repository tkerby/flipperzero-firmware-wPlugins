#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/file_utils.h"
#include "../utils/draw.h"

typedef enum {
    Send_START,
    Send_PNG = Send_START,
    Send_C,
    Send_BMX,
    Send_COUNT,
    Send_NONE
} SendMode;

const char* SendModeStr[Send_COUNT] = {
    "PNG",
    ".C",
    "Not Implemented BMX",
};
const SendMode Send_UP[Send_COUNT] = {
    Send_NONE,
    Send_PNG,
    Send_C,
};
const SendMode Send_DOWN[Send_COUNT] = {Send_C, Send_BMX, Send_NONE};

static struct {
    SendMode send_mode;
} sendModel = {.send_mode = Send_PNG};

void send_as_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    int pad = 2; // outside padding between frame and items
    int elem_h = 7;
    int elem_w = 22;
    int elem_pad = 2; // the space between

    // draw an empty panel frame
    int rw = elem_w + (pad * 2) + (elem_pad * 2);
    int rh = (elem_h + (elem_pad * 2)) * Send_COUNT + (pad * 2);
    int x = (canvas_width(canvas) - rw) / 2;
    int y = (canvas_height(canvas) - rh) / 2;
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    // draw Send menu items
    for(SendMode mode = Send_START; mode < Send_COUNT; mode++) {
        canvas_draw_str_aligned(
            canvas,
            x + pad + elem_pad + 1,
            y + pad + elem_pad + mode * (elem_h + elem_pad * 2),
            AlignLeft,
            AlignTop,
            SendModeStr[mode]);
    }
    // hightlight the selected item
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_rbox(
        canvas,
        x + pad,
        y + pad + (elem_h + (elem_pad * 2)) * sendModel.send_mode,
        elem_w + elem_pad * 2,
        elem_h + elem_pad * 2,
        0);
    canvas_set_color(canvas, ColorBlack);
}

bool send_as_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            SendMode next_Send = Send_UP[sendModel.send_mode];
            if(next_Send != Send_NONE) {
                sendModel.send_mode = next_Send;
            }
            break;
        }
        case InputKeyDown: {
            SendMode next_Send = Send_DOWN[sendModel.send_mode];
            if(next_Send != Send_NONE) {
                sendModel.send_mode = next_Send;
            }
            break;
        }
        case InputKeyBack: {
            consumed = false; // send control back to File panel
            app->panel = Panel_File;
            break;
        }
        case InputKeyOk: {
            switch(sendModel.send_mode) {
            case Send_PNG: {
                app->panel = Panel_SendUSB;
                send_usb_start(app->icon, SendAsPNG);
                break;
            }
            case Send_C: {
                app->panel = Panel_SendUSB;
                send_usb_start(app->icon, SendAsC);
                break;
            }
            default:
                break;
            }
            consumed = false;
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
