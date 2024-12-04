#include "scenes.h"
#include "../flipper_spi_terminal.h"

void flipper_spi_terminal_scene_about_alloc(FlipperSPITerminalApp* app) {
    app->about_screen = text_box_alloc();

    text_box_set_text(
        app->about_screen,
        "SPI TERMINAL\n"
        "\n"
        "By JAN WIESEMANN.de\n"
        "https://github.com/janwiesemann/flipper-spi-terminal\n"
        "\n"
        "SPI TERMINAL is a SPI App, which allows you to control external devices using SPI. Your Flipper can act as a SPI Master or Slave device. The Slave mode allows you to sniff the communication between different SPI peripherals.\n"
        "\n"
        "The App uses the Low-Level SPI Interface of the STM32WB55RG Microprocessor. All data is transmitted with DMA Sub-module and can reach speeds of up to 32 Mbit/s in Master and up to 24 Mbit/s in Slave mode.");

    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneAbout,
        text_box_get_view(app->about_screen));
}

void flipper_spi_terminal_scene_about_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneAbout);
    text_box_free(app->about_screen);
}

void flipper_spi_terminal_scene_about_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneAbout);
}

bool flipper_spi_terminal_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_about_on_exit(void* context) {
    UNUSED(context);
}
