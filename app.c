#include <alloc/alloc.h>
#include <flip_storage/storage.h>

// Entry point for the FlipWorld application
int32_t flip_world_main(void *p)
{
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipWorld application
    FlipWorldApp *app = flip_world_app_alloc();
    if (!app)
        return -1;

    // initialize the VGM
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc1, false); // pull pin 15 low

    // check if board is connected (Derek Jamison)
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        easy_flipper_dialog("FlipperHTTP Error", "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
        return -1;
    }

    if (!flipper_http_ping(fhttp))
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
        furi_delay_ms(100); // this causes a BusFault
    }
    flipper_http_free(fhttp);

    if (counter == 0)
        easy_flipper_dialog("FlipperHTTP Error", "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");

    // save app version
    char app_version[16];
    snprintf(app_version, sizeof(app_version), "%f", (double)VERSION);
    save_char("app_version", app_version);

    // Run the view dispatcher
    view_dispatcher_run(app->view_dispatcher);

    // Free the resources used by the FlipWorld application
    flip_world_app_free(app);

    // Return 0 to indicate success
    return 0;
}
