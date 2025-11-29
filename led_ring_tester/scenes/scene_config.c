#include "../led_ring_tester_app.h"

#define LED_COUNT_OPTIONS 5
static const uint8_t led_count_values[LED_COUNT_OPTIONS] = {8, 12, 16, 24, 32};
static const char* led_count_names[LED_COUNT_OPTIONS] = {"8 LEDs", "12 LEDs", "16 LEDs", "24 LEDs", "32 LEDs"};

#define GPIO_PIN_OPTIONS 7
static const GpioPin* gpio_pin_values[GPIO_PIN_OPTIONS] = {
    &gpio_ext_pc0, // Pin 2
    &gpio_ext_pc1, // Pin 3
    &gpio_ext_pc3, // Pin 7
    &gpio_ext_pb2, // Pin 6
    &gpio_ext_pb3, // Pin 4
    &gpio_ext_pa4, // Pin 5
    &gpio_ext_pa6, // Pin 9
};
static const char* gpio_pin_names[GPIO_PIN_OPTIONS] = {
    "PC0 (Pin 2)",
    "PC1 (Pin 3)",
    "PC3 (Pin 7)",
    "PB2 (Pin 6)",
    "PB3 (Pin 4)",
    "PA4 (Pin 5)",
    "PA6 (Pin 9)",
};

typedef enum {
    ConfigIndexGpioPin,
    ConfigIndexLedCount,
    ConfigIndexBrightness,
} ConfigIndex;

static uint8_t get_gpio_pin_index(const GpioPin* pin) {
    for(uint8_t i = 0; i < GPIO_PIN_OPTIONS; i++) {
        if(gpio_pin_values[i] == pin) {
            return i;
        }
    }
    return 2; // Default to PC3
}

static uint8_t get_led_count_index(uint8_t count) {
    for(uint8_t i = 0; i < LED_COUNT_OPTIONS; i++) {
        if(led_count_values[i] == count) {
            return i;
        }
    }
    return 1; // Default to 12 LEDs
}

static void config_gpio_pin_change(VariableItem* item) {
    LedRingTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, gpio_pin_names[index]);

    app->config.gpio_pin = gpio_pin_values[index];

    // Reinitialize LED strip with new pin
    if(app->led_strip) {
        ws2812b_free(app->led_strip);
    }
    app->led_strip = ws2812b_alloc(app->config.gpio_pin, app->config.led_count);
}

static void config_led_count_change(VariableItem* item) {
    LedRingTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, led_count_names[index]);

    app->config.led_count = led_count_values[index];

    // Update LED strip count
    if(app->led_strip) {
        ws2812b_set_led_count(app->led_strip, app->config.led_count);
    }
}

static void config_brightness_change(VariableItem* item) {
    LedRingTesterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    // Brightness from 10% to 100% in 10% steps
    app->config.brightness = (index + 1) * 25; // 25, 50, 75, 100, 125, 150, 175, 200, 225, 255

    char brightness_text[8];
    snprintf(brightness_text, sizeof(brightness_text), "%d%%", (app->config.brightness * 100) / 255);
    variable_item_set_current_value_text(item, brightness_text);
}

void led_ring_tester_scene_on_enter_config(void* context) {
    LedRingTesterApp* app = context;
    VariableItemList* config_list = app->config_list;
    VariableItem* item;

    variable_item_list_reset(config_list);
    variable_item_list_set_header(config_list, "Configuration");

    // GPIO Pin selection
    item = variable_item_list_add(config_list, "GPIO Pin", GPIO_PIN_OPTIONS, config_gpio_pin_change, app);
    uint8_t gpio_index = get_gpio_pin_index(app->config.gpio_pin);
    variable_item_set_current_value_index(item, gpio_index);
    variable_item_set_current_value_text(item, gpio_pin_names[gpio_index]);

    // LED count selection
    item = variable_item_list_add(config_list, "LED Count", LED_COUNT_OPTIONS, config_led_count_change, app);
    uint8_t led_count_index = get_led_count_index(app->config.led_count);
    variable_item_set_current_value_index(item, led_count_index);
    variable_item_set_current_value_text(item, led_count_names[led_count_index]);

    // Brightness selection (10 levels)
    item = variable_item_list_add(config_list, "Brightness", 10, config_brightness_change, app);
    uint8_t brightness_index = (app->config.brightness / 25) - 1;
    if(brightness_index > 9) brightness_index = 4; // Default to 50%
    variable_item_set_current_value_index(item, brightness_index);
    char brightness_text[8];
    snprintf(brightness_text, sizeof(brightness_text), "%d%%", (app->config.brightness * 100) / 255);
    variable_item_set_current_value_text(item, brightness_text);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewConfig);
}

bool led_ring_tester_scene_on_event_config(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_config(void* context) {
    LedRingTesterApp* app = context;
    variable_item_list_reset(app->config_list);
}
