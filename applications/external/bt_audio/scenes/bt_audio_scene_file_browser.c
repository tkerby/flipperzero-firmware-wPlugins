#include "../bt_audio.h"
#include <string.h>

// Maximum files to display in browser
#define FILE_BROWSER_MAX_FILES         50
// Maximum path length
#define FILE_BROWSER_MAX_PATH          256
// Maximum filename length (excluding path)
#define FILE_BROWSER_MAX_NAME          64
// Display name buffer size: "[DIR] " prefix (6 chars) + name + null terminator
#define FILE_BROWSER_DISPLAY_NAME_SIZE (FILE_BROWSER_MAX_NAME + 8)

// File entry structure for browser
typedef struct {
    char name[FILE_BROWSER_MAX_NAME];
    char full_path[FILE_BROWSER_MAX_PATH];
    bool is_directory;
} FileBrowserEntry;

// File browser state (static to persist across scene callbacks)
static FileBrowserEntry file_entries[FILE_BROWSER_MAX_FILES];
static uint32_t file_entry_count = 0;
static char current_path[FILE_BROWSER_MAX_PATH] = "";
static char selected_file[FILE_BROWSER_MAX_PATH] = "";

// ESP32 SD browsing state
static bool esp32_listing_in_progress = false;
static bool esp32_listing_complete = false;
static bool esp32_listing_error = false;
static char esp32_error_message[64] = "";

// Submenu indices - offset to avoid collision with other events
#define FILE_BROWSER_INDEX_BASE    1000
#define FILE_BROWSER_INDEX_PARENT  999 // ".." entry
#define FILE_BROWSER_INDEX_REFRESH 998 // Refresh option for ESP32 mode

// Forward declarations
static void scan_directory(BtAudio* app, const char* path);
static void scan_esp32_directory(BtAudio* app, const char* path);
static void bt_audio_file_browser_submenu_callback(void* context, uint32_t index);

/**
 * UART callback for file browser scene
 * Handles responses from ESP32 for file listing and playback
 */
