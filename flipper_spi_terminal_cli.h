#pragma once

#include "flipper_spi_terminal.h"

#define SPI_TERM_CLI_COMMAND "spi"

void flipper_spi_terminal_cli_alloc(FlipperSPITerminalApp* app);
void flipper_spi_terminal_cli_free(FlipperSPITerminalApp* app);
