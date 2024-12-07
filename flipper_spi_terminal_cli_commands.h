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

CLI_COMMAND(dbg_echo, "<text>", "(DEBUG) prints", { printf("%s", furi_string_get_cstr(args)); })
