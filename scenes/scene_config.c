#include "scenes.h"
#include <lib/toolbox/value_index.h>
#include "../flipper_spi_terminal.h"

#define UNWRAP_ARGS(...) __VA_ARGS__

// Value array generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    const type spi_config_##name##_values[] = {UNWRAP_ARGS values};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Value string array generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    const char* const spi_config_##name##_strings[] = {UNWRAP_ARGS strings};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Value changed callback generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    static void name##_changed(VariableItem* item) {                                          \
        FlipperSPITerminalApp* app = (variable_item_get_context)(item);                       \
        uint8_t index = (variable_item_get_current_value_index)(item);                        \
        (variable_item_set_current_value_text)(item, spi_config_##name##_strings[index]);     \
        app->config.spi.spiTypedefField = spi_config_##name##_values[index];                  \
    }
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Add item generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    item = variable_item_list_add(                                                            \
        app->configScreen, label, COUNT_OF(spi_config_##name##_values), name##_changed, app); \
    value_index = (valueIndexFunc)(app->config.spi.spiTypedefField,                           \
                                   spi_config_##name##_values,                                \
                                   COUNT_OF(spi_config_##name##_values));                     \
    variable_item_set_current_value_index(item, value_index);                                 \
    name##_changed(item);

static void flipper_spi_terminal_scene_config_alloc_spi_config_items(FlipperSPITerminalApp* app) {
    VariableItem* item;
    size_t value_index;

    UNUSED(item);
    UNUSED(value_index);

#include "scene_config_values_config.h"
}
#undef ADD_CONFIG_ENTRY

const char* const spi_config_display_mode_strings[FlipperSPITerminalAppPrintModeMax] = {
    [FlipperSPITerminalAppPrintModeDynamic] = "Dynamic",
    [FlipperSPITerminalAppPrintModeASCII] = "ASCII",
    [FlipperSPITerminalAppPrintModeHex] = "Hex",
    [FlipperSPITerminalAppPrintModeBinary] = "Binary",
};

const size_t spi_config_dma_buffer_size_values[] = {
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
};

void display_mode_changed(VariableItem* item) {
    furi_check(item);
    SPI_TERM_CONTEXT_TO_APP(variable_item_get_context(item));
    app->config.print_mode =
        (FlipperSPITerminalAppPrintMode)variable_item_get_current_value_index(item);

    furi_check(app->config.print_mode < FlipperSPITerminalAppPrintModeMax);

    const char* str = spi_config_display_mode_strings[app->config.print_mode];
    variable_item_set_current_value_text(item, str);
}
void dma_buffer_size_changed(VariableItem* item) {
    furi_check(item);
    SPI_TERM_CONTEXT_TO_APP(variable_item_get_context(item));
    uint8_t selectedIndex = variable_item_get_current_value_index(item);
    app->config.rx_dma_buffer_size = spi_config_dma_buffer_size_values[selectedIndex];

    FuriString* str = furi_string_alloc();
    furi_string_cat_printf(str, "%zu", app->config.rx_dma_buffer_size);
    variable_item_set_current_value_text(item, furi_string_get_cstr(str));
}

void flipper_spi_terminal_scene_config_on_center_button(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);

    FURI_LOG_E("SPI", "TO-DO SHOW HELP");
}

void flipper_spi_terminal_scene_config_alloc(FlipperSPITerminalApp* app) {
    furi_check(app);

    app->configScreen = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneConfig,
        variable_item_list_get_view(app->configScreen));

    VariableItem* item;
    size_t value_index;

    // Help
    item = variable_item_list_add(app->configScreen, " - Press OK for help - ", 0, NULL, NULL);

    // Display Mode
    item = variable_item_list_add(
        app->configScreen,
        "Display mode",
        FlipperSPITerminalAppPrintModeMax,
        display_mode_changed,
        app);
    variable_item_set_current_value_index(item, app->config.print_mode);
    display_mode_changed(item);

    // DBA Buffer size
    item = variable_item_list_add(
        app->configScreen,
        "DMA Buffer size",
        COUNT_OF(spi_config_dma_buffer_size_values),
        dma_buffer_size_changed,
        app);
    value_index = value_index_uint32(
        app->config.rx_dma_buffer_size,
        (uint32_t*)&spi_config_dma_buffer_size_values[0],
        COUNT_OF(spi_config_dma_buffer_size_values));
    variable_item_set_current_value_index(item, value_index);
    dma_buffer_size_changed(item);

    // SPI related Settings
    flipper_spi_terminal_scene_config_alloc_spi_config_items(app);

    variable_item_list_set_enter_callback(
        app->configScreen, flipper_spi_terminal_scene_config_on_center_button, app);
}

void flipper_spi_terminal_scene_config_free(FlipperSPITerminalApp* app) {
    furi_check(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
    variable_item_list_free(app->configScreen);
}

void flipper_spi_terminal_scene_config_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
}

bool flipper_spi_terminal_scene_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_config_on_exit(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    SPI_TERM_LOG_D("Print Mode: %s", spi_config_display_mode_strings[app->config.print_mode]);
    SPI_TERM_LOG_D("DMA RX Buffer size: %zu", app->config.rx_dma_buffer_size);

    size_t value_index;
    const char* str;

    UNUSED(value_index);
    UNUSED(str);

#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    value_index = (valueIndexFunc)(app->config.spi.spiTypedefField,                           \
                                   spi_config_##name##_values,                                \
                                   COUNT_OF(spi_config_##name##_values));                     \
    str = spi_config_##name##_strings[value_index];                                           \
    SPI_TERM_LOG_D("SPI " label ": %s", str);
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY
}
