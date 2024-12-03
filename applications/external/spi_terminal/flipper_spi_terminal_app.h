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
  FlipperSPITerminalAppDisplayModeAuto,
  FlipperSPITerminalAppDisplayModeHex,
  FlipperSPITerminalAppDisplayModeBinary,

  FlipperSPITerminalAppDisplayModeMax,
} FlipperSPITerminalAppDisplayMode;

typedef struct {
  FlipperSPITerminalAppDisplayMode display_mode;
  size_t rx_dma_buffer_size;
  LL_SPI_InitTypeDef spi;
} FlipperSPITerminalAppConfig;

typedef struct {
  VariableItemList *view;

  TextBox *help_view;
  const char *help_string;
} FlipperSPITerminalAppScreenConfig;

typedef struct {
  TextBox *view;
  FuriString *output_string_buffer;
  FuriString *tmp_buffer;
  unsigned int counter;

  uint8_t *rx_dma_buffer;
  FuriStreamBuffer *rx_buffer_stream;
} FlipperSPITerminalAppScreenTerminal;

typedef struct {
  Gui *gui;
  ViewDispatcher *view_dispatcher;
  SceneManager *scene_manager;

  DialogEx *main_screen;

  FlipperSPITerminalAppScreenConfig config_screen;

  FlipperSPITerminalAppScreenTerminal terminal_screen;

  FlipperSPITerminalAppConfig config;
} FlipperSPITerminalApp;
