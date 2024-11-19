#include "../flipper_spi_terminal.h"
#include "scenes.h"

void flipper_spi_terminal_scene_terminal_alloc(FlipperSPITerminalApp* app) {
    app->terminalScreen = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneTerminal,
        text_box_get_view(app->terminalScreen));
}

void flipper_spi_terminal_scene_terminal_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Free Terminal Screen");
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);
    text_box_free(app->terminalScreen);
}

void flipper_spi_terminal_scene_terminal_on_enter(void* context) {
    FlipperSPITerminalApp* app = context;

    /*furi_hal_spi_bus_handle_init(spi_terminal_spi_handle);
  furi_hal_spi_acquire(spi_terminal_spi_handle);
  LL_SPI_Disable(spi_terminal_spi);
  LL_SPI_Init(spi_terminal_spi, &spi_terminal_preset);
  LL_SPI_Enable(spi_terminal_spi);
*/

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);
}

bool flipper_spi_terminal_scene_terminal_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_terminal_on_exit(void* context) {
    UNUSED(context);

    // furi_hal_spi_bus_handle_deinit(app->spiHandle);
}
