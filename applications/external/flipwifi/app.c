#include <flip_wifi.h>
#include <alloc/flip_wifi_alloc.h>

// Entry point for the FlipWiFi application
int32_t flip_wifi_main(void* p) {
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipWiFi application
    app_instance = flip_wifi_app_alloc();
    if(!app_instance) {
        FURI_LOG_E(TAG, "Failed to allocate FlipWiFiApp");
        return -1;
    }

    if(!flipper_http_ping()) {
        FURI_LOG_E(TAG, "Failed to ping the device");
        return -1;
    }

    // Run the view dispatcher
    view_dispatcher_run(app_instance->view_dispatcher);

    // Free the resources used by the FlipWiFi application
    flip_wifi_app_free(app_instance);

    // Return 0 to indicate success
    return 0;
}
