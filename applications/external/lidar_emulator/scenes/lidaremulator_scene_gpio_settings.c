#include "../lidaremulator_app_i.h"

static const char* gpio_settings_pin_text[] = {
    "Flipper",
    "2 (A7)",
    "Detect",
};

static const char* gpio_settings_otg_text[] = {
    "OFF",
    "ON",
};

enum {
    GpioEventTypeTxPinChanged = 1,
    GpioEventTypeOtgChanged = 2,
};

static inline uint32_t gpio_custom_event_pack(uint16_t type, uint16_t value) {
    return (type << 16) | value;
}

static inline uint16_t gpio_custom_event_get_type(uint32_t event) {
    return event >> 16;
}

static inline uint16_t gpio_custom_event_get_value(uint32_t event) {
    return event & 0xFFFF;
}

static void gpio_settings_pin_change_callback(VariableItem* item) {
    LidarEmulatorApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gpio_settings_pin_text[index]);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, gpio_custom_event_pack(GpioEventTypeTxPinChanged, index));
}

static void gpio_settings_otg_change_callback(VariableItem* item) {
    LidarEmulatorApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, gpio_settings_otg_text[index]);
    view_dispatcher_send_custom_event(
        app->view_dispatcher, gpio_custom_event_pack(GpioEventTypeOtgChanged, index));
}

static void gpio_settings_init(LidarEmulatorApp* app) {
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        var_item_list,
        "Signal Output",
        COUNT_OF(gpio_settings_pin_text),
        gpio_settings_pin_change_callback,
        app);

    value_index = app->app_state.tx_pin;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, gpio_settings_pin_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "5V on GPIO",
        COUNT_OF(gpio_settings_otg_text),
        gpio_settings_otg_change_callback,
        app);

    if(app->app_state.tx_pin < FuriHalInfraredTxPinMax) {
        value_index = app->app_state.is_otg_enabled;
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, gpio_settings_otg_text[value_index]);
    } else {
        variable_item_set_values_count(item, 1);
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "Auto");
    }
}

void lidaremulator_scene_gpio_settings_on_enter(void* context) {
    LidarEmulatorApp* app = context;
    gpio_settings_init(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LidarEmulatorViewVariableList);
}

bool lidaremulator_scene_gpio_settings_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    LidarEmulatorApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint16_t custom_event_type = gpio_custom_event_get_type(event.event);
        const uint16_t custom_event_value = gpio_custom_event_get_value(event.event);

        if(custom_event_type == GpioEventTypeTxPinChanged) {
            lidaremulator_set_tx_pin(app, custom_event_value);
            variable_item_list_reset(app->var_item_list);
            gpio_settings_init(app);
        } else if(custom_event_type == GpioEventTypeOtgChanged) {
            lidaremulator_enable_otg(app, custom_event_value);
        }

        consumed = true;
    }

    return consumed;
}

void lidaremulator_scene_gpio_settings_on_exit(void* context) {
    LidarEmulatorApp* app = context;
    variable_item_list_reset(app->var_item_list);
    lidaremulator_save_settings(app);
}
