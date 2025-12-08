// scenes/protopirate_scene_about.c
#include "../protopirate_app_i.h"

void protopirate_scene_about_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;

    widget_add_string_multiline_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "ProtoPirate");

    widget_add_string_multiline_element(
        app->widget,
        0,
        12,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Kia protocols by RocketGod\n"
        "L0rdDiakon & YougZ\n\n\n"
        "App by RocketGod & MMX");

    widget_add_string_element(
        app->widget, 0, 64 - 12, AlignLeft, AlignBottom, FontSecondary, "Version: 1.1");

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void protopirate_scene_about_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    widget_reset(app->widget);
}
