#ifndef WEB_CRAWLER_STORAGE_H
#define WEB_CRAWLER_STORAGE_H

#include <furi.h>
#include <storage/storage.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/" http_tag "/settings.bin"
#define RECEIVED_DATA_PATH                         \
    STORAGE_EXT_PATH_PREFIX "/apps_data/" http_tag \
                            "/" // add the file name to the end (e.g. "received_data.txt")

// will need to make a duplicate of the file
// one to save data, and the other for users to manipulate

#define MAX_RECEIVED_DATA_SIZE 1024
#define SHOW_MAX_FILE_SIZE     2048

// Define the truncation notice
#define TRUNCATION_NOTICE "\n\n[Data truncated due to size limits]"

// Function to save settings: path, SSID, and password
static void save_settings(
    const char* path,
    const char* ssid,
    const char* password,
    const char* file_rename,
    const char* file_type,
    const char* http_method,
    const char* headers,
    const char* payload) {
    // Create the directory for saving settings
    char directory_path[256];
    snprintf(
        directory_path,
        sizeof(directory_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/web_crawler_app");

    // Create the directory
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    // Open the settings file
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open settings file for writing: %s", SETTINGS_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(file_type == NULL || strlen(file_type) == 0) {
        file_type = ".txt";
    }

    // Save the SSID length and data
    size_t ssid_length = strlen(ssid) + 1; // Include null terminator
    if(storage_file_write(file, &ssid_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, ssid, ssid_length) != ssid_length) {
        FURI_LOG_E(TAG, "Failed to write SSID");
    }

    // Save the password length and data
    size_t password_length = strlen(password) + 1; // Include null terminator
    if(storage_file_write(file, &password_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, password, password_length) != password_length) {
        FURI_LOG_E(TAG, "Failed to write password");
    }

    // Save the path length and data
    size_t path_length = strlen(path) + 1; // Include null terminator
    if(storage_file_write(file, &path_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, path, path_length) != path_length) {
        FURI_LOG_E(TAG, "Failed to write path");
    }
    // Save the file rename length and data
    size_t file_rename_length = strlen(file_rename) + 1; // Include null terminator
    if(storage_file_write(file, &file_rename_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, file_rename, file_rename_length) != file_rename_length) {
        FURI_LOG_E(TAG, "Failed to write file rename");
    }

    // Save the file type length and data
    size_t file_type_length = strlen(file_type) + 1; // Include null terminator
    if(storage_file_write(file, &file_type_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, file_type, file_type_length) != file_type_length) {
        FURI_LOG_E(TAG, "Failed to write file type");
    }

    // Save the http method length and data
    size_t http_method_length = strlen(http_method) + 1; // Include null terminator
    if(storage_file_write(file, &http_method_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, http_method, http_method_length) != http_method_length) {
        FURI_LOG_E(TAG, "Failed to write http method");
    }

    // Save the headers length and data
    size_t headers_length = strlen(headers) + 1; // Include null terminator
    if(storage_file_write(file, &headers_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, headers, headers_length) != headers_length) {
        FURI_LOG_E(TAG, "Failed to write headers");
    }

    // Save the payload length and data
    size_t payload_length = strlen(payload) + 1; // Include null terminator
    if(storage_file_write(file, &payload_length, sizeof(size_t)) != sizeof(size_t) ||
       storage_file_write(file, payload, payload_length) != payload_length) {
        FURI_LOG_E(TAG, "Failed to write payload");
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Function to load settings (the variables must be opened in the order they were saved)
static bool load_settings(
    char* path,
    size_t path_size,
    char* ssid,
    size_t ssid_size,
    char* password,
    size_t password_size,
    char* file_rename,
    size_t file_rename_size,
    char* file_type,
    size_t file_type_size,
    char* http_method,
    size_t http_method_size,
    char* headers,
    size_t headers_size,
    char* payload,
    size_t payload_size,
    WebCrawlerApp* app) {
    if(!app) {
        FURI_LOG_E(TAG, "WebCrawlerApp is NULL");
        return false;
    }
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open settings file for reading: %s", SETTINGS_PATH);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false; // Return false if the file does not exist
    }

    // Load the SSID
    size_t ssid_length;
    if(storage_file_read(file, &ssid_length, sizeof(size_t)) != sizeof(size_t) ||
       ssid_length > ssid_size || storage_file_read(file, ssid, ssid_length) != ssid_length) {
        FURI_LOG_E(TAG, "Failed to read SSID");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    ssid[ssid_length - 1] = '\0'; // Ensure null-termination

    // Load the password
    size_t password_length;
    if(storage_file_read(file, &password_length, sizeof(size_t)) != sizeof(size_t) ||
       password_length > password_size ||
       storage_file_read(file, password, password_length) != password_length) {
        FURI_LOG_E(TAG, "Failed to read password");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    password[password_length - 1] = '\0'; // Ensure null-termination

    // Load the path
    size_t path_length;
    if(storage_file_read(file, &path_length, sizeof(size_t)) != sizeof(size_t) ||
       path_length > path_size || storage_file_read(file, path, path_length) != path_length) {
        FURI_LOG_E(TAG, "Failed to read path");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    path[path_length - 1] = '\0'; // Ensure null-termination

    // Load the file rename
    size_t file_rename_length;
    if(storage_file_read(file, &file_rename_length, sizeof(size_t)) != sizeof(size_t) ||
       file_rename_length > file_rename_size ||
       storage_file_read(file, file_rename, file_rename_length) != file_rename_length) {
        FURI_LOG_E(TAG, "Failed to read file rename");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    file_rename[file_rename_length - 1] = '\0'; // Ensure null-termination

    // Load the file type
    size_t file_type_length;
    if(storage_file_read(file, &file_type_length, sizeof(size_t)) != sizeof(size_t) ||
       file_type_length > file_type_size ||
       storage_file_read(file, file_type, file_type_length) != file_type_length) {
        FURI_LOG_E(TAG, "Failed to read file type");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    file_type[file_type_length - 1] = '\0'; // Ensure null-termination

    // Load the http method
    size_t http_method_length;
    if(storage_file_read(file, &http_method_length, sizeof(size_t)) != sizeof(size_t) ||
       http_method_length > http_method_size ||
       storage_file_read(file, http_method, http_method_length) != http_method_length) {
        FURI_LOG_E(TAG, "Failed to read http method");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Load the headers
    size_t headers_length;
    if(storage_file_read(file, &headers_length, sizeof(size_t)) != sizeof(size_t) ||
       headers_length > headers_size ||
       storage_file_read(file, headers, headers_length) != headers_length) {
        FURI_LOG_E(TAG, "Failed to read headers");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Load the payload
    size_t payload_length;
    if(storage_file_read(file, &payload_length, sizeof(size_t)) != sizeof(size_t) ||
       payload_length > payload_size ||
       storage_file_read(file, payload, payload_length) != payload_length) {
        FURI_LOG_E(TAG, "Failed to read payload");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // set the path, ssid, and password
    strncpy(app->path, path, path_size);
    strncpy(app->ssid, ssid, ssid_size);
    strncpy(app->password, password, password_size);
    strncpy(app->file_rename, file_rename, file_rename_size);
    strncpy(app->file_type, file_type, file_type_size);
    strncpy(app->http_method, http_method, http_method_size);
    strncpy(app->headers, headers, headers_size);
    strncpy(app->payload, payload, payload_size);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

static bool delete_received_data(WebCrawlerApp* app) {
    if(app == NULL) {
        FURI_LOG_E(TAG, "WebCrawlerApp is NULL");
        return false;
    }
    // Open the storage record
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    if(!storage_simply_remove_recursive(storage, RECEIVED_DATA_PATH "received_data.txt")) {
        FURI_LOG_E(TAG, "Failed to delete main file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Allocate memory for new_path
    char* new_path = malloc(256);
    if(new_path == NULL) {
        FURI_LOG_E(TAG, "Memory allocation failed for paths");
        free(new_path);
        return false;
    }

    if(app->file_type == NULL || strlen(app->file_type) == 0) {
        app->file_type = ".txt";
    }

    // Format the new_path
    int ret_new =
        snprintf(new_path, 256, "%s%s%s", RECEIVED_DATA_PATH, app->file_rename, app->file_type);
    if(ret_new < 0 || (size_t)ret_new >= 256) {
        FURI_LOG_E(TAG, "Failed to create new_path");
        free(new_path);
        return false;
    }

    if(!storage_simply_remove_recursive(storage, new_path)) {
        FURI_LOG_E(TAG, "Failed to delete duplicate file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    furi_record_close(RECORD_STORAGE);

    return true;
}

static bool rename_received_data(
    const char* old_name,
    const char* new_name,
    const char* file_type,
    const char* old_file_type) {
    // Open the storage record
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }
    // Allocate memory for old_path and new_path
    char* new_path = malloc(256);
    char* old_path = malloc(256);
    if(new_path == NULL || old_path == NULL) {
        FURI_LOG_E(TAG, "Memory allocation failed for paths");
        free(old_path);
        free(new_path);
        return false;
    }

    if(file_type == NULL || strlen(file_type) == 0) {
        file_type = ".txt";
    }
    if(old_file_type == NULL || strlen(old_file_type) == 0) {
        old_file_type = ".txt";
    }

    // Format the old_path
    int ret_old = snprintf(old_path, 256, "%s%s%s", RECEIVED_DATA_PATH, old_name, old_file_type);
    if(ret_old < 0 || (size_t)ret_old >= 256) {
        FURI_LOG_E(TAG, "Failed to create old_path");
        free(old_path);
        free(new_path);
        return false;
    }

    // Format the new_path
    int ret_new = snprintf(new_path, 256, "%s%s%s", RECEIVED_DATA_PATH, new_name, file_type);
    if(ret_new < 0 || (size_t)ret_new >= 256) {
        FURI_LOG_E(TAG, "Failed to create new_path");
        free(old_path);
        free(new_path);
        return false;
    }

    // Check if the file exists
    if(!storage_file_exists(storage, old_path)) {
        if(!storage_file_exists(storage, RECEIVED_DATA_PATH "received_data.txt")) {
            FURI_LOG_E(TAG, "No saved file exists");
            free(old_path);
            free(new_path);
            furi_record_close(RECORD_STORAGE);
            return false;
        } else {
            bool renamed =
                storage_common_copy(storage, RECEIVED_DATA_PATH "received_data.txt", new_path) ==
                FSE_OK;

            furi_record_close(RECORD_STORAGE);
            return renamed;
        }
    } else {
        bool renamed = storage_common_rename(storage, old_path, new_path) == FSE_OK;
        storage_simply_remove_recursive(storage, old_path);
        furi_record_close(RECORD_STORAGE);
        return renamed;
    }
}

static bool text_show_read_lines(File* file, FuriString* str_result) {
    // Reset the FuriString to ensure it's empty before reading
    furi_string_reset(str_result);

    // Define a buffer to hold the read data
    uint8_t buffer[SHOW_MAX_FILE_SIZE];

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, SHOW_MAX_FILE_SIZE);
    if(storage_file_get_error(file) != FSE_OK) {
        FURI_LOG_E(TAG, "Error reading from file.");
        return false;
    }

    // Append each byte to the FuriString
    for(size_t i = 0; i < read_count; i++) {
        furi_string_push_back(str_result, buffer[i]);
    }

    return true;
}

static bool load_received_data(WebCrawlerApp* app) {
    if(app == NULL) {
        FURI_LOG_E(TAG, "WebCrawlerApp is NULL");
        return false;
    }

    if(!app->widget_file_read) {
        FURI_LOG_E(TAG, "Textbox is NULL");
        return false;
    }

    // Open the storage record
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    // Allocate a file handle
    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to allocate storage file");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Open the file for reading
    if(!storage_file_open(
           file, RECEIVED_DATA_PATH "received_data.txt", FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false; // Return false if the file does not exist
    }

    // Allocate a FuriString to hold the received data
    FuriString* str_result = furi_string_alloc();
    if(!str_result) {
        FURI_LOG_E(TAG, "Failed to allocate FuriString");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Read data into the FuriString
    bool read_success = text_show_read_lines(file, str_result);
    if(!read_success) {
        FURI_LOG_E(TAG, "Failed to read data from file");
        furi_string_free(str_result);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Check if there is more data beyond the maximum size
    char extra_byte;
    storage_file_read(file, &extra_byte, 1);

    // Retrieve the C-string from FuriString
    const char* data_cstr = furi_string_get_cstr(str_result);
    // Set the text box with the received data

    widget_reset(app->widget_file_read);

    if(str_result != NULL) {
        widget_add_text_scroll_element(app->widget_file_read, 0, 0, 128, 64, data_cstr);
    } else {
        widget_add_text_scroll_element(app->widget_file_read, 0, 0, 128, 64, "File is empty.");
    }
    // Clean up
    furi_string_free(str_result);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

#endif // WEB_CRAWLER_STORAGE_H
