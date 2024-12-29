#include <flip_wifi.h>
#include <alloc/flip_wifi_alloc.h>

// Entry point for the FlipWiFi application
int32_t flip_wifi_main(void *p)
{
    UNUSED(p);

    // Initialize the FlipWiFi application
    FlipWiFiApp *app = flip_wifi_app_alloc();
    if (!app)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipWiFiApp");
        return -1;
    }

    // check if board is connected (Derek Jamison)
    // initialize the http
    if (flipper_http_init(flipper_http_rx_callback, app))
    {
        if (!flipper_http_ping())
        {
            FURI_LOG_E(TAG, "Failed to ping the device");
            return -1;
        }

        // Try to wait for pong response.
        uint8_t counter = 10;
        while (fhttp.state == INACTIVE && --counter > 0)
        {
            FURI_LOG_D(TAG, "Waiting for PONG");
            furi_delay_ms(100);
        }

        if (counter == 0)
            easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");

        flipper_http_deinit();
    }
    else
    {
        easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
    }

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the FlipWiFi application
    flip_wifi_app_free(app);

    // Return 0 to indicate success
    return 0;
}