static void bt_audio_file_browser_rx_callback(const char* data, size_t len, void* context) {
    BtAudio* app = context;
    UNUSED(len);

    FURI_LOG_I(TAG, "File Browser RX: %s", data);

    // Handle ESP32 file listing responses
    if(strncmp(data, "FILE:", 5) == 0) {
        // Parse file entry: FILE:<full_path>|<filename>|<type>
        if(file_entry_count < FILE_BROWSER_MAX_FILES) {
            const char* entry = data + 5;

            // Find first separator
            const char* sep1 = strchr(entry, '|');
            if(sep1) {
                // Find second separator
                const char* sep2 = strchr(sep1 + 1, '|');
                if(sep2) {
                    // Extract full path
                    size_t path_len = sep1 - entry;
                    if(path_len >= FILE_BROWSER_MAX_PATH) path_len = FILE_BROWSER_MAX_PATH - 1;
                    strncpy(file_entries[file_entry_count].full_path, entry, path_len);
                    file_entries[file_entry_count].full_path[path_len] = '\0';

                    // Extract filename
                    size_t name_len = sep2 - (sep1 + 1);
                    if(name_len >= FILE_BROWSER_MAX_NAME) name_len = FILE_BROWSER_MAX_NAME - 1;
                    strncpy(file_entries[file_entry_count].name, sep1 + 1, name_len);
                    file_entries[file_entry_count].name[name_len] = '\0';

                    // Extract type (d = directory, f = file)
                    file_entries[file_entry_count].is_directory = (*(sep2 + 1) == 'd');

                    file_entry_count++;
                    FURI_LOG_D(
                        TAG,
                        "Added entry: %s (dir=%d)",
                        file_entries[file_entry_count - 1].name,
                        file_entries[file_entry_count - 1].is_directory);
                }
            }
        }
    } else if(strncmp(data, "LIST_FILES_PATH:", 16) == 0) {
        // Store current path being listed
        strncpy(current_path, data + 16, sizeof(current_path) - 1);
        current_path[sizeof(current_path) - 1] = '\0';
        FURI_LOG_I(TAG, "Listing path: %s", current_path);
    } else if(strncmp(data, "LIST_FILES_COUNT:", 17) == 0) {
        // Just log the count
        FURI_LOG_I(TAG, "File count: %s", data + 17);
    } else if(strcmp(data, "LIST_FILES_END") == 0) {
        // Listing complete
        esp32_listing_complete = true;
        esp32_listing_in_progress = false;
        FURI_LOG_I(TAG, "ESP32 listing complete, %lu entries", (unsigned long)file_entry_count);
        // Send event to rebuild menu
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventStatusComplete);
    } else if(strncmp(data, "LIST_FILES_ERROR:", 17) == 0) {
        // Listing error
        esp32_listing_error = true;
        esp32_listing_in_progress = false;
        strncpy(esp32_error_message, data + 17, sizeof(esp32_error_message) - 1);
        esp32_error_message[sizeof(esp32_error_message) - 1] = '\0';
        FURI_LOG_W(TAG, "ESP32 listing error: %s", esp32_error_message);
        view_dispatcher_send_custom_event(app->view_dispatcher, BtAudioEventStatusComplete);
    }
    // Handle playback responses
    else if(strncmp(data, "PLAYING_FILE:", 13) == 0) {
        strncpy(app->current_filename, data + 13, BT_AUDIO_FILENAME_LEN - 1);
        app->current_filename[BT_AUDIO_FILENAME_LEN - 1] = '\0';
        app->is_playing = true;
        app->is_paused = false;
        // Switch to control scene to show player
        scene_manager_next_scene(app->scene_manager, BtAudioSceneControl);
    } else if(strncmp(data, "PLAYING", 7) == 0) {
        app->is_playing = true;
        app->is_paused = false;
    } else if(strncmp(data, "ERROR:", 6) == 0) {
        FURI_LOG_W(TAG, "ESP32 error: %s", data + 6);
    } else if(strncmp(data, "FILE_NOT_FOUND:", 15) == 0) {
        FURI_LOG_W(TAG, "File not found: %s", data + 15);
    }
}

/**
 * Request ESP32 to list files in a directory
 * The actual listing is received asynchronously via UART callback
 */
static void scan_esp32_directory(BtAudio* app, const char* path) {
    // Reset state
    file_entry_count = 0;
    esp32_listing_in_progress = true;
    esp32_listing_complete = false;
    esp32_listing_error = false;
    esp32_error_message[0] = '\0';

    // Send LIST_FILES command
    // Buffer: "LIST_FILES:" (11) + path (256 max) + "\n" (1) + null (1) = 269 max
    char cmd[FILE_BROWSER_MAX_PATH + 20];
    if(path == NULL || path[0] == '\0') {
        // Default: list root music folder
        bt_audio_uart_tx(app->uart, "LIST_FILES\n");
        FURI_LOG_I(TAG, "Requesting ESP32 file list (default path)");
    } else {
        // List specific directory
        snprintf(cmd, sizeof(cmd), "LIST_FILES:%s\n", path);
        bt_audio_uart_tx(app->uart, cmd);
        FURI_LOG_I(TAG, "Requesting ESP32 file list: %s", path);
    }
}

/**
 * Scan a directory for MP3 files and subdirectories (Flipper SD only)
 */
