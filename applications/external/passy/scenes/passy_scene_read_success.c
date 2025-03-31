#include "../passy_i.h"
#include <dolphin/dolphin.h>

#define TAG "PassySceneReadCardSuccess"

void passy_scene_read_success_on_enter(void* context) {
    Passy* passy = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);
    notification_message(passy->notifications, &sequence_success);

    furi_string_reset(passy->text_box_store);
    FuriString* str = passy->text_box_store;
    furi_string_cat_printf(str, "%s\n", bit_buffer_get_data(passy->DG1) + 3);

    text_box_set_font(passy->text_box, TextBoxFontText);
    text_box_set_text(passy->text_box, furi_string_get_cstr(passy->text_box_store));
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewTextBox);
}

bool passy_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void passy_scene_read_success_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    text_box_reset(passy->text_box);
}
