#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_about_on_enter(void* context) {
    NfcDictManager* app = context;

    furi_string_reset(app->text_box_content);
    furi_string_cat_str(
        app->text_box_content,
        "NFC Dict Manager v1.0\n\n"
        "Created by: grugnoymeme\n"
        "a.k.a. 47LeCoste\n"
        "GitHub:\n"
        "github.com/grugnoymeme\n"
        "USAGE:\n"
        "- Backup: Save current dicts\n"
        "- Select: Choose dict for system/user\n"
        "- Merge: Combine two dicts\n"
        "- Optimize: Clean invalid keys\n"
        "Custo dicts dir:\n"
        "/ext/nfc/dictionaries/\n"
        "Keys format: 0-9, A-F, max 12 chars, UPPERCASE");

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_content));
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewTextBox);
}

bool nfc_dict_manager_scene_about_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_about_on_exit(void* context) {
    NfcDictManager* app = context;
    text_box_reset(app->text_box);
}
