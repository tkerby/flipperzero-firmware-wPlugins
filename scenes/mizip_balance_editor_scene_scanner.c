#include "../mizip_balance_editor.h"

void mizip_balance_editor_scene_scanner_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    popup_set_header(
        app->popup, "Apply MiZip\n  tag to the\n      back", 65, 30, AlignLeft, AlignTop);
    popup_set_icon(app->popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdScanner);
}

bool mizip_balance_editor_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void mizip_balance_editor_scene_scanner_on_exit(void* context) {
    UNUSED(context);
}
