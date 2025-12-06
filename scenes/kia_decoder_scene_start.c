// scenes/kia_decoder_scene_start.c
#include "../kia_decoder_app_i.h"

typedef enum {
    SubmenuIndexKiaReceiver,
    SubmenuIndexKiaReceiverConfig,
    SubmenuIndexKiaAbout,
} SubmenuIndex;

static void kia_decoder_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void kia_decoder_scene_start_on_enter(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;

    submenu_add_item(
        app->submenu,
        "Receive",
        SubmenuIndexKiaReceiver,
        kia_decoder_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Configuration",
        SubmenuIndexKiaReceiverConfig,
        kia_decoder_scene_start_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "About",
        SubmenuIndexKiaAbout,
        kia_decoder_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, KiaDecoderSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, KiaDecoderViewSubmenu);
}

bool kia_decoder_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexKiaAbout) {
            scene_manager_next_scene(app->scene_manager, KiaDecoderSceneAbout);
            consumed = true;
        } else if(event.event == SubmenuIndexKiaReceiver) {
            scene_manager_next_scene(app->scene_manager, KiaDecoderSceneReceiver);
            consumed = true;
        } else if(event.event == SubmenuIndexKiaReceiverConfig) {
            scene_manager_next_scene(app->scene_manager, KiaDecoderSceneReceiverConfig);
            consumed = true;
        }
        scene_manager_set_scene_state(app->scene_manager, KiaDecoderSceneStart, event.event);
    }

    return consumed;
}

void kia_decoder_scene_start_on_exit(void* context) {
    furi_assert(context);
    KiaDecoderApp* app = context;
    submenu_reset(app->submenu);
}