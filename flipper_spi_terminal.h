#pragma once

#include <furi.h>
#include <rpc/rpc_app.h>

#define SPI_TERM_LOG(level, format, ...) \
    furi_log_print_format(level, "SPI Terminal", "%s: " format, __func__, ##__VA_ARGS__)

#define SPI_TERM_LOG_E(format, ...) SPI_TERM_LOG(FuriLogLevelError, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_W(format, ...) SPI_TERM_LOG(FuriLogLevelWarn, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_I(format, ...) SPI_TERM_LOG(FuriLogLevelInfo, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_D(format, ...) SPI_TERM_LOG(FuriLogLevelDebug, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_T(format, ...) SPI_TERM_LOG(FuriLogLevelTrace, format, ##__VA_ARGS__)

#include "flipper_spi_terminal_app.h"
