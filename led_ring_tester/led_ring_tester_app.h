#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <furi_hal_gpio.h>

#include "ws2812b.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct LedRingTesterApp LedRingTesterApp;

// View types
typedef enum {
    LedRingTesterViewSubmenu,
    LedRingTesterViewConfig,
    LedRingTesterViewIndividualTest,
    LedRingTesterViewColorSweep,
    LedRingTesterViewAllOnOff,
    LedRingTesterViewBrightness,
    LedRingTesterViewQuickTest,
} LedRingTesterView;

// Scene types
typedef enum {
    LedRingTesterSceneMenu,
    LedRingTesterSceneConfig,
    LedRingTesterSceneIndividualTest,
    LedRingTesterSceneColorSweep,
    LedRingTesterSceneAllOnOff,
    LedRingTesterSceneBrightness,
    LedRingTesterSceneQuickTest,
    LedRingTesterSceneNum,
} LedRingTesterScene;

// LED configuration
typedef struct {
    uint8_t led_count;
    const GpioPin* gpio_pin;
    uint8_t brightness;
} LedRingConfig;

// Main app structure
struct LedRingTesterApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    // Views
    Submenu* submenu;
    VariableItemList* config_list;
    View* individual_test_view;
    View* color_sweep_view;
    View* all_onoff_view;
    View* brightness_view;
    View* quick_test_view;

    // LED configuration
    LedRingConfig config;
    WS2812B* led_strip;

    // Safety timer (2 minutes)
    FuriTimer* safety_timer;
};

// Scene handlers
void led_ring_tester_scene_on_enter_menu(void* context);
bool led_ring_tester_scene_on_event_menu(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_menu(void* context);

void led_ring_tester_scene_on_enter_config(void* context);
bool led_ring_tester_scene_on_event_config(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_config(void* context);

void led_ring_tester_scene_on_enter_individual_test(void* context);
bool led_ring_tester_scene_on_event_individual_test(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_individual_test(void* context);

void led_ring_tester_scene_on_enter_color_sweep(void* context);
bool led_ring_tester_scene_on_event_color_sweep(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_color_sweep(void* context);

void led_ring_tester_scene_on_enter_all_onoff(void* context);
bool led_ring_tester_scene_on_event_all_onoff(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_all_onoff(void* context);

void led_ring_tester_scene_on_enter_brightness(void* context);
bool led_ring_tester_scene_on_event_brightness(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_brightness(void* context);

void led_ring_tester_scene_on_enter_quick_test(void* context);
bool led_ring_tester_scene_on_event_quick_test(void* context, SceneManagerEvent event);
void led_ring_tester_scene_on_exit_quick_test(void* context);

#ifdef __cplusplus
}
#endif
