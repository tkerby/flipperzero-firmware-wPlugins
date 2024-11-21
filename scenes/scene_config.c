#include "scenes.h"
#include <lib/toolbox/value_index.h>

#define UNWRAP_ARGS(...) __VA_ARGS__

//Value array generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    const type spi_config_##name##_values[] = {UNWRAP_ARGS values};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

//Value string array generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    const char* const spi_config_##name##_strings[] = {UNWRAP_ARGS strings};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

//Value changed callback generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    static void name##_changed(VariableItem* item) {                                          \
        FlipperSPITerminalApp* app = (variable_item_get_context)(item);                       \
        uint8_t index = (variable_item_get_current_value_index)(item);                        \
        (variable_item_set_current_value_text)(item, spi_config_##name##_strings[index]);     \
        app->spi_config.spiTypedefField = spi_config_##name##_values[index];                  \
    }
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

//Add item generation
#define ADD_CONFIG_ENTRY(label, name, type, valueIndexFunc, spiTypedefField, values, strings) \
    item = variable_item_list_add(                                                            \
        app->configScreen, label, COUNT_OF(spi_config_##name##_values), name##_changed, app); \
    value_index = (valueIndexFunc)(app->spi_config.spiTypedefField,                           \
                                   spi_config_##name##_values,                                \
                                   COUNT_OF(spi_config_##name##_values));                     \
    variable_item_set_current_value_index(item, value_index);                                 \
    variable_item_set_current_value_text(item, spi_config_##name##_strings[value_index]);

static void flipper_spi_terminal_scene_config_alloc_items(FlipperSPITerminalApp* app) {
    VariableItem* item;
    size_t value_index;

    UNUSED(item);
    UNUSED(value_index);

#include "scene_config_values_config.h"
}
#undef ADD_CONFIG_ENTRY

void flipper_spi_terminal_scene_config_alloc(FlipperSPITerminalApp* app) {
    app->configScreen = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneConfig,
        variable_item_list_get_view(app->configScreen));

    flipper_spi_terminal_scene_config_alloc_items(app);
}

void flipper_spi_terminal_scene_config_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
    variable_item_list_free(app->configScreen);
}

void flipper_spi_terminal_scene_config_on_enter(void* context) {
    FlipperSPITerminalApp* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
}

bool flipper_spi_terminal_scene_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_config_on_exit(void* context) {
    UNUSED(context);
}
