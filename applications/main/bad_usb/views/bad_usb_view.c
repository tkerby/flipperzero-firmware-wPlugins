#include "bad_usb_view.h"
#include "../helpers/ducky_script.h"
#include <toolbox/path.h>
#include <gui/elements.h>
#include <bad_usb_icons.h>
#include <bt/bt_service/bt_i.h>

#define MAX_NAME_LEN 64

struct BadUsb {
    View* view;
    BadUsbButtonCallback callback;
    void* context;
};

typedef struct {
    char file_name[MAX_NAME_LEN];
    char layout[MAX_NAME_LEN];
    BadUsbState state;
    bool pause_wait;
    uint8_t anim_frame;
    BadUsbHidInterface interface;
    Bt* bt;
} BadUsbModel;

static void bad_usb_draw_callback(Canvas* canvas, void* _model) {
    BadUsbModel* model = _model;

    FuriString* disp_str = furi_string_alloc_set(model->file_name);
    elements_string_fit_width(canvas, disp_str, 128 - 2);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, furi_string_get_cstr(disp_str));

    if(strlen(model->layout) == 0) {
        furi_string_set(disp_str, "(default)");
    } else {
        furi_string_printf(disp_str, "(%s)", model->layout);
    }
    if(model->interface == BadUsbHidInterfaceBle && model->bt->pin_code) {
        furi_string_cat_printf(disp_str, "  PIN: %ld", model->bt->pin_code);
    } else {
        uint32_t e = model->state.elapsed;
        furi_string_cat_printf(disp_str, "  %02lu:%02lu.%ld", e / 60 / 1000, e / 1000, e % 1000);
    }
    elements_string_fit_width(canvas, disp_str, 128 - 2);
    canvas_draw_str(
        canvas, 2, 8 + canvas_current_font_height(canvas), furi_string_get_cstr(disp_str));

    furi_string_reset(disp_str);

    if(model->interface == BadUsbHidInterfaceBle) {
        canvas_draw_icon(canvas, 22, 24, &I_Bad_BLE_48x22);
    } else {
        canvas_draw_icon(canvas, 22, 24, &I_UsbTree_48x22);
    }

    BadUsbWorkerState state = model->state.state;

    if((state == BadUsbStateIdle) || (state == BadUsbStateDone) ||
       (state == BadUsbStateNotConnected)) {
        elements_button_center(canvas, "Run");
        elements_button_left(canvas, "Config");
        elements_button_right(canvas, model->interface == BadUsbHidInterfaceBle ? "USB" : "BLE");
    } else if((state == BadUsbStateRunning) || (state == BadUsbStateDelay)) {
        elements_button_center(canvas, "Stop");
        if(!model->pause_wait) {
            elements_button_right(canvas, "Pause");
        }
    } else if(state == BadUsbStatePaused) {
        elements_button_center(canvas, "End");
        elements_button_right(canvas, "Resume");
    } else if(state == BadUsbStateWaitForBtn) {
        elements_button_center(canvas, "Press to continue");
    } else if(state == BadUsbStateWillRun) {
        elements_button_center(canvas, "Cancel");
    }

    if(state == BadUsbStateNotConnected) {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "Connect");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "to device");
    } else if(state == BadUsbStateWillRun) {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "Will run");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "on connect");
    } else if(state == BadUsbStateFileError) {
        canvas_draw_icon(canvas, 4, 26, &I_Error_18x18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 31, AlignRight, AlignBottom, "File");
        canvas_draw_str_aligned(canvas, 127, 43, AlignRight, AlignBottom, "ERROR");
    } else if(state == BadUsbStateScriptError) {
        canvas_draw_icon(canvas, 4, 26, &I_Error_18x18);
        furi_string_printf(disp_str, "line %zu", model->state.error_line);
        canvas_draw_str_aligned(
            canvas, 127, 46, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        furi_string_set_str(disp_str, model->state.error);
        elements_string_fit_width(canvas, disp_str, canvas_width(canvas));
        canvas_draw_str_aligned(
            canvas, 127, 56, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 127, 33, AlignRight, AlignBottom, "ERROR:");
    } else if(state == BadUsbStateIdle) {
        canvas_draw_icon(canvas, 4, 26, &I_Smile_18x18);
        furi_string_printf(disp_str, "0/%zu", model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 124, 47, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 112, 37, AlignRight, AlignBottom, "0");
        canvas_draw_icon(canvas, 115, 23, &I_Percent_10x14);
    } else if(state == BadUsbStateRunning) {
        if(model->anim_frame == 0) {
            canvas_draw_icon(canvas, 4, 23, &I_EviSmile1_18x21);
        } else {
            canvas_draw_icon(canvas, 4, 23, &I_EviSmile2_18x21);
        }
        furi_string_printf(disp_str, "%zu/%zu", model->state.line_cur, model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 124, 47, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(
            disp_str, "%zu", ((model->state.line_cur - 1) * 100) / model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 112, 37, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_draw_icon(canvas, 115, 23, &I_Percent_10x14);
    } else if(state == BadUsbStateDone) {
        canvas_draw_icon(canvas, 4, 23, &I_EviSmile1_18x21);
        furi_string_printf(disp_str, "%zu/%zu", model->state.line_nb, model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 124, 47, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 112, 37, AlignRight, AlignBottom, "100");
        canvas_draw_icon(canvas, 115, 23, &I_Percent_10x14);
    } else if(state == BadUsbStateDelay) {
        if(model->anim_frame == 0) {
            canvas_draw_icon(canvas, 4, 23, &I_EviWaiting1_18x21);
        } else {
            canvas_draw_icon(canvas, 4, 23, &I_EviWaiting2_18x21);
        }
        uint32_t delay = model->state.delay_remain / 10;
        if(delay) {
            furi_string_printf(disp_str, "Delay %lus", delay);
            canvas_draw_str_aligned(
                canvas, 4, 61, AlignLeft, AlignBottom, furi_string_get_cstr(disp_str));
        }
        furi_string_printf(disp_str, "%zu/%zu", model->state.line_cur, model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 124, 47, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(
            disp_str, "%zu", ((model->state.line_cur - 1) * 100) / model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 112, 37, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_draw_icon(canvas, 115, 23, &I_Percent_10x14);
    } else if((state == BadUsbStatePaused) || (state == BadUsbStateWaitForBtn)) {
        if(model->anim_frame == 0) {
            canvas_draw_icon(canvas, 4, 23, &I_EviWaiting1_18x21);
        } else {
            canvas_draw_icon(canvas, 4, 23, &I_EviWaiting2_18x21);
        }
        if(state != BadUsbStateWaitForBtn) {
            canvas_draw_str_aligned(canvas, 4, 61, AlignLeft, AlignBottom, "Paused");
        }
        furi_string_printf(disp_str, "%zu/%zu", model->state.line_cur, model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 124, 47, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(
            disp_str, "%zu", ((model->state.line_cur - 1) * 100) / model->state.line_nb);
        canvas_draw_str_aligned(
            canvas, 112, 37, AlignRight, AlignBottom, furi_string_get_cstr(disp_str));
        canvas_draw_icon(canvas, 115, 23, &I_Percent_10x14);
    } else {
        canvas_draw_icon(canvas, 4, 26, &I_Clock_18x18);
    }

    furi_string_free(disp_str);
}

static bool bad_usb_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    BadUsb* bad_usb = context;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            consumed = true;
            furi_assert(bad_usb->callback);
            bad_usb->callback(event->key, bad_usb->context);
        } else if(event->key == InputKeyOk) {
            with_view_model(
                bad_usb->view, BadUsbModel * model, { model->pause_wait = false; }, true);
            consumed = true;
            furi_assert(bad_usb->callback);
            bad_usb->callback(event->key, bad_usb->context);
        } else if(event->key == InputKeyRight) {
            with_view_model(
                bad_usb->view,
                BadUsbModel * model,
                {
                    if((model->state.state == BadUsbStateRunning) ||
                       (model->state.state == BadUsbStateDelay)) {
                        model->pause_wait = true;
                    }
                },
                true);
            consumed = true;
            furi_assert(bad_usb->callback);
            bad_usb->callback(event->key, bad_usb->context);
        }
    }

    return consumed;
}

BadUsb* bad_usb_view_alloc(void) {
    BadUsb* bad_usb = malloc(sizeof(BadUsb));

    bad_usb->view = view_alloc();
    view_allocate_model(bad_usb->view, ViewModelTypeLocking, sizeof(BadUsbModel));
    view_set_context(bad_usb->view, bad_usb);
    view_set_draw_callback(bad_usb->view, bad_usb_draw_callback);
    view_set_input_callback(bad_usb->view, bad_usb_input_callback);

    with_view_model(
        bad_usb->view, BadUsbModel * model, { model->bt = furi_record_open(RECORD_BT); }, true);

    return bad_usb;
}

void bad_usb_view_free(BadUsb* bad_usb) {
    furi_assert(bad_usb);
    furi_record_close(RECORD_BT);
    view_free(bad_usb->view);
    free(bad_usb);
}

View* bad_usb_view_get_view(BadUsb* bad_usb) {
    furi_assert(bad_usb);
    return bad_usb->view;
}

void bad_usb_view_set_button_callback(
    BadUsb* bad_usb,
    BadUsbButtonCallback callback,
    void* context) {
    furi_assert(bad_usb);
    furi_assert(callback);
    with_view_model(
        bad_usb->view,
        BadUsbModel * model,
        {
            UNUSED(model);
            bad_usb->callback = callback;
            bad_usb->context = context;
        },
        true);
}

void bad_usb_view_set_file_name(BadUsb* bad_usb, const char* name) {
    furi_assert(name);
    with_view_model(
        bad_usb->view,
        BadUsbModel * model,
        { strlcpy(model->file_name, name, MAX_NAME_LEN); },
        true);
}

void bad_usb_view_set_layout(BadUsb* bad_usb, const char* layout) {
    furi_assert(layout);
    with_view_model(
        bad_usb->view,
        BadUsbModel * model,
        { strlcpy(model->layout, layout, MAX_NAME_LEN); },
        true);
}

void bad_usb_view_set_state(BadUsb* bad_usb, BadUsbState* st) {
    furi_assert(st);
    with_view_model(
        bad_usb->view,
        BadUsbModel * model,
        {
            memcpy(&(model->state), st, sizeof(BadUsbState));
            model->anim_frame ^= 1;
            if(model->state.state == BadUsbStatePaused) {
                model->pause_wait = false;
            }
        },
        true);
}

void bad_usb_view_set_interface(BadUsb* bad_usb, BadUsbHidInterface interface) {
    with_view_model(bad_usb->view, BadUsbModel * model, { model->interface = interface; }, true);
}

bool bad_usb_view_is_idle_state(BadUsb* bad_usb) {
    bool is_idle = false;
    with_view_model(
        bad_usb->view,
        BadUsbModel * model,
        {
            if((model->state.state == BadUsbStateIdle) ||
               (model->state.state == BadUsbStateDone) ||
               (model->state.state == BadUsbStateNotConnected)) {
                is_idle = true;
            }
        },
        false);
    return is_idle;
}
