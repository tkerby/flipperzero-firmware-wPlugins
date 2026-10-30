#pragma once

#include <furi.h>

/* Firmware-owned session interface. No callback or context points into the GUI FAP. */
#define RECORD_CLI_GUI_BRIDGE      "cli_gui_bridge"
#define CLI_GUI_BRIDGE_API_VERSION 1U

typedef struct {
    uint32_t version;
    bool (*open)(size_t output_capacity, size_t input_capacity);
    size_t (*available)(void);
    size_t (*receive)(void* data, size_t size);
    size_t (*send)(const void* data, size_t size);
    void (*close)(bool persist);
} CliGuiBridge;
