#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
} BlinkerApp;

typedef enum {
    BlinkerViewSubmenu,    // Changed from ViewSubmenu
    BlinkerViewWidget,     // Changed from ViewWidget
} BlinkerView;            // Changed from View

static void submenu_callback(void* context, uint32_t index) {
    BlinkerApp* app = context;
    UNUSED(index);
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Blinking");
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewWidget);  // Updated enum value
}

static bool navigation_callback(void* context) {
    BlinkerApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);  // Updated enum value
    return true;
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Initialize views
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();

    // Add menu item and views
    submenu_add_item(app->submenu, "Start", 0, submenu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewSubmenu, submenu_get_view(app->submenu));  // Updated enum value
    view_dispatcher_add_view(app->view_dispatcher, BlinkerViewWidget, widget_get_view(app->widget));     // Updated enum value

    // Configure navigation
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    // Run application
    view_dispatcher_switch_to_view(app->view_dispatcher, BlinkerViewSubmenu);  // Updated enum value
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewWidget);    // Updated enum value
    view_dispatcher_remove_view(app->view_dispatcher, BlinkerViewSubmenu);   // Updated enum value
    widget_free(app->widget);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}