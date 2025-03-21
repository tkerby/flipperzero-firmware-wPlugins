#include "../mizip_balance_editor.h"

void mizip_balance_editor_app_number_input_callback(void* context, uint32_t index) {
    MiZipBalanceEditorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void mizip_balance_editor_scene_number_input_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdNumberInput);
}

bool mizip_balance_editor_scene_number_input_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void mizip_balance_editor_scene_number_input_on_exit(void* context) {
    UNUSED(context);
}
