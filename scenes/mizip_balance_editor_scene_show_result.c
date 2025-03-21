#include "../mizip_balance_editor.h"

static void
    mizip_balance_editor_scene_confirm_dialog_callback(DialogExResult result, void* context) {
    MiZipBalanceEditorApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void mizip_balance_editor_scene_show_result_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    DialogEx* dialog_ex = app->dialog_ex;

    FuriString* message;
    if(app->valid_file) {
        message = furi_string_alloc_set("Valid file");
    } else {
        message = furi_string_alloc_set("Not a MiZip file");
    }

    dialog_ex_set_header(
        dialog_ex, furi_string_get_cstr(app->filePath), 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(dialog_ex, furi_string_get_cstr(message), 64, 29, AlignCenter, AlignCenter);

    dialog_ex_set_left_button_text(dialog_ex, "-");
    dialog_ex_set_right_button_text(dialog_ex, "+");
    dialog_ex_set_center_button_text(dialog_ex, "Custom value");

    dialog_ex_set_result_callback(dialog_ex, mizip_balance_editor_scene_confirm_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdShowResult);
}

bool mizip_balance_editor_scene_show_result_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void mizip_balance_editor_scene_show_result_on_exit(void* context) {
    UNUSED(context);
}
