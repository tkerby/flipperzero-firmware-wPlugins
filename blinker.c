#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <furi_hal_light.h>

typedef enum {
    BlinkerViewSubmenu,
    BlinkerViewWidget,
} BlinkerView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    FuriTimer* timer;
    BlinkerView current_view;
} BlinkerApp;

static void blink_timer_callback(void* context) {
    UNUSED(context);

    static bool led_state = false;
    led_state = !led_state;
    
    // TODO: change color to red and off
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
    
    furi_timer_start(app->timer, 500); // 500ms = 2 times per second
}

static bool handle_back_event(void* context) {
    BlinkerApp* app = context;
    
    if(app->current_view == BlinkerViewWidget) {
        // If in widget view, stop timer and LED, return to menu
        furi_timer_stop(app->timer);
        // TODO: look at what values do i want the LED to be configured to
        furi_hal_light_set(LightRed, 0x00);
        
        app->current_view = BlinkerViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
        
        return true;  // Exit to main menu
    }
    
    return false;  // Exit the app
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));
    
    // Initialize state
    app->current_view = BlinkerViewSubmenu;

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize views
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();

    submenu_add_item(app->submenu, "Start", 0, start_blink_callback, app);
    
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewWidget, widget_get_view(app->widget));

    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, handle_back_event);

    // Create and configure timer
    app->timer = furi_timer_alloc(blink_timer_callback, FuriTimerTypePeriodic, app);

    // Run application
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewSubmenu);
    widget_free(app->widget);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}