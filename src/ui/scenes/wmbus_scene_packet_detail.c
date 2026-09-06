#include "../../app/wmbus_app_i.h"

void wmbus_scene_packet_detail_on_enter(void* context) {
    WmBusApp* app = context;
    if(!app || !wmbus_app_ensure_widget(app)) {
        return;
    }
    FuriString* detail = furi_string_alloc();
    if(!detail) {
        return;
    }

    widget_reset(app->widget);
    bool has_detail =
        wmbus_rx_view_build_selected_text(app->rx_view, detail, app->detail_application_only);
    if(!has_detail) {
        furi_string_set(detail, "No packet selected.");
    }
    widget_add_text_scroll_element(app->widget, 0U, 0U, 128U, 64U, furi_string_get_cstr(detail));
    furi_string_free(detail);
    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewWidget);
}

bool wmbus_scene_packet_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wmbus_scene_packet_detail_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->widget) return;
    widget_reset(app->widget);
}
