#include "ami_tool_i.h"

void ami_tool_scene_saved_on_enter(void* context) {
    AmiToolApp* app = context;

    furi_string_printf(app->text_box_store,
                       "Saved\n\nPlaceholder screen for\nsaved figures or profiles.");

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

bool ami_tool_scene_saved_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;
    UNUSED(app);

    if(event.type == SceneManagerEventTypeBack) {
        return false; /* default behaviour: previous scene */
    }

    return false;
}

void ami_tool_scene_saved_on_exit(void* context) {
    AmiToolApp* app = context;
    UNUSED(app);
}
