#pragma once

#include <lib/toolbox/value_index.h>
#include "../flipper_spi_terminal_app.h"

size_t value_index_display_mode(
    const FlipperSPITerminalAppDisplayMode value,
    const FlipperSPITerminalAppDisplayMode values[],
    size_t values_count);

size_t value_index_size_t(const size_t value, const size_t values[], size_t values_count);
