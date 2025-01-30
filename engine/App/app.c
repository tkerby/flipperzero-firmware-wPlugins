#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>

#define TAG "VGMGameRemote"

// Define the submenu items for our [VGM] Game Remote application
typedef enum
{
    VGMGameRemoteSubmenuIndexRun, // Click to run the [VGM] Game Remote application
    VGMGameRemoteSubmenuIndexAbout,
} VGMGameRemoteSubmenuIndex;

// Define a single view for our [VGM] Game Remote application
typedef enum
{
    VGMGameRemoteViewMain,    // The main screen
    VGMGameRemoteViewSubmenu, // The submenu
    VGMGameRemoteViewAbout,   // The about screen
} VGMGameRemoteView;

// Each screen will have its own view
typedef struct
{
    ViewDispatcher *view_dispatcher; // Switches between our views
    View *view_main;                 // The main screen that displays "Hello, World!"
    Submenu *submenu;                // The submenu
    Widget *widget;                  // The widget
    FlipperHTTP *fhttp;              // The FlipperHTTP context
    char last_input[2];              // The last input
} VGMGameRemote;

// Callback for drawing the main screen
static void vgm_game_remote_view_draw_callback(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_draw_str(canvas, 0, 10, "Press any button");
}

static bool vgm_game_remote_view_input_callback(InputEvent *event, void *context)
{
    VGMGameRemote *app = (VGMGameRemote *)context;
    furi_check(app);
    if (event->key == InputKeyUp)
    {
        app->last_input[0] = '0';
        app->last_input[1] = '\0';
        flipper_http_send_data(app->fhttp, app->last_input);
        return true;
    }
    else if (event->key == InputKeyDown)
    {
        app->last_input[0] = '1';
        app->last_input[1] = '\0';
        flipper_http_send_data(app->fhttp, app->last_input);
        return true;
    }
    else if (event->key == InputKeyLeft)
    {
        app->last_input[0] = '2';
        app->last_input[1] = '\0';
        flipper_http_send_data(app->fhttp, app->last_input);
        return true;
    }
    else if (event->key == InputKeyRight)
    {
        app->last_input[0] = '3';
        app->last_input[1] = '\0';
        flipper_http_send_data(app->fhttp, app->last_input);
        return true;
    }
    else if (event->key == InputKeyOk)
    {
        app->last_input[0] = '4';
        app->last_input[1] = '\0';
        flipper_http_send_data(app->fhttp, app->last_input);
        return true;
    }
    // else if (event->key == InputKeyBack)
    // {
    //     app->last_input[0] = '5';
    //     app->last_input[1] = '\0';
    //     flipper_http_send_data(app->fhttp, app->last_input);
    //     return true;
    // }
    return false;
}

static void free_fhttp(VGMGameRemote *app)
{
    furi_check(app);
    if (app->fhttp)
    {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }
}
static void callback_submenu_choices(void *context, uint32_t index)
{
    VGMGameRemote *app = (VGMGameRemote *)context;
    furi_check(app);
    switch (index)
    {
    case VGMGameRemoteSubmenuIndexRun:
        free_fhttp(app);
        app->fhttp = flipper_http_alloc();
        if (!app->fhttp)
        {
            easy_flipper_dialog("Error", "Failed to allocate FlipperHTTP\nUART likely busy.Restart\nyour Flipper Zero.");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewMain);
        break;
    case VGMGameRemoteSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewAbout);
        break;
    default:
        break;
    }
}

static uint32_t callback_to_submenu(void *context)
{
    UNUSED(context);
    return VGMGameRemoteViewSubmenu;
}

static uint32_t callback_exit_app(void *context)
{
    // Exit the application
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

static VGMGameRemote *vgm_game_remote_app_alloc()
{
    VGMGameRemote *app = (VGMGameRemote *)malloc(sizeof(VGMGameRemote));

    Gui *gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }

    // Main view
    if (!easy_flipper_set_view(&app->view_main, VGMGameRemoteViewMain, vgm_game_remote_view_draw_callback, vgm_game_remote_view_input_callback, callback_to_submenu, &app->view_dispatcher, app))
    {
        return NULL;
    }

    // Widget
    if (!easy_flipper_set_widget(&app->widget, VGMGameRemoteViewAbout, "VGM Game Engine Remote\n-----\nCompanion app for the VGM\nGame Engine.\n-----\nwww.github.com/jblanked", callback_to_submenu, &app->view_dispatcher))
    {
        return NULL;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu, VGMGameRemoteViewSubmenu, "VGM Game Remote", callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }
    submenu_add_item(app->submenu, "Run", VGMGameRemoteSubmenuIndexRun, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "About", VGMGameRemoteSubmenuIndexAbout, callback_submenu_choices, app);

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, VGMGameRemoteViewSubmenu);

    return app;
}

// Function to free the resources used by VGMGameRemote
static void vgm_game_remote_app_free(VGMGameRemote *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "VGMGameRemote is NULL");
        return;
    }
    // Free View(s)
    if (app->view_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewMain);
        view_free(app->view_main);
    }

    // Free Submenu(s)
    if (app->submenu)
    {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewSubmenu);
        submenu_free(app->submenu);
    }

    // Free Widget(s)
    if (app->widget)
    {
        view_dispatcher_remove_view(app->view_dispatcher, VGMGameRemoteViewAbout);
        widget_free(app->widget);
    }

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    free_fhttp(app);

    // free the app
    free(app);
    app = NULL;
}

// Entry point for the [VGM] Game Remote application
int32_t vgm_game_remote_main(void *p)
{
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the [VGM] Game Remote application
    VGMGameRemote *app = vgm_game_remote_app_alloc();
    if (!app)
    {
        FURI_LOG_E(TAG, "Failed to allocate VGMGameRemote");
        return -1;
    }

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the [VGM] Game Remote application
    vgm_game_remote_app_free(app);

    // Return 0 to indicate success
    return 0;
}
