#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_scene_file_select_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    //Resetting data
    furi_string_set(app->filePath, NFC_APP_FOLDER);
    mf_classic_reset(app->mf_classic_data);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = NFC_APP_FOLDER;
    browser_options.hide_dot_files = true;
    //Show file browser
    if(!dialog_file_browser_show(app->dialogs, app->filePath, app->filePath, &browser_options)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    //Check if file is MiZip file
    app->nfc_device = nfc_device_alloc();
    if(nfc_device_load(app->nfc_device, furi_string_get_cstr(app->filePath))) {
        if(nfc_device_get_protocol(app->nfc_device) == NfcProtocolMfClassic) {
            mf_classic_copy(
                app->mf_classic_data, nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic));
            app->is_valid_mizip_data = mizip_verify(context);
            nfc_device_free(app->nfc_device);
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowResult);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
    }
}

bool mizip_balance_editor_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void mizip_balance_editor_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
