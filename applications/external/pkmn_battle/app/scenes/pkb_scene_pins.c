/*
 * Link cable pinout selection.
 * Adapted from Flipper-Zero-Game-Boy-Pokemon-Trading src/scenes/pokemon_pins.c
 * (MIT, Copyright (c) 2023 Esteban Fuentealba, Copyright (c) 2023-2024 KBEmbedded).
 */
#include "../pkb_app.h"
#include "pkb_scene.h"

#include <gblink.h>
#include <gblink/include/gblink_pinconf.h>

/* Must match gblink's gblink_pinouts order, plus "Custom" */
static const char* named_groups[] = {
    "Original",
    "MLVK2.5",
    "Custom",
    "",
};

static void pins_rebuild_list(PkbApp* app, int mode);

static void pins_default_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PkbApp* app = variable_item_get_context(item);

    variable_item_set_current_value_text(item, named_groups[index]);
    gblink_pin_set_default(app->gblink, index);
    pins_rebuild_list(app, index);
}

static void pins_pin_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    PkbApp* app = variable_item_get_context(item);
    uint8_t which = variable_item_list_get_selected_item_index(app->variable_item_list);
    gblink_bus_pins pin;

    switch(which) {
    case 1:
        pin = PIN_SERIN;
        break;
    case 2:
        pin = PIN_SEROUT;
        break;
    case 3:
        pin = PIN_CLK;
        break;
    default:
        furi_crash();
    }

    if(index > gblink_pin_get(app->gblink, pin))
        index = gblink_pin_get_next(index);
    else
        index = gblink_pin_get_prev(index);
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(item, gpio_pins[index].name);
    gblink_pin_set(app->gblink, pin, index);
}

static void pins_add_pin(PkbApp* app, const char* label, gblink_bus_pins pin, int mode) {
    int pinmax = gblink_pin_count_max() + 1;
    bool fixed = mode < PINOUT_COUNT;
    VariableItem* item = variable_item_list_add(
        app->variable_item_list, label, fixed ? 1 : pinmax, pins_pin_callback, app);
    int pinnum = gblink_pin_get(app->gblink, pin);
    variable_item_set_current_value_index(item, fixed ? 0 : pinnum);
    variable_item_set_current_value_text(item, gpio_pins[pinnum].name);
}

static void pins_rebuild_list(PkbApp* app, int mode) {
    variable_item_list_reset(app->variable_item_list);

    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Mode", PINOUT_COUNT + 1, pins_default_callback, app);
    variable_item_set_current_value_index(item, mode);
    variable_item_set_current_value_text(item, named_groups[mode]);

    pins_add_pin(app, "SI:", PIN_SERIN, mode);
    pins_add_pin(app, "SO:", PIN_SEROUT, mode);
    pins_add_pin(app, "CLK:", PIN_CLK, mode);
}

void pkb_scene_pins_on_enter(void* context) {
    PkbApp* app = context;
    int def_mode = gblink_pin_get_default(app->gblink);

    pins_rebuild_list(app, (def_mode < 0) ? PINOUT_COUNT : def_mode);

    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewVariableItemList);
}

bool pkb_scene_pins_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pkb_scene_pins_on_exit(void* context) {
    PkbApp* app = context;
    gblink_pinconf_save(app->gblink);
    variable_item_list_reset(app->variable_item_list);
}
