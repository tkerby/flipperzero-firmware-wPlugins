#include "../nfc_eink_app.h"

void nfc_eink_scene_exit_confirm_dialog_callback(DialogExResult result, void* context) {
    NfcEinkApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, result);
}

void nfc_eink_scene_exit_confirm_on_enter(void* context) {
    NfcEinkApp* instance = context;
    DialogEx* dialog_ex = instance->dialog_ex;

    dialog_ex_set_left_button_text(dialog_ex, "Exit");
    dialog_ex_set_right_button_text(dialog_ex, "Stay");
    dialog_ex_set_header(dialog_ex, "Exit to Main Menu?", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(dialog_ex, "All unsaved data will be lost", 64, 12, AlignCenter, AlignTop);
    dialog_ex_set_context(dialog_ex, instance);
    dialog_ex_set_result_callback(dialog_ex, nfc_eink_scene_exit_confirm_dialog_callback);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewDialogEx);
}

bool nfc_eink_scene_exit_confirm_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultRight) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == DialogExResultLeft) {
            nfc_eink_screen_free(instance->screen);
            instance->screen = NULL;
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcEinkAppSceneStart);
            /*  if(scene_manager_has_previous_scene(nfc->scene_manager, NfcSceneMfClassicDictAttack) &&
               (scene_manager_has_previous_scene(nfc->scene_manager, NfcSceneReadMenu) ||
                scene_manager_has_previous_scene(nfc->scene_manager, NfcSceneSavedMenu))) {
                const uint32_t possible_scenes[] = {NfcSceneReadMenu, NfcSceneSavedMenu};
                consumed = scene_manager_search_and_switch_to_previous_scene_one_of(
                    nfc->scene_manager, possible_scenes, COUNT_OF(possible_scenes));
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    nfc->scene_manager, NfcSceneStart);
            } */
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_exit_confirm_on_exit(void* context) {
    NfcEinkApp* instance = context;

    // Clean view
    dialog_ex_reset(instance->dialog_ex);
}
