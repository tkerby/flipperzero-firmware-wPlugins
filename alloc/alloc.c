#include <alloc/alloc.h>

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
static uint32_t callback_exit_app(void *context)
{
    // Exit the application
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

// Function to allocate resources for the FlipWorldApp
FlipWorldApp *flip_world_app_alloc()
{
    FlipWorldApp *app = (FlipWorldApp *)malloc(sizeof(FlipWorldApp));

    Gui *gui = furi_record_open(RECORD_GUI);

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu, FlipWorldViewSubmenu, VERSION_TAG, callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }
    submenu_add_item(app->submenu, "Play", FlipWorldSubmenuIndexRun, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "About", FlipWorldSubmenuIndexAbout, callback_submenu_choices, app);
    submenu_add_item(app->submenu, "Settings", FlipWorldSubmenuIndexSettings, callback_submenu_choices, app);
    //

    // Switch to the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWorldViewSubmenu);

    return app;
}

// Function to free the resources used by FlipWorldApp
void flip_world_app_free(FlipWorldApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWorldApp is NULL");
        return;
    }

    // Free Submenu(s)
    if (app->submenu)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWorldViewSubmenu);
        submenu_free(app->submenu);
    }

    free_all_views(app, true, true);

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    free(app);
}
