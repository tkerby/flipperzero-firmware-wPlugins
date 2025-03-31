#include "../passy_i.h"
#include <dolphin/dolphin.h>

#define TAG "PassySceneReadCardSuccess"

void passy_scene_read_error_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Passy* passy = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(passy->view_dispatcher, result);
    }
}

void passy_scene_read_error_on_enter(void* context) {
    Passy* passy = context;
    Widget* widget = passy->widget;

    // Send notification
    notification_message(passy->notifications, &sequence_error);
    FuriString* primary_str = furi_string_alloc_set("Read Errror");
    FuriString* secondary_str = furi_string_alloc_set("Try again?");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", passy_scene_read_error_widget_callback, passy);

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(primary_str));

    widget_add_string_element(
        widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(secondary_str));

    furi_string_free(primary_str);
    furi_string_free(secondary_str);
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewWidget);
}

bool passy_scene_read_error_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(passy->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void passy_scene_read_error_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    widget_reset(passy->widget);
}
