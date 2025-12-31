#include "../application.h"
#include "../tonuino_nfc.h"
#include "scenes.h"

void tonuino_scene_read_card_waiting_on_enter(void* context) {
    TonuinoApp* app = context;

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for card...");
    widget_add_string_element(
        app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Place card on back");

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);

    // Perform NFC read
    if(tonuino_read_card(app)) {
        notification_message(app->notifications, &sequence_success);
        scene_manager_next_scene(app->scene_manager, TonuinoSceneReadCardResult);
    } else {
        notification_message(app->notifications, &sequence_error);
        // Stay on waiting scene and show error
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "Read Failed!");
        widget_add_string_element(
            app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "No TonUINO data");
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "found on card");
    }
}

bool tonuino_scene_read_card_waiting_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_read_card_waiting_on_exit(void* context) {
    TonuinoApp* app = context;
    widget_reset(app->widget);
}
