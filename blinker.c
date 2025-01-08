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

typedef enum {
    Min,
    Max,
    Dur,
} Mode;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    DialogEx* dialog;
    Widget* widget;
    NumberInput* number_input;
    FuriTimer* timer;
    BlinkerView current_view;
    uint32_t start_time;
    uint32_t blink_interval;
    uint32_t duration;
    uint32_t min_interval;
    Mode mode;
} BlinkerApp;

static void update_main_view(BlinkerApp* app) {
    dialog_ex_set_header(app->dialog, "Blinker", 64, 20, AlignCenter, AlignCenter);
    char text[64];
    snprintf(text, sizeof(text), "Dur: %lds Min: %ld", app->duration, app->min_interval);
    dialog_ex_set_text(app->dialog, text, 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog, "Min");
    dialog_ex_set_center_button_text(app->dialog, "Flash");
    dialog_ex_set_right_button_text(app->dialog, "Dur");
}


static void number_input_callback(void* context, int32_t value) {
    BlinkerApp* app = context;
    
    if (app->mode == Min) {
        app->min_interval = value;
    } else if (app->mode == Dur) {
        app->duration = value;
    }
    
    app->current_view = BlinkerViewDialog;
    update_main_view(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
}

static void show_number_input(BlinkerApp* app, const char* header, uint32_t current, uint32_t min, uint32_t max) {
    app->current_view = BlinkerViewNumber;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(app->number_input, number_input_callback, app, current, min, max);
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewNumber);
}

static void timer_callback(void* context) {
    BlinkerApp* app = context;
    static bool led_state = false;
    uint32_t elapsed_time = (furi_get_tick() - app->start_time) / 1000;
    
    if(elapsed_time <= app->duration) {
        uint32_t range = 500 - app->min_interval;
        app->blink_interval = app->min_interval + (elapsed_time * range / app->duration);
        furi_timer_stop(app->timer);
        furi_timer_start(app->timer, app->blink_interval);
    }
    
    led_state = !led_state;
    furi_hal_light_set(LightRed, led_state ? 0xFF : 0x00);
}

static void dialog_callback(DialogExResult result, void* context) {
    BlinkerApp* app = context;
    
    switch(result) {
    case DialogExResultLeft:
    case DialogExPressLeft:
        app->mode = Min;
        show_number_input(app, "Min interval (ms)", app->min_interval, 50, 450);
        break;

    case DialogExResultRight:
    case DialogExPressRight:
        app->mode = Dur;
        show_number_input(app, "Duration (sec)", app->duration, 5, 120);
        break;

    case DialogExResultCenter:
    case DialogExPressCenter:
        widget_reset(app->widget);
        widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Blinking");
        app->current_view = BlinkerViewWidget;
        app->start_time = furi_get_tick();
        app->blink_interval = app->min_interval;
        furi_timer_start(app->timer, app->blink_interval);
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewWidget);
        break;

    case DialogExReleaseLeft:
    case DialogExReleaseRight:
    case DialogExReleaseCenter:
        // Handle button releases - no action needed
        break;
    }
}

static bool back_button(void* context) {
    BlinkerApp* app = context;
    
    if(app->current_view == BlinkerViewWidget) {
        furi_timer_stop(app->timer);
        furi_hal_light_set(LightRed, 0x00);
        app->current_view = BlinkerViewDialog;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
        return true;
    }
    
    if(app->current_view != BlinkerViewDialog) {
        app->current_view = BlinkerViewDialog;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewDialog);
        return true;
    }
    
    return false;
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));
    
    // Initialize state
    app->current_view = BlinkerViewDialog;  // Changed from Submenu
    app->start_time = 0; // Default value - not important
    app->blink_interval = 100; // Default value - not important
    app->duration = 20; // Default value
    app->min_interval = 100; // Default value

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize views
    app->dialog = dialog_ex_alloc();
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