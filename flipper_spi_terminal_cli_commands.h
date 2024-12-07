#ifndef CLI_COMMAND
#include "flipper_spi_terminal_cli.h"
#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation)                       \
    void cmdName(                                                                                \
        const FlipperSpiTerminalCliCommand* cmd, FlipperSPITerminalApp* app, FuriString* args) { \
        UNUSED(cmd);                                                                             \
        UNUSED(app);                                                                             \
        UNUSED(args);                                                                            \
        cmdImplementation                                                                        \
    }
#endif

#ifndef CLI_COMMANDS_ADDED_INCLUDES
#include "views/terminal_view.h"
#define CLI_COMMANDS_ADDED_INCLUDES
#endif

CLI_COMMAND(dbg_term_set_data, "<text>", "(DEBUG) Sets the data of the terminal view to <text>", {
    if(app->terminal_screen.is_active) {
        const char* str = furi_string_get_cstr(args);
        const size_t length = furi_string_size(args);

        terminal_view_reset(app->terminal_screen.view);
        furi_stream_buffer_send(
            app->terminal_screen.rx_buffer_stream, str, length, FuriWaitForever);

        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipperSPITerminalEventReceivedData);
    } else {
        printf("Non on terminal screen!");
    }
})
CLI_COMMAND(dbg_term_buffer, NULL, "(DEBUG) prints the current buffer", {
    terminal_view_debug_print_buffer(app->terminal_screen.view);
})
