// scenes/kia_decoder_scene_about.c
#include "../kia_decoder_app_i.h"

void kia_decoder_scene_about_on_enter(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;

    widget_add_string_multiline_element(
        app->widget,
        0,
        0,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "ProtoPirate");

    widget_add_string_multiline_element(
        app->widget,
        0,
        12,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Kia protocols by RocketGod\n"
        "L0rdDiakon & Youngz\n\n\n"
        "App by RocketGod");

    widget_add_string_element(
        app->widget,
        0,
        64 - 12,
        AlignLeft,
        AlignBottom,
        FontSecondary,
        "Version: 1.0");

    view_dispatcher_switch_to_view(app->view_dispatcher, KiaDecoderViewWidget);
}

bool kia_decoder_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void kia_decoder_scene_about_on_exit(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    widget_reset(app->widget);
}