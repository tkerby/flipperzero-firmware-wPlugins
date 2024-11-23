#include "../flipper_spi_terminal.h"
#include "scenes.h"

typedef enum {
    FlipperSPITerminalAppTerminalEventReceived
} FlipperSPITerminalAppTerminalEvent;

void flipper_spi_terminal_scene_terminal_alloc(FlipperSPITerminalApp* app) {
    app->terminalScreen.view = text_box_alloc();

    app->terminalScreen.receive_buffer = furi_stream_buffer_alloc(128, 64);

    app->terminalScreen.output_string_buffer = furi_string_alloc();
    furi_string_reserve(app->terminalScreen.output_string_buffer, SPI_TERM_OUTPUT_BUFFER_SIZE);

    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneTerminal,
        text_box_get_view(app->terminalScreen.view));
}

void flipper_spi_terminal_scene_terminal_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Free Terminal Screen");

    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);

    furi_string_free(app->terminalScreen.output_string_buffer);

    furi_stream_buffer_free(app->terminalScreen.receive_buffer);

    text_box_free(app->terminalScreen.view);
}

int32_t spi_worker(void* context) {
    furi_check(context);
    FlipperSPITerminalApp* app = context;
    SPI_TERM_LOG_T("Starting worker...");

    SPI_TERM_LOG_T("Init SPI-Bus handle");
    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);

    // Override the default SPI configuration for the R-Bus.
    // This will be reset as soon as furi_hal_spi_acquire is beeing called.
    SPI_TERM_LOG_T("Init SPI-Settings");
    LL_SPI_Disable(furi_hal_spi_bus_handle_external.bus->spi);
    LL_SPI_Init(furi_hal_spi_bus_handle_external.bus->spi, &app->config.spi);
    LL_SPI_Enable(furi_hal_spi_bus_handle_external.bus->spi);

    SPI_TERM_LOG_T("Starting loop...");
    SPI_TypeDef* spi = furi_hal_spi_bus_handle_external.bus->spi;

    while(app->terminalScreen.spi_thread_continue) {
        SPI_TERM_LOG_T("loop");
        if(LL_SPI_IsActiveFlag_RXNE(spi)) {
            uint8_t d = LL_SPI_ReceiveData8(spi);

            FURI_LOG_E("SPI", "%d", d);

            furi_stream_buffer_send(app->terminalScreen.receive_buffer, &d, 1, FuriWaitForever);

            view_dispatcher_send_custom_event(
                app->view_dispatcher, FlipperSPITerminalAppTerminalEventReceived);
        }
    }

    SPI_TERM_LOG_T("Stopping worker...");

    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);

    SPI_TERM_LOG_T("Stopped worker!");

    return 0;
}

void flipper_spi_terminal_scene_terminal_on_enter(void* context) {
    FlipperSPITerminalApp* app = context;

    furi_string_reset(app->terminalScreen.output_string_buffer);
    text_box_set_text(
        app->terminalScreen.view, furi_string_get_cstr(app->terminalScreen.output_string_buffer));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneTerminal);

    furi_stream_buffer_reset(app->terminalScreen.receive_buffer);

    app->terminalScreen.spi_thread_continue = true;
    app->terminalScreen.spi_thread = furi_thread_alloc_ex("SPI-Worker", 1024, spi_worker, app);
    furi_thread_start(app->terminalScreen.spi_thread);
}

bool flipper_spi_terminal_scene_terminal_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    FlipperSPITerminalApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlipperSPITerminalAppTerminalEventReceived) {
            for(size_t i = 0; i < 3; i++) {
                uint8_t buffer[32];
                size_t count = furi_stream_buffer_receive(
                    app->terminalScreen.receive_buffer, buffer, sizeof(buffer) - 1, 0);
                if(count == 0) break;

                buffer[count] = 0;

                furi_string_cat_printf(app->terminalScreen.output_string_buffer, "%s", buffer);
            }

            text_box_set_text(
                app->terminalScreen.view,
                furi_string_get_cstr(app->terminalScreen.output_string_buffer));

            text_box_set_focus(app->terminalScreen.view, TextBoxFocusEnd);

            return true;
        }
    }

    return false;
}

void flipper_spi_terminal_scene_terminal_on_exit(void* context) {
    FlipperSPITerminalApp* app = context;

    app->terminalScreen.spi_thread_continue = false;
    furi_thread_join(app->terminalScreen.spi_thread);
    furi_thread_free(app->terminalScreen.spi_thread);
}
