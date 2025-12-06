// scenes/kia_decoder_scene_receiver_info.c
#include "../kia_decoder_app_i.h"

void kia_decoder_scene_receiver_info_on_enter(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;

    FuriString* text;
    text = furi_string_alloc();

    kia_history_get_text_item_menu(app->txrx->history, text, app->txrx->idx_menu_chosen);
    
    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));

    furi_string_reset(text);
    kia_history_get_text_item(app->txrx->history, text, app->txrx->idx_menu_chosen);

    widget_add_string_multiline_element(
        app->widget, 0, 14, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));

    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, KiaDecoderViewWidget);
}

bool kia_decoder_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void kia_decoder_scene_receiver_info_on_exit(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    widget_reset(app->widget);
}