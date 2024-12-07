#include "flipper_spi_terminal_cli.h"

void flipper_spi_terminal_cli_alloc(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Alloc CLI");
    furi_check(app);

    SPI_TERM_LOG_T("Open CLI");
    app->cli = furi_record_open(RECORD_CLI);
}

void flipper_spi_terminal_cli_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Free CLI");
    furi_check(app);

    SPI_TERM_LOG_T("Closing CLI");
    furi_record_close(RECORD_CLI);
}
