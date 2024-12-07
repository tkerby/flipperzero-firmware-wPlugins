#include "flipper_spi_terminal_cli.h"
#include "flipper_spi_terminal.h"
#include <toolbox/args.h>

struct FlipperSpiTerminalCliCommand {
    const char* name;
    const char* format;
    const char* description;
    const FlipperSPITerminalCLICallback callback;
};

const char* flipper_spi_terminal_cli_command_get_name(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->name;
}

const char* flipper_spi_terminal_cli_command_get_format(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->format;
}

const char*
    flipper_spi_terminal_cli_command_get_description(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->description;
}

void flipper_spi_terminal_cli_print_usage(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);

    printf(" - " SPI_TERM_CLI_COMMAND " %s", cmd->name);

    if(cmd->format != NULL) {
        printf(" %s", cmd->format);
    }

    if(cmd->description != NULL) {
        printf(":\n\t%s", cmd->description);
    }

    printf("\n\n");
}

#define CLI_COMMAND_FULL_CALLBACK_NAME(cmdName) flipper_spi_terminal_cli_command_##cmdName

// Generate Callbacks for commands
#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation)                       \
    static void CLI_COMMAND_FULL_CALLBACK_NAME(cmdName)(                                         \
        const FlipperSpiTerminalCliCommand* cmd, FlipperSPITerminalApp* app, FuriString* args) { \
        furi_check(cmd);                                                                         \
        SPI_TERM_LOG_D("Executing command: %s\n\t%s", cmd->name, cmd->description);              \
        furi_check(app);                                                                         \
        furi_check(args);                                                                        \
        cmdImplementation                                                                        \
    }
#include "flipper_spi_terminal_cli_commands.h"
#undef CLI_COMMAND

#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation) \
    {                                                                      \
        .name = #cmdName,                                                  \
        .format = cmdFormat,                                               \
        .description = cmdDescription,                                     \
        .callback = CLI_COMMAND_FULL_CALLBACK_NAME(cmdName),               \
    },
static const FlipperSpiTerminalCliCommand commands[] = {
#include "flipper_spi_terminal_cli_commands.h"
};
#undef CLI_COMMAND

static void flipper_spi_terminal_cli_command(Cli* cli, FuriString* args, void* context) {
    SPI_TERM_LOG_T("Received CLI command %s", furi_string_get_cstr(args));
    furi_check(cli);
    furi_check(args);
    furi_check(context);

    FlipperSPITerminalApp* app = context;

    bool executed = false;
    FuriString* cmd = furi_string_alloc();
    args_read_string_and_trim(args, cmd);
    for(size_t i = 0; i < COUNT_OF(commands); i++) {
        const FlipperSpiTerminalCliCommand* cli_cmd = &commands[i];
        if(furi_string_equal_str(cmd, cli_cmd->name)) {
            SPI_TERM_LOG_T("Found command %s!", cli_cmd->name);
            cli_cmd->callback(cli_cmd, app, args);
            executed = true;
            break;
        }
    }
    furi_string_free(cmd);

    if(!executed) {
        SPI_TERM_LOG_T("Command not found! Printing usage...");
        for(size_t i = 0; i < COUNT_OF(commands); i++) {
            const FlipperSpiTerminalCliCommand* cli_cmd = &commands[i];
            flipper_spi_terminal_cli_print_usage(cli_cmd);
        }
    }
}

void flipper_spi_terminal_cli_alloc(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Alloc CLI");
    furi_check(app);

    SPI_TERM_LOG_T("Open CLI");
    app->cli = furi_record_open(RECORD_CLI);

    cli_add_command(
        app->cli,
        SPI_TERM_CLI_COMMAND,
        CliCommandFlagParallelSafe,
        flipper_spi_terminal_cli_command,
        app);
}

void flipper_spi_terminal_cli_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Free CLI");
    furi_check(app);

    SPI_TERM_LOG_T("Closing CLI");
    furi_record_close(RECORD_CLI);
}
