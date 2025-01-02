#include <alloc/alloc.h>
#include <flip_storage/storage.h>

// Entry point for the FlipWorld application
int32_t flip_world_main(void* p) {
    // Suppress unused parameter warning
    UNUSED(p);

    // Initialize the FlipWorld application
    FlipWorldApp* app = flip_world_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate FlipWorldApp");
        return -1;
    }

    // initialize the VGM
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc1, false); // pull pin 15 low

    // check if board is connected (Derek Jamison)
    FlipperHTTP* fhttp = flipper_http_alloc();
    if(!fhttp) {
        easy_flipper_dialog(
            "FlipperHTTP Error",
            "The UART is likely busy.\nEnsure you have the correct\nflash for your board then\nrestart your Flipper Zero.");
        return -1;
    }

    if(!flipper_http_ping(fhttp)) {
        FURI_LOG_E(TAG, "Failed to ping the device");
        flipper_http_free(fhttp);
        return -1;
    }

    // Try to wait for pong response.
    uint32_t counter = 10;
    while(fhttp->state == INACTIVE && --counter > 0) {
        FURI_LOG_D(TAG, "Waiting for PONG");
        furi_delay_ms(100); // this causes a BusFault
    }

    flipper_http_free(fhttp);
    if(counter == 0) {
        easy_flipper_dialog(
            "FlipperHTTP Error",
            "Ensure your WiFi Developer\nBoard or Pico W is connected\nand the latest FlipperHTTP\nfirmware is installed.");
    }

    // this will be removed in version 0.3. we'll keep all our data in the data folder from now on
    // load app version
    char saved_app_version[16];
    if(load_char("app_version", saved_app_version, sizeof(saved_app_version))) {
        float saved_version = strtod(saved_app_version, NULL);
        if(saved_version == 0.1) {
            // transfer files over into the data folder (to bs used to load the player context)
            char directory_path[256];
            snprintf(
                directory_path,
                sizeof(directory_path),
                STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");

            // Create the directory
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_common_mkdir(storage, directory_path);

            // copy the whole folder
            char source_path[128];
            snprintf(
                source_path, sizeof(source_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");

            if(storage_common_migrate(storage, source_path, directory_path) != FSE_OK) {
                FURI_LOG_E(TAG, "Failed to migrate files");
            } else {
                void clean_up(char* file_path) {
                    char updated_file_path[128];
                    snprintf(
                        updated_file_path,
                        sizeof(updated_file_path),
                        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/%s",
                        file_path);

                    // check if the file exists
                    if(storage_file_exists(storage, updated_file_path) &&
                       storage_common_remove(storage, updated_file_path) != FSE_OK) {
                        FURI_LOG_E(TAG, "Failed to delete %s", updated_file_path);
                    }

                    // check if the directory exists
                    if(storage_dir_exists(storage, updated_file_path) &&
                       storage_common_remove(storage, updated_file_path) != FSE_OK) {
                        FURI_LOG_E(TAG, "Failed to delete %s", updated_file_path);
                    }
                }

                // clean up
                clean_up("WiFi-SSID.txt");
                clean_up("WiFi-Password.txt");
                clean_up("Flip-Social-Username.txt");
                clean_up("Flip-Social-Password.txt");
                clean_up("Game-FPS.txt");
                clean_up("Game-Screen-Always-On.txt");
                clean_up("is_logged_in.txt");
                clean_up("data/worlds");
                clean_up("data/settings.bin");
            }
        }
    } else {
        // transfer files over into the data folder (to bs used to load the player context)
        char directory_path[128];
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");

        // Create the directory
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_mkdir(storage, directory_path);

        // copy the whole folder
        char source_path[128];
        snprintf(
            source_path, sizeof(source_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");

        if(storage_common_migrate(storage, source_path, directory_path) != FSE_OK) {
            FURI_LOG_E(TAG, "Failed to migrate files");
        } else {
            void clean_up(char* file_path) {
                char updated_file_path[128];
                snprintf(
                    updated_file_path,
                    sizeof(updated_file_path),
                    STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/%s",
                    file_path);

                // check if the file exists
                if(storage_file_exists(storage, updated_file_path) &&
                   storage_common_remove(storage, updated_file_path) != FSE_OK) {
                    FURI_LOG_E(TAG, "Failed to delete %s", updated_file_path);
                }

                // check if the directory exists
                if(storage_dir_exists(storage, updated_file_path) &&
                   storage_common_remove(storage, updated_file_path) != FSE_OK) {
                    FURI_LOG_E(TAG, "Failed to delete %s", updated_file_path);
                }
            }

            // clean up
            clean_up("WiFi-SSID.txt");
            clean_up("WiFi-Password.txt");
            clean_up("Flip-Social-Username.txt");
            clean_up("Flip-Social-Password.txt");
            clean_up("Game-FPS.txt");
            clean_up("Game-Screen-Always-On.txt");
            clean_up("is_logged_in.txt");
            clean_up("data/worlds");
            clean_up("data/settings.bin");
        }
    }

    // svae app version
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
