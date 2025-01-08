#include <furi.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

typedef enum {
    BlinkerViewSubmenu,
    BlinkerViewWidget,
    BlinkerViewNumber,
} BlinkerView;

typedef struct {
    Gui* gui;
    BlinkerView current_view;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    NumberInput* number_input;
    FuriTimer* timer;
    uint32_t start_timestamp;
    uint32_t current_interval;
    uint32_t duration;
} BlinkerApp;

static void timer_callback(void* context) {
    BlinkerApp* app = context;

    static bool led_state = false;

    uint32_t current_time = furi_get_tick();
    uint32_t elapsed_time = (current_time - app->start_timestamp) / 1000; // Convert to seconds

    uint32_t min_sleep = 50; // Minimum sleep time in ms
    uint32_t max_sleep = 500; // Maximum sleep time in ms

    // Calculate new interval based on elapsed time
    if(elapsed_time < app->duration) {
        // First phase; gradually increase interval from `min_sleep` to `max_sleep`
        app->current_interval = min_sleep + (elapsed_time * ((max_sleep - min_sleep) / app->duration)); // Linear increase
        furi_timer_stop(app->timer);
        furi_timer_start(app->timer, app->current_interval);
    } else if(elapsed_time == app->duration) {
        // Second phase; set final interval to `max_sleep` and keep it constant
        app->current_interval = max_sleep;
        furi_timer_stop(app->timer);
        furi_timer_start(app->timer, app->current_interval);
    }
    
    led_state = !led_state;
    if(led_state) {
        furi_hal_light_set(LightRed, 0xFF);
    } else {
        furi_hal_light_set(LightRed, 0x00);
    }
}

static void start_blink_callback(void* context, uint32_t index) {
    BlinkerApp* app = context;
    UNUSED(index);
    
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Blinking");
    
    app->current_view = BlinkerViewWidget;
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewWidget);
    
    // Initialize blinking parameters
    app->start_timestamp = furi_get_tick();
    app->current_interval = 50; // NOTE: This value replicates min_sleep from timer_callback
    furi_timer_start(app->timer, app->current_interval);
}

static void duration_callback(void* context, int32_t value) {
    BlinkerApp* app = context;

    app->duration = value;
    app->current_view = BlinkerViewSubmenu;

    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
}

static void set_duration_callback(void* context, uint32_t index) {
    BlinkerApp* app = context;
    UNUSED(index);
    
    app->current_view = BlinkerViewNumber;

    NumberInput* number_input = app->number_input;

    number_input_set_header_text(number_input, "Blink duration (sec)");
    number_input_set_result_callback(
        number_input,      // Number input module
        duration_callback, // Function to call when value is set
        context,
        app->duration,     // Value which is being changed
        5,                 // Min value
        120                // Max value
    );
    
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewNumber);
}

static bool back_button(void* context) {
    BlinkerApp* app = context;
    
    if(app->current_view == BlinkerViewWidget) {
        // If in widget view, stop timer and LED, return to menu
        furi_timer_stop(app->timer);
        // TODO: look at what values do i want the LED to be configured to
        furi_hal_light_set(LightRed, 0x00);
        
        app->current_view = BlinkerViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
        
        return true;  // Exit to main menu
    } else if(app->current_view == BlinkerViewNumber) {
        app->current_view = BlinkerViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
        return true;
    }
    
    return false;  // Exit the app
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));
    
    // Initialize state
    app->current_view = BlinkerViewSubmenu;
    app->start_timestamp = 0; // Default value - not important
    app->current_interval = 100; // Default value - not important
    app->duration = 20; // Default value

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize views
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();

    // Initialize number input
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BlinkerViewNumber,
        number_input_get_view(app->number_input));
    
    

    // Add menu items
    submenu_add_item(app->submenu, "Start", 1, start_blink_callback, app);
    submenu_add_item(app->submenu, "Set Duration", 0, set_duration_callback, app);
    
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewWidget, widget_get_view(app->widget));

    // On back button press
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_button);

    // Create and configure timer
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);

    // Run application
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewNumber);
    widget_free(app->widget);
    submenu_free(app->submenu);
    number_input_free(app->number_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}