#include "ami_tool_i.h"

void ami_tool_scene_read_on_enter(void* context) {
    AmiToolApp* app = context;

    furi_string_printf(app->text_box_store,
                       "Read\n\nThis is the placeholder screen\nfor the NFC read flow.");

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

bool ami_tool_scene_read_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;
    UNUSED(app);

    if(event.type == SceneManagerEventTypeBack) {
        /* Let SceneManager automatically go back to previous scene */
        return false;
    }

    return false;
}

void ami_tool_scene_read_on_exit(void* context) {
    AmiToolApp* app = context;
    UNUSED(app);
}
