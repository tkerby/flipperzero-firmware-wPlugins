#include <flip_downloader.h>
#include <alloc/flip_store_alloc.h>
#include <update/update.h>
#include <flip_storage/flip_store_storage.h>

// Entry point for the Hello World application
int32_t main_flip_downloader(void *p)
{
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the Hello World application
    FlipStoreApp *app = flip_store_app_alloc();
    if (!app)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipStoreApp");
        return -1;
    }

    // check if board is connected (Derek Jamison)
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
        return -1;
    }

    if (!flipper_http_send_command(fhttp, HTTP_CMD_PING))
    {
        FURI_LOG_E(TAG, "Failed to ping the device");
        flipper_http_free(fhttp);
        return -1;
    }

    // Try to wait for pong response.
    uint32_t counter = 10;
    while (fhttp->state == INACTIVE && --counter > 0)
    {
        FURI_LOG_D(TAG, "Waiting for PONG");
        furi_delay_ms(100);
    }

    if (counter == 0)
    {
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
    }

    save_char("app_version", VERSION);

    // for now use the catalog API until I implement caching on the server
    if (update_is_ready(fhttp, true))
    {
        easy_flipper_dialog("Update Status", "Complete.\nRestart your Flipper Zero.");
    }

    flipper_http_free(fhttp);

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the Hello World application
    flip_store_app_free(app);

    // Return 0 to indicate success
    return 0;
}
