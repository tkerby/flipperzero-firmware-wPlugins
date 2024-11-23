#include "flipper_spi_terminal.h"
#include <furi_hal_rtc.h>
#include "scenes/scenes.h"

static bool flipper_spi_terminal_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipperSPITerminalApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flipper_spi_terminal_back_event_callback(void* context) {
    furi_assert(context);
    FlipperSPITerminalApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void flipper_spi_terminal_tick_event_callback(void* context) {
    furi_assert(context);
    FlipperSPITerminalApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static FlipperSPITerminalApp* flipper_spi_terminal_alloc(void) {
    SPI_TERM_LOG_T("Alloc App");
    FlipperSPITerminalApp* app = malloc(sizeof(FlipperSPITerminalApp));

    app->config.print_mode = FlipperSPITerminalAppPrintModeDynamic;
    app->config.spi.Mode = LL_SPI_MODE_SLAVE;
    app->config.spi.TransferDirection = LL_SPI_FULL_DUPLEX;
    app->config.spi.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    app->config.spi.ClockPolarity = LL_SPI_POLARITY_LOW;
    app->config.spi.ClockPhase = LL_SPI_PHASE_1EDGE;
    app->config.spi.NSS = LL_SPI_NSS_SOFT;
    app->config.spi.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV32;
    app->config.spi.BitOrder = LL_SPI_MSB_FIRST;
    app->config.spi.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    app->config.spi.CRCPoly = 7;

    SPI_TERM_LOG_T("Open GUI");
    app->gui = furi_record_open(RECORD_GUI);

    SPI_TERM_LOG_T("Alloc Scene Manager");
    app->scene_manager = scene_manager_alloc(&flipper_spi_terminal_scene_handlers, app);

    SPI_TERM_LOG_T("Alloc Dispatcher!");
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipper_spi_terminal_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipper_spi_terminal_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flipper_spi_terminal_tick_event_callback, 100);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    flipper_spi_terminal_scenes_alloc(app);

    SPI_TERM_LOG_T("Alloc Terminal Screen!");

    return app;
}

static void flipper_spi_terminal_free(FlipperSPITerminalApp* app) {
    flipper_spi_terminal_scenes_free(app);

    SPI_TERM_LOG_T("Free Scene Manager");
    scene_manager_free(app->scene_manager);

    SPI_TERM_LOG_T("Free Dispatcher");
    view_dispatcher_free(app->view_dispatcher);

    SPI_TERM_LOG_T("Close GUI");
    furi_record_close(RECORD_GUI);

    SPI_TERM_LOG_T("Free App");
    free(app);
}

int32_t flipper_spi_terminal_main(void* args) {
    UNUSED(args);

    FlipperSPITerminalApp* app = flipper_spi_terminal_alloc();

    scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneMain);

    view_dispatcher_run(app->view_dispatcher);

    flipper_spi_terminal_free(app);

    return 0;
}
