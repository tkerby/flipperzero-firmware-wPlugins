#include "../mizip_balance_editor.h"

void mizip_balance_editor_app_submenu_callback(void* context, uint32_t index) {
    MiZipBalanceEditorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void mizip_balance_editor_scene_main_menu_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    submenu_set_header(app->submenu, "MiZip Balance Editor");
    submenu_add_item(
        app->submenu,
        "TODO Direct to tag",
        SubmenuIndexDirectToTag,
        mizip_balance_editor_app_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Edit MiZip file",
        SubmenuIndexEditMiZipFile,
        mizip_balance_editor_app_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu);
}

bool mizip_balance_editor_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            // Request switch to the Widget view via the custom event queue.
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiZipBalanceEditorViewIdWidget);
        } else {
            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(
                &browser_options, NFC_APP_EXTENSION, &I_Nfc_10px);
            browser_options.base_path = NFC_APP_FOLDER;
            browser_options.hide_dot_files = true;
            //Show file browser
            dialog_file_browser_show(app->dialogs, app->filePath, app->filePath, &browser_options);
            //Check if file is MiZip file
            mizip_balance_editor_load_file(app);
            //Return to view, delete later
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget,
                64,
                32,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                furi_string_get_cstr(app->filePath));
            widget_add_button_element(
                app->widget, GuiButtonTypeCenter, "Return to menu", NULL, app);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiZipBalanceEditorViewIdWidget);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
    }

    return consumed;
}

void mizip_balance_editor_scene_main_menu_on_exit(void* context) {
    UNUSED(context);
}
