#include "../application.h"
#include "../tonuino_nfc.h"
#include "scenes.h"

void tonuino_scene_write_card_waiting_on_enter(void* context) {
    TonuinoApp* app = context;

    // CRITICAL: Reset widget state before use
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for card...");
    widget_add_string_element(
        app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Place card on back");

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewWidget);

    // Perform NFC write
    if(tonuino_write_card(app)) {
        notification_message(app->notifications, &sequence_success);
        scene_manager_next_scene(app->scene_manager, TonuinoSceneWriteCardResult);
    } else {
        notification_message(app->notifications, &sequence_error);
        // Stay on waiting scene and show error
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Write Failed!");
        widget_add_string_element(
            app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Card not detected");
    }
}

bool tonuino_scene_write_card_waiting_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_write_card_waiting_on_exit(void* context) {
    TonuinoApp* app = context;
    widget_reset(app->widget);
}
