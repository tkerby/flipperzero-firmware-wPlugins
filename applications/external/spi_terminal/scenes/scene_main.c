#include "scenes.h"

void flipper_spi_terminal_scene_main_alloc(FlipperSPITerminalApp* app) {
    app->mainScreen = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlipperSPITerminalAppSceneMain, dialog_ex_get_view(app->mainScreen));
}

void flipper_spi_terminal_scene_main_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneMain);
    dialog_ex_free(app->mainScreen);
}

static void
    flipper_spi_terminal_scene_main_dialog_result_callback(DialogExResult result, void* context) {
    FlipperSPITerminalApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void flipper_spi_terminal_scene_main_on_enter(void* context) {
    FlipperSPITerminalApp* app = context;

    dialog_ex_set_context(app->mainScreen, app);

    dialog_ex_set_left_button_text(app->mainScreen, "Config");
    dialog_ex_set_center_button_text(app->mainScreen, "Run");

    dialog_ex_set_result_callback(
        app->mainScreen, flipper_spi_terminal_scene_main_dialog_result_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneMain);
}

bool flipper_spi_terminal_scene_main_on_event(void* context, SceneManagerEvent event) {
    FlipperSPITerminalApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneConfig);
            return true;
        } else if(event.event == DialogExResultCenter) {
            scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneTerminal);
            return true;
        }
    }

    return false;
}

void flipper_spi_terminal_scene_main_on_exit(void* context) {
    UNUSED(context);
}
