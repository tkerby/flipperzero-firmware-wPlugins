#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

#include <notification/notification_messages.h>
#include <rpc/rpc_app.h>

#include <furi_hal_bt.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>

#include <furi_hal_region.h>

#define SPI_TERM_LOG(level, format, ...) \
    furi_log_print_format(level, "SPI Terminal", "%s: " format, __func__, ##__VA_ARGS__)

#define SPI_TERM_LOG_E(format, ...) SPI_TERM_LOG(FuriLogLevelError, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_W(format, ...) SPI_TERM_LOG(FuriLogLevelWarn, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_I(format, ...) SPI_TERM_LOG(FuriLogLevelInfo, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_D(format, ...) SPI_TERM_LOG(FuriLogLevelDebug, format, ##__VA_ARGS__)
#define SPI_TERM_LOG_T(format, ...) SPI_TERM_LOG(FuriLogLevelTrace, format, ##__VA_ARGS__)

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    DialogEx* mainScreen;
    VariableItemList* configScreen;
    TextBox* terminalScreen;
} FlipperSPITerminalApp;
