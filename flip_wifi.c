#include "flip_wifi.h"
#include <callback/flip_wifi_callback.h>
WiFiPlaylist *wifi_playlist = NULL;
// Function to free the resources used by FlipWiFiApp
void flip_wifi_app_free(FlipWiFiApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // Free Submenu(s)
    if (app->submenu_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        submenu_free(app->submenu_main);
    }

    flip_wifi_free_all(app);

    // free the view dispatcher
    if (app->view_dispatcher)
        view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    if (app)
        free(app);
}
