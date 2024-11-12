#include "../flipper_spi_terminal.h"
#include "scenes.h"

void flipper_spi_terminal_scene_config_on_enter(void* context) {
    FlipperSPITerminalApp* app = context;

    /* dialog_ex_set_result_callback(
        app->mainScreen, flipper_spi_terminal_scene_main_dialog_result_callback);*/

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
}

bool flipper_spi_terminal_scene_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_config_on_exit(void* context) {
    UNUSED(context);
}
