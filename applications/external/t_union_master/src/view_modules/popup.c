#include "popup.h"
#include <gui/elements.h>
#include <furi.h>
#include "elements.h"

#include "../font_provider/c_font.h"

struct Popup {
    View* view;
    void* context;
    PopupCallback callback;

    FuriTimer* timer;
    uint32_t timer_period_in_ms;
    bool timer_enabled;
};

typedef struct {
    const char* text;
    uint8_t x;
    uint8_t y;
    Align horizontal;
    Align vertical;
} TextElement;

typedef struct {
    uint8_t x;
    uint8_t y;
    const Icon* icon;
} IconElement;

typedef struct {
    TextElement header;
    TextElement text;
    IconElement icon;
} PopupModel;

static void popup_view_draw_callback(Canvas* canvas, void* _model) {
    PopupModel* model = _model;

    // Prepare canvas
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->icon.icon != NULL) {
        canvas_draw_icon(canvas, model->icon.x, model->icon.y, model->icon.icon);
    }

    // Draw header
    if(model->header.text != NULL) {
        // canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned_utf8(
            canvas,
            model->header.x,
            model->header.y,
            model->header.horizontal,
            model->header.vertical,
            model->header.text);
    }

    // Draw text
    if(model->text.text != NULL) {
        // canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned_utf8(
            canvas,
            model->text.x,
            model->text.y,
            model->text.horizontal,
            model->text.vertical,
            model->text.text);
    }
}

static void popup_timer_callback(void* context) {
    furi_assert(context);
    Popup* popup = context;

    if(popup->callback) {
        popup->callback(popup->context);
    }
}

static bool popup_view_input_callback(InputEvent* event, void* context) {
    Popup* popup = context;
    bool consumed = false;

    // Process key presses only
    if(event->type == InputTypeShort && popup->callback) {
        popup->callback(popup->context);
        consumed = true;
    }

    return consumed;
}

void popup_start_timer(void* context) {
    Popup* popup = context;
    if(popup->timer_enabled) {
        uint32_t timer_period =
            popup->timer_period_in_ms / (1000.0f / furi_kernel_get_tick_frequency());
        if(timer_period == 0) timer_period = 1;

        if(furi_timer_start(popup->timer, timer_period) != FuriStatusOk) {
            furi_crash();
        };
    }
}

void popup_stop_timer(void* context) {
    Popup* popup = context;
    furi_timer_stop(popup->timer);
}

Popup* popup_alloc_utf8() {
    Popup* popup = malloc(sizeof(Popup));
    popup->view = view_alloc();
    popup->timer = furi_timer_alloc(popup_timer_callback, FuriTimerTypeOnce, popup);
    furi_assert(popup->timer);
    popup->timer_period_in_ms = 1000;
    popup->timer_enabled = false;

    view_set_context(popup->view, popup);
    view_allocate_model(popup->view, ViewModelTypeLocking, sizeof(PopupModel));
    view_set_draw_callback(popup->view, popup_view_draw_callback);
    view_set_input_callback(popup->view, popup_view_input_callback);
    view_set_enter_callback(popup->view, popup_start_timer);
    view_set_exit_callback(popup->view, popup_stop_timer);

    with_view_model(
        popup->view,
        PopupModel * model,
        {
            model->header.text = NULL;
            model->header.x = 0;
            model->header.y = 0;
            model->header.horizontal = AlignLeft;
            model->header.vertical = AlignBottom;

            model->text.text = NULL;
            model->text.x = 0;
            model->text.y = 0;
            model->text.horizontal = AlignLeft;
            model->text.vertical = AlignBottom;

            model->icon.x = 0;
            model->icon.y = 0;
            model->icon.icon = NULL;
        },
        true);
    return popup;
}
