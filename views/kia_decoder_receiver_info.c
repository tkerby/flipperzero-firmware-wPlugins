// views/kia_decoder_receiver_info.c
#include "kia_decoder_receiver_info.h"
#include "../kia_decoder_app_i.h"
#include <input/input.h>
#include <gui/elements.h>
#include <furi.h>

struct KiaReceiverInfo {
    View* view;
    KiaReceiverInfoCallback callback;
    void* context;
};

typedef struct {
    FuriString* protocol_name;
    FuriString* info_text;
} KiaReceiverInfoModel;

void kia_view_receiver_info_set_callback(
    KiaReceiverInfo* receiver_info,
    KiaReceiverInfoCallback callback,
    void* context) {
    furi_assert(receiver_info);
    receiver_info->callback = callback;
    receiver_info->context = context;
}

void kia_view_receiver_info_draw(Canvas* canvas, KiaReceiverInfoModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 0, AlignCenter, AlignTop, furi_string_get_cstr(model->protocol_name));
    
    canvas_draw_line(canvas, 0, 12, 127, 12);
    
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas, 0, 16, AlignLeft, AlignTop, furi_string_get_cstr(model->info_text));
}

bool kia_view_receiver_info_input(InputEvent* event, void* context) {
    furi_assert(context);
    UNUSED(context);
    bool consumed = false;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            consumed = true;
        }
    }

    return consumed;
}

void kia_view_receiver_info_enter(void* context) {
    furi_assert(context);
    UNUSED(context);
}

void kia_view_receiver_info_exit(void* context) {
    furi_assert(context);
    KiaReceiverInfo* receiver_info = context;
    with_view_model(
        receiver_info->view,
        KiaReceiverInfoModel * model,
        {
            furi_string_reset(model->protocol_name);
            furi_string_reset(model->info_text);
        },
        false);
}

KiaReceiverInfo* kia_view_receiver_info_alloc() {
    KiaReceiverInfo* receiver_info = malloc(sizeof(KiaReceiverInfo));

    receiver_info->view = view_alloc();
    view_allocate_model(receiver_info->view, ViewModelTypeLocking, sizeof(KiaReceiverInfoModel));
    view_set_context(receiver_info->view, receiver_info);
    view_set_draw_callback(receiver_info->view, (ViewDrawCallback)kia_view_receiver_info_draw);
    view_set_input_callback(receiver_info->view, kia_view_receiver_info_input);
    view_set_enter_callback(receiver_info->view, kia_view_receiver_info_enter);
    view_set_exit_callback(receiver_info->view, kia_view_receiver_info_exit);

    with_view_model(
        receiver_info->view,
        KiaReceiverInfoModel * model,
        {
            model->protocol_name = furi_string_alloc();
            model->info_text = furi_string_alloc();
        },
        true);

    return receiver_info;
}

void kia_view_receiver_info_free(KiaReceiverInfo* receiver_info) {
    furi_assert(receiver_info);

    with_view_model(
        receiver_info->view,
        KiaReceiverInfoModel * model,
        {
            furi_string_free(model->protocol_name);
            furi_string_free(model->info_text);
        },
        false);

    view_free(receiver_info->view);
    free(receiver_info);
}

View* kia_view_receiver_info_get_view(KiaReceiverInfo* receiver_info) {
    furi_assert(receiver_info);
    return receiver_info->view;
}

void kia_view_receiver_info_set_protocol_name(
    KiaReceiverInfo* receiver_info,
    const char* protocol_name) {
    furi_assert(receiver_info);
    with_view_model(
        receiver_info->view,
        KiaReceiverInfoModel * model,
        { furi_string_set_str(model->protocol_name, protocol_name); },
        true);
}

void kia_view_receiver_info_set_info_text(KiaReceiverInfo* receiver_info, const char* info_text) {
    furi_assert(receiver_info);
    with_view_model(
        receiver_info->view,
        KiaReceiverInfoModel * model,
        { furi_string_set_str(model->info_text, info_text); },
        true);
}