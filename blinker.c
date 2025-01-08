#include <furi.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

typedef enum {
    BlinkerViewDialog,
    BlinkerViewWidget,
    BlinkerViewNumber,
} BlinkerView;

typedef struct {
    Gui* gui;
    BlinkerView current_view;
    ViewDispatcher* view_dispatcher;
    DialogEx* dialog;
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


static void update_main_view(BlinkerApp* app) {
    dialog_ex_set_header(app->dialog, "Blinker", 64, 20, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(app->dialog, "Start");
    dialog_ex_set_right_button_text(app->dialog, "Dur.");
    char duration_text[32];
    snprintf(duration_text, sizeof(duration_text), "Duration: %lds", app->duration);
    dialog_ex_set_text(app->dialog, duration_text, 64, 32, AlignCenter, AlignCenter);
}


static void duration_callback(void* context, int32_t value) {
    BlinkerApp* app = context;

    app->duration = value;
    app->current_view = BlinkerViewDialog;
    
    // Update the main view to show new duration
    update_main_view(app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
}

static bool back_button(void* context) {
    BlinkerApp* app = context;
    
    if(app->current_view == BlinkerViewWidget) {
        // If in widget view, stop timer and LED, return to menu
        furi_timer_stop(app->timer);
        furi_hal_light_set(LightRed, 0x00);
        app->current_view = BlinkerViewDialog;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
        return true;
    } else if(app->current_view == BlinkerViewNumber) {
        app->current_view = BlinkerViewDialog;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
        return true;
    } else if(app->current_view == BlinkerViewDialog) {
        // We're in the main dialog view, allow exit
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    
    return false;
}

static void dialog_callback(DialogExResult result, void* context) {
    BlinkerApp* app = context;
    
    switch(result) {
    case DialogExResultCenter:
        // Start blinking (middle button)
        widget_reset(app->widget);
        widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Blinking");
        app->current_view = BlinkerViewWidget;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewWidget);
        app->start_timestamp = furi_get_tick();
        app->current_interval = 50;
        furi_timer_start(app->timer, app->current_interval);
        break;
    case DialogExResultRight:
        // Set duration (right button)
        app->current_view = BlinkerViewNumber;

        NumberInput* number_input = app->number_input;

        number_input_set_header_text(number_input, "Blink duration (sec)");
        number_input_set_result_callback(
            number_input,
            duration_callback,
            app,           // Context
            app->duration, // Current value
            5,            // Min value
            120);         // Max value
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewNumber);
        break;
    default:
        break;
    }
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));
    
    // Initialize state
    app->current_view = BlinkerViewDialog;  // Changed from Submenu
    app->start_timestamp = 0; // Default value - not important
    app->current_interval = 100; // Default value - not important
    app->duration = 20; // Default value

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize views
    app->dialog = dialog_ex_alloc();  // Changed from submenu
    app->widget = widget_alloc();
    app->number_input = number_input_alloc();

    // Configure dialog
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, dialog_callback);
    // Ensure dialog has a reference to app context for back button
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_button);
    update_main_view(app);

    // Add views
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewDialog, dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewNumber, number_input_get_view(app->number_input));

    // On back button press
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_button);

    // Create and configure timer
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);

    // Run application
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewNumber);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    number_input_free(app->number_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}