static void scan_directory(BtAudio* app, const char* path) {
    file_entry_count = 0;

    // ESP32 SD mode - use separate function
    if(app->config.sd_source != BtAudioSdSourceFlipper) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    // Determine the path - use default if empty
    const char* scan_path = (path[0] == '\0') ? "/ext/music" : path;

    // Save current path
    strncpy(current_path, scan_path, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';

    if(!storage_dir_open(dir, scan_path)) {
        FURI_LOG_W(TAG, "Failed to open directory: %s", scan_path);
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo fileinfo;
    char name[FILE_BROWSER_MAX_NAME];

    // Collect directories first, then files
    // First pass: directories
    while(storage_dir_read(dir, &fileinfo, name, sizeof(name)) &&
          file_entry_count < FILE_BROWSER_MAX_FILES) {
        if(file_info_is_dir(&fileinfo)) {
            // Skip hidden directories
            if(name[0] == '.') continue;

            strncpy(file_entries[file_entry_count].name, name, sizeof(file_entries[0].name) - 1);
            file_entries[file_entry_count].name[sizeof(file_entries[0].name) - 1] = '\0';

            snprintf(
                file_entries[file_entry_count].full_path,
                FILE_BROWSER_MAX_PATH,
                "%s/%s",
                scan_path,
                name);
            file_entries[file_entry_count].is_directory = true;
            file_entry_count++;
        }
    }
    storage_dir_close(dir);

    // Second pass: MP3 files
    if(storage_dir_open(dir, scan_path)) {
        while(storage_dir_read(dir, &fileinfo, name, sizeof(name)) &&
              file_entry_count < FILE_BROWSER_MAX_FILES) {
            if(!file_info_is_dir(&fileinfo)) {
                // Check for MP3 extension (case insensitive)
                size_t len = strlen(name);
                if(len > 4) {
                    const char* ext = name + len - 4;
                    if(strcasecmp(ext, ".mp3") == 0) {
                        strncpy(
                            file_entries[file_entry_count].name,
                            name,
                            sizeof(file_entries[0].name) - 1);
                        file_entries[file_entry_count].name[sizeof(file_entries[0].name) - 1] =
                            '\0';

                        snprintf(
                            file_entries[file_entry_count].full_path,
                            FILE_BROWSER_MAX_PATH,
                            "%s/%s",
                            scan_path,
                            name);
                        file_entries[file_entry_count].is_directory = false;
                        file_entry_count++;
                    }
                }
            }
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Found %lu entries in %s", (unsigned long)file_entry_count, scan_path);
}

/**
 * Build submenu with directory contents
 */
static void build_file_browser_menu(BtAudio* app) {
    Submenu* submenu = app->submenu;

    // Add parent directory navigation if not at root
    const char* root_path = (app->config.sd_source == BtAudioSdSourceFlipper) ? "/ext/music" :
                                                                                "/music";
    if(strcmp(current_path, root_path) != 0 && strlen(current_path) > strlen(root_path)) {
        submenu_add_item(
            submenu,
            "[..] Parent Directory",
            FILE_BROWSER_INDEX_PARENT,
            bt_audio_file_browser_submenu_callback,
            app);
    }

    // Add refresh option for ESP32 mode
    if(app->config.sd_source != BtAudioSdSourceFlipper) {
        submenu_add_item(
            submenu,
            "[Refresh]",
            FILE_BROWSER_INDEX_REFRESH,
            bt_audio_file_browser_submenu_callback,
            app);
    }

    // Add all entries
    for(uint32_t i = 0; i < file_entry_count; i++) {
        char display_name[FILE_BROWSER_DISPLAY_NAME_SIZE];
        // Use precision specifier to limit name length, derived from FILE_BROWSER_MAX_NAME
        const int max_name_len = FILE_BROWSER_MAX_NAME - 1;
        if(file_entries[i].is_directory) {
            snprintf(
                display_name,
                sizeof(display_name),
                "[DIR] %.*s",
                max_name_len,
                file_entries[i].name);
        } else {
            snprintf(
                display_name, sizeof(display_name), "%.*s", max_name_len, file_entries[i].name);
        }
        submenu_add_item(
            submenu,
            display_name,
            FILE_BROWSER_INDEX_BASE + i,
            bt_audio_file_browser_submenu_callback,
            app);
    }
}

static void bt_audio_file_browser_submenu_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_audio_scene_file_browser_on_enter(void* context) {
    BtAudio* app = context;
    Submenu* submenu = app->submenu;

    // Set up UART callback for both modes
    bt_audio_uart_set_rx_callback(app->uart, bt_audio_file_browser_rx_callback);

    // Check SD source
    if(app->config.sd_source != BtAudioSdSourceFlipper) {
        // ESP32 SD mode - request file listing from ESP32
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "ESP32 SD Browser");
        widget_add_string_element(
            app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "Loading file list...");
        view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);

        // Request file listing from ESP32
        scan_esp32_directory(app, "");
        return;
    }

    // Flipper SD mode - scan local directory
    scan_directory(app, "");

    // Build the menu
    submenu_set_header(submenu, "Browse Music");
    build_file_browser_menu(app);

    if(file_entry_count == 0) {
        // No files found - show message
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "No MP3 Files");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Add files to");
        widget_add_string_element(
            app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "/ext/music/");
        view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
    }
}

bool bt_audio_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Handle ESP32 listing completion event
        if(event.event == BtAudioEventStatusComplete) {
            if(app->config.sd_source != BtAudioSdSourceFlipper) {
                // ESP32 mode - listing complete, build menu
                if(esp32_listing_error) {
                    // Show error
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget,
                        64,
                        16,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "ESP32 SD Error");
                    widget_add_string_element(
                        app->widget,
                        64,
                        32,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        esp32_error_message);
                    widget_add_string_element(
                        app->widget,
                        64,
                        48,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Check SD card");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                } else if(file_entry_count == 0) {
                    // No files found
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "No MP3 Files");
                    widget_add_string_element(
                        app->widget,
                        64,
                        32,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "Add files to ESP32");
                    widget_add_string_element(
                        app->widget,
                        64,
                        44,
                        AlignCenter,
                        AlignCenter,
                        FontSecondary,
                        "/music/ folder");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                } else {
                    // Build menu with received files
                    submenu_reset(app->submenu);
                    submenu_set_header(app->submenu, "ESP32 SD Browser");
                    build_file_browser_menu(app);
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewSubmenu);
                }
                consumed = true;
            }
        } else if(event.event == FILE_BROWSER_INDEX_REFRESH) {
            // Refresh ESP32 file listing
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "ESP32 SD Browser");
            widget_add_string_element(
                app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "Refreshing...");
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
            scan_esp32_directory(app, current_path);
            consumed = true;
        } else if(event.event == FILE_BROWSER_INDEX_PARENT) {
            // Navigate to parent directory safely
            const char* root_path =
                (app->config.sd_source == BtAudioSdSourceFlipper) ? "/ext/music" : "/music";
            size_t root_len = strlen(root_path);
            size_t current_len = strlen(current_path);

            // Only navigate up if not at root
            if(current_len > root_len) {
                char* last_slash = strrchr(current_path, '/');
                // Ensure we don't go above root and have a valid path
                if(last_slash && (size_t)(last_slash - current_path) >= root_len) {
                    *last_slash = '\0';
                }
            }

            // Rescan and rebuild menu
            submenu_reset(app->submenu);

            if(app->config.sd_source == BtAudioSdSourceFlipper) {
                scan_directory(app, current_path);
                submenu_set_header(app->submenu, "Browse Music");
                build_file_browser_menu(app);
            } else {
                // ESP32 mode - request new listing
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "ESP32 SD Browser");
                widget_add_string_element(
                    app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "Loading...");
                view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                scan_esp32_directory(app, current_path);
            }

            consumed = true;
        } else if(
            event.event >= FILE_BROWSER_INDEX_BASE &&
            event.event < FILE_BROWSER_INDEX_BASE + file_entry_count) {
            // File or directory selected
            uint32_t index = event.event - FILE_BROWSER_INDEX_BASE;

            if(file_entries[index].is_directory) {
                // Navigate into directory
                submenu_reset(app->submenu);

                if(app->config.sd_source == BtAudioSdSourceFlipper) {
                    scan_directory(app, file_entries[index].full_path);
                    submenu_set_header(app->submenu, "Browse Music");
                    build_file_browser_menu(app);
                } else {
                    // ESP32 mode - request new listing
                    widget_reset(app->widget);
                    widget_add_string_element(
                        app->widget,
                        64,
                        20,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "ESP32 SD Browser");
                    widget_add_string_element(
                        app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "Loading...");
                    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                    scan_esp32_directory(app, file_entries[index].full_path);
                }
            } else {
                // File selected - play it
                strncpy(selected_file, file_entries[index].full_path, sizeof(selected_file) - 1);
                selected_file[sizeof(selected_file) - 1] = '\0';

                FURI_LOG_I(TAG, "Selected file: %s", selected_file);

                // Update display info
                strncpy(
                    app->current_filename, file_entries[index].name, BT_AUDIO_FILENAME_LEN - 1);
                app->current_filename[BT_AUDIO_FILENAME_LEN - 1] = '\0';
                app->playing_favorites = false;

                if(app->config.sd_source == BtAudioSdSourceFlipper) {
                    // Flipper SD mode - stream the file
                    app->is_playing = true;
                    app->is_paused = false;

                    if(bt_audio_stream_flipper_sd_file(app, selected_file)) {
                        // Success - go to control scene
                        scene_manager_next_scene(app->scene_manager, BtAudioSceneControl);
                    } else {
                        // Failed
                        widget_reset(app->widget);
                        widget_add_string_element(
                            app->widget,
                            64,
                            28,
                            AlignCenter,
                            AlignCenter,
                            FontPrimary,
                            "Stream Failed");
                        widget_add_string_element(
                            app->widget,
                            64,
                            44,
                            AlignCenter,
                            AlignCenter,
                            FontSecondary,
                            "Check connection");
                        view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewWidget);
                    }
                } else {
                    // ESP32 SD mode - send PLAY_FILE command with directory context for continuous play
                    if(app->config.continuous_play) {
                        // Continuous play mode: send directory and file info
                        // Format: PLAY_FILE_DIR:<directory>|<filename>
                        // ESP32 will scan the directory and start playback from the selected file
                        char cmd[FILE_BROWSER_MAX_PATH * 2 + 32];
                        int cmd_len = snprintf(
                            cmd,
                            sizeof(cmd),
                            "PLAY_FILE_DIR:%s|%s\n",
                            current_path,
                            file_entries[index].name);
                        if(cmd_len < 0 || cmd_len >= (int)sizeof(cmd)) {
                            FURI_LOG_W(TAG, "Command may be truncated due to long path");
                        }
                        bt_audio_uart_tx(app->uart, cmd);
                        FURI_LOG_I(
                            TAG,
                            "Requesting ESP32 continuous play from: %s, starting with: %s",
                            current_path,
                            file_entries[index].name);
                    } else {
                        // Single file repeat mode: send just the file path
                        char cmd[FILE_BROWSER_MAX_PATH + 16];
                        int cmd_len = snprintf(cmd, sizeof(cmd), "PLAY_FILE:%s\n", selected_file);
                        if(cmd_len < 0 || cmd_len >= (int)sizeof(cmd)) {
                            FURI_LOG_W(TAG, "Command may be truncated due to long path");
                        }
                        bt_audio_uart_tx(app->uart, cmd);
                        FURI_LOG_I(TAG, "Requesting ESP32 to play (repeat): %s", selected_file);
                    }
                    // The UART callback will handle the PLAYING_FILE response and switch to control scene
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_file_browser_on_exit(void* context) {
    BtAudio* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);

    // Reset ESP32 listing state
    esp32_listing_in_progress = false;
    esp32_listing_complete = false;
    esp32_listing_error = false;
}
