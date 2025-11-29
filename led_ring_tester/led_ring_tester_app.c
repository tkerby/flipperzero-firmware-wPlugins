#include "led_ring_tester_app.h"

// Scene handlers array
void (*const led_ring_tester_scene_on_enter_handlers[])(void*) = {
    led_ring_tester_scene_on_enter_menu,
    led_ring_tester_scene_on_enter_config,
    led_ring_tester_scene_on_enter_individual_test,
    led_ring_tester_scene_on_enter_color_sweep,
    led_ring_tester_scene_on_enter_all_onoff,
    led_ring_tester_scene_on_enter_brightness,
    led_ring_tester_scene_on_enter_quick_test,
};

bool (*const led_ring_tester_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    led_ring_tester_scene_on_event_menu,
    led_ring_tester_scene_on_event_config,
    led_ring_tester_scene_on_event_individual_test,
    led_ring_tester_scene_on_event_color_sweep,
    led_ring_tester_scene_on_event_all_onoff,
    led_ring_tester_scene_on_event_brightness,
    led_ring_tester_scene_on_event_quick_test,
};

void (*const led_ring_tester_scene_on_exit_handlers[])(void*) = {
    led_ring_tester_scene_on_exit_menu,
    led_ring_tester_scene_on_exit_config,
    led_ring_tester_scene_on_exit_individual_test,
    led_ring_tester_scene_on_exit_color_sweep,
    led_ring_tester_scene_on_exit_all_onoff,
    led_ring_tester_scene_on_exit_brightness,
    led_ring_tester_scene_on_exit_quick_test,
};

// Scene manager handlers
static const SceneManagerHandlers led_ring_tester_scene_manager_handlers = {
    .on_enter_handlers = led_ring_tester_scene_on_enter_handlers,
    .on_event_handlers = led_ring_tester_scene_on_event_handlers,
    .on_exit_handlers = led_ring_tester_scene_on_exit_handlers,
    .scene_num = LedRingTesterSceneNum,
};

// View dispatcher callbacks
static bool led_ring_tester_view_dispatcher_navigation_event_callback(void* context) {
    furi_assert(context);
    LedRingTesterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool led_ring_tester_view_dispatcher_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    LedRingTesterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// Safety timer callback - stop all LED activity after 2 minutes
static void led_ring_tester_safety_timer_callback(void* context) {
    furi_assert(context);
    LedRingTesterApp* app = context;

    // Turn off all LEDs
    if(app->led_strip) {
        ws2812b_clear(app->led_strip);
        ws2812b_update(app->led_strip);
    }

    // Return to menu
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, LedRingTesterSceneMenu);
}

// Allocate app
static LedRingTesterApp* led_ring_tester_app_alloc() {
    LedRingTesterApp* app = malloc(sizeof(LedRingTesterApp));

    // Records
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, led_ring_tester_view_dispatcher_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, led_ring_tester_view_dispatcher_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene manager
    app->scene_manager = scene_manager_alloc(&led_ring_tester_scene_manager_handlers, app);

    // Views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LedRingTesterViewSubmenu, submenu_get_view(app->submenu));

    app->config_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LedRingTesterViewConfig,
        variable_item_list_get_view(app->config_list));

    // Custom views will be created in their respective scene files
    app->individual_test_view = NULL;
    app->color_sweep_view = NULL;
    app->all_onoff_view = NULL;
    app->brightness_view = NULL;
    app->quick_test_view = NULL;

    // Initialize LED configuration - default GPIO C3 (pin 7), 12 LEDs
    app->config.gpio_pin = &gpio_ext_pc3;
    app->config.led_count = 12;
    app->config.brightness = 128; // 50% brightness

    // Initialize LED strip with default config
    app->led_strip = ws2812b_alloc(app->config.gpio_pin, app->config.led_count);

    // Create safety timer (2 minutes = 120000 ms)
    app->safety_timer = furi_timer_alloc(led_ring_tester_safety_timer_callback, FuriTimerTypeOnce, app);

    return app;
}

// Free app
static void led_ring_tester_app_free(LedRingTesterApp* app) {
    furi_assert(app);

    // Free safety timer
    if(app->safety_timer) {
        furi_timer_stop(app->safety_timer);
        furi_timer_free(app->safety_timer);
    }

    // Free LED strip
    if(app->led_strip) {
        ws2812b_free(app->led_strip);
    }

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewConfig);
    variable_item_list_free(app->config_list);

    if(app->individual_test_view) {
        view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewIndividualTest);
        view_free(app->individual_test_view);
    }

    if(app->color_sweep_view) {
        view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewColorSweep);
        view_free(app->color_sweep_view);
    }

    if(app->all_onoff_view) {
        view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewAllOnOff);
        view_free(app->all_onoff_view);
    }

    if(app->brightness_view) {
        view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewBrightness);
        view_free(app->brightness_view);
    }

    if(app->quick_test_view) {
        view_dispatcher_remove_view(app->view_dispatcher, LedRingTesterViewQuickTest);
        view_free(app->quick_test_view);
    }

    // Free scene manager and view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

// Entry point
int32_t led_ring_tester_app(void* p) {
    UNUSED(p);

    LedRingTesterApp* app = led_ring_tester_app_alloc();

    // Start with menu scene
    scene_manager_next_scene(app->scene_manager, LedRingTesterSceneMenu);

    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    led_ring_tester_app_free(app);

    return 0;
}
