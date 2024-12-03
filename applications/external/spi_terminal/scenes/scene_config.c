#include "scenes.h"
#include <lib/toolbox/value_index.h>
#include "../flipper_spi_terminal.h"

static size_t value_index_display_mode(
    const FlipperSPITerminalAppDisplayMode value,
    const FlipperSPITerminalAppDisplayMode values[],
    size_t values_count) {
    size_t index = 0;

    for(size_t i = 0; i < values_count; i++) {
        if(value == values[i]) {
            index = i;
            break;
        }
    }

    return index;
}

inline static size_t
    value_index_size_t(const size_t value, const size_t values[], size_t values_count) {
    return value_index_uint32(value, (uint32_t*)values, values_count);
}

#define UNWRAP_ARGS(...) __VA_ARGS__

// Value array generation
#define ADD_CONFIG_ENTRY(                                                              \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings) \
    const type spi_config_##name##_values[] = {UNWRAP_ARGS values};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Value string array generation
#define ADD_CONFIG_ENTRY(                                                              \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings) \
    const char* const spi_config_##name##_strings[] = {UNWRAP_ARGS strings};
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Value changed callback generation
#define ADD_CONFIG_ENTRY(                                                                 \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings)    \
    static void name##_changed(VariableItem* item) {                                      \
        FlipperSPITerminalApp* app = (variable_item_get_context)(item);                   \
        uint8_t index = (variable_item_get_current_value_index)(item);                    \
        (variable_item_set_current_value_text)(item, spi_config_##name##_strings[index]); \
        app->config.field = spi_config_##name##_values[index];                            \
    }
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY

// Add item generation
#define ADD_CONFIG_ENTRY(                                                              \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings) \
    item = variable_item_list_add(                                                     \
        app->config_screen.view,                                                       \
        label,                                                                         \
        COUNT_OF(spi_config_##name##_values),                                          \
        name##_changed,                                                                \
        app);                                                                          \
    value_index = (valueIndexFunc)(app->config.field,                                  \
                                   spi_config_##name##_values,                         \
                                   COUNT_OF(spi_config_##name##_values));              \
    variable_item_set_current_value_index(item, value_index);                          \
    name##_changed(item);

static void flipper_spi_terminal_scene_config_alloc_spi_config_items(FlipperSPITerminalApp* app) {
    VariableItem* item;
    size_t value_index;

    UNUSED(item);
    UNUSED(value_index);

#include "scene_config_values_config.h"
}
#undef ADD_CONFIG_ENTRY

#define SPI_TERM_HELP_TEXT_INDEX_OFFSET 1
// Help strings
#define ADD_CONFIG_ENTRY(                                                              \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings) \
    label "\n\n" helpText,
const char* const flipper_spi_terminal_scene_config_help_strings[] = {
#include "scene_config_values_config.h"
};
#undef ADD_CONFIG_ENTRY

void flipper_spi_terminal_scene_config_on_center_button(void* context, uint32_t index) {
    SPI_TERM_CONTEXT_TO_APP(context);

    if(index < SPI_TERM_HELP_TEXT_INDEX_OFFSET ||
       index >= SPI_TERM_HELP_TEXT_INDEX_OFFSET +
                    COUNT_OF(flipper_spi_terminal_scene_config_help_strings)) {
        return;
    }

    const char* text =
        flipper_spi_terminal_scene_config_help_strings[index - SPI_TERM_HELP_TEXT_INDEX_OFFSET];
    SPI_TERM_LOG_T("Setting help text to '%s'", text);
    app->config_screen.help_string = text;

    scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneConfigHelp);
}

void flipper_spi_terminal_scene_config_alloc(FlipperSPITerminalApp* app) {
    furi_check(app);

    app->config_screen.view = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneConfig,
        variable_item_list_get_view(app->config_screen.view));

    // Help
    variable_item_list_add(app->config_screen.view, "Press OK for help", 0, NULL, NULL);

    // MAKE SURE TO ADJUST THE SPI_TERM_HELP_TEXT_INDEX_OFFSET VALUE, IF YOU ADD A VALUE BEFORE THIS COMMENT!

    // Add all auto generated Config values
    flipper_spi_terminal_scene_config_alloc_spi_config_items(app);

    variable_item_list_set_enter_callback(
        app->config_screen.view, flipper_spi_terminal_scene_config_on_center_button, app);
}

void flipper_spi_terminal_scene_config_free(FlipperSPITerminalApp* app) {
    furi_check(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfig);
    variable_item_list_free(app->config_screen.view);
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

    size_t value_index;
    const char* str;

    UNUSED(value_index);
    UNUSED(str);

#define ADD_CONFIG_ENTRY(                                                              \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, values, strings) \
    value_index = (valueIndexFunc)(app->config.field,                                  \
                                   spi_config_##name##_values,                         \
                                   COUNT_OF(spi_config_##name##_values));              \
    str = spi_config_##name##_strings[value_index];                                    \
    SPI_TERM_LOG_D(label "(" #name "): %s", str);
#include "scene_config_values_config.h"
#undef ADD_CONFIG_ENTRY
}
