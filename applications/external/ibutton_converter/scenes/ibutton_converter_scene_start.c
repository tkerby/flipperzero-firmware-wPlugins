#include "../ibutton_converter_i.h"
#include "ibutton_converter_scene.h"
#include <dolphin/dolphin.h>

enum SubmenuIndex {
    SubmenuIndexSelectFile,
};

void ibutton_converter_scene_start_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    Submenu* submenu = ibutton_converter->submenu;

    ibutton_converter_reset_key(ibutton_converter);

    submenu_add_item(
        submenu,
        "Select file",
        SubmenuIndexSelectFile,
        ibutton_converter_submenu_callback,
        ibutton_converter);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(
            ibutton_converter->scene_manager, iButtonConverterSceneStart));

    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewSubmenu);
}

bool ibutton_converter_scene_start_on_event(void* context, SceneManagerEvent event) {
    iButtonConverter* ibutton_converter = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            ibutton_converter->scene_manager, iButtonConverterSceneStart, event.event);
        consumed = true;
        if(event.event == SubmenuIndexSelectFile) {
            scene_manager_next_scene(
                ibutton_converter->scene_manager, iButtonConverterSceneSelectKey);
        }
    }

    return consumed;
}

void ibutton_converter_scene_start_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    submenu_reset(ibutton_converter->submenu);
}
