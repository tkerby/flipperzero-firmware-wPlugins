#include <flip_library.h>
#include <alloc/flip_library_alloc.h>

// Entry point for the FlipLibrary application
int32_t flip_library_app(void* p) {
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipLibrary application
    app_instance = flip_library_app_alloc();
    if(!app_instance) {
        FURI_LOG_E(TAG, "Failed to allocate FlipLibraryApp");
        return -1;
    }

    if(!flipper_http_ping()) {
        FURI_LOG_E(TAG, "Failed to ping the device");
        return -1;
    }

    // Run the view dispatcher
    view_dispatcher_run(app_instance->view_dispatcher);

    // Free the resources used by the FlipLibrary application
    flip_library_app_free(app_instance);

    // Return 0 to indicate success
    return 0;
}
