#include "../../app/wmbus_app_i.h"

#ifndef WMBUS_GIT_COMMIT
#define WMBUS_GIT_COMMIT "unknown"
#endif

void wmbus_scene_about_on_enter(void* context) {
    WmBusApp* app = context;
    if(!app || !wmbus_app_ensure_widget(app)) {
        return;
    }

    widget_reset(app->widget);
    widget_add_text_box_element(
        app->widget,
        4U,
        2U,
        120U,
        60U,
        AlignLeft,
        AlignTop,
        "\e#WM-Bus Decoder\e#\n"
        "Version: 1.0.0 (" WMBUS_GIT_COMMIT ")\n"
        "Author: Martin Mares\n"
        "https://github.com/maresmar/wmbus-decoder\n",
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewWidget);
}

bool wmbus_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wmbus_scene_about_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->widget) return;
    widget_reset(app->widget);
}
