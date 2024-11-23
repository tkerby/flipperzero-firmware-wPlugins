#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>
#include <furi_hal_spi_types.h>

typedef enum {
    FlipperSPITerminalAppPrintModeDynamic,
    FlipperSPITerminalAppPrintModeASCII,
    FlipperSPITerminalAppPrintModeHex,
    FlipperSPITerminalAppPrintModeBinary,

    FlipperSPITerminalAppPrintModeMax,
} FlipperSPITerminalAppPrintMode;

typedef struct {
    LL_SPI_InitTypeDef spi;
    FlipperSPITerminalAppPrintMode print_mode;
} FlipperSPITerminalAppConfig;

typedef struct {
    TextBox* view;
    bool spi_thread_continue;
    FuriThread* spi_thread;
    FuriString* output_string_buffer;
    FuriStreamBuffer* receive_buffer;
} FlipperSPITerminalAppScreenTerminal;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    DialogEx* mainScreen;

    VariableItemList* configScreen;

    FlipperSPITerminalAppScreenTerminal terminalScreen;

    FlipperSPITerminalAppConfig config;
} FlipperSPITerminalApp;
