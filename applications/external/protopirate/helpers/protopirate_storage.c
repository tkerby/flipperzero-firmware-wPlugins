// helpers/protopirate_storage.c
#include "protopirate_storage.h"
#include <toolbox/stream/file_stream.h>
#include <toolbox/dir_walk.h>

#define TAG                  "ProtoPirateStorage"
#define MAX_FILES_TO_DISPLAY 50

// Structure to hold file info for sorting
typedef struct {
    char name[64];
    uint32_t timestamp;
} FileEntry;

bool protopirate_storage_init() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool protopirate_storage_get_next_filename(const char* protocol_name, FuriString* out_filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* temp_path = furi_string_alloc();
    uint32_t index = 0;
    bool found = false;

    // Find next available index
    while(!found && index < 999) {
        furi_string_printf(
            temp_path,
            "%s/%s_%03lu%s",
            PROTOPIRATE_APP_FOLDER,
            protocol_name,
            index,
            PROTOPIRATE_APP_EXTENSION);

        if(!storage_file_exists(storage, furi_string_get_cstr(temp_path))) {
            furi_string_set(out_filename, temp_path);
            found = true;
        } else {
            index++;
        }
    }

    furi_string_free(temp_path);
    furi_record_close(RECORD_STORAGE);

    return found;
}

bool protopirate_storage_save_capture(
    FlipperFormat* flipper_format,
    const char* protocol_name,
    FuriString* out_path) {
    if(!protopirate_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    FuriString* file_path = furi_string_alloc();

    if(!protopirate_storage_get_next_filename(protocol_name, file_path)) {
        FURI_LOG_E(TAG, "Failed to get next filename");
        furi_string_free(file_path);
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Create a new flipper format file for saving
    FlipperFormat* save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        if(!flipper_format_file_open_new(save_file, furi_string_get_cstr(file_path))) {
            FURI_LOG_E(TAG, "Failed to create file");
            break;
        }

        // Write standard SubGhz file header
        if(!flipper_format_write_header_cstr(save_file, "Flipper SubGhz Key File", 1)) {
            FURI_LOG_E(TAG, "Failed to write header");
            break;
        }

        // Rewind source format to beginning
        flipper_format_rewind(flipper_format);

        // Copy ALL data from the source flipper format
        FuriString* string_value = furi_string_alloc();
        uint32_t uint32_value;
        uint32_t* uint32_array = NULL;
        uint32_t uint32_array_size = 0;

        // Protocol name
        if(flipper_format_read_string(flipper_format, "Protocol", string_value)) {
            flipper_format_write_string(save_file, "Protocol", string_value);
        }

        // Bit count
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Bit", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Bit", &uint32_value, 1);
        }

        // Key data
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_string(flipper_format, "Key", string_value)) {
            flipper_format_write_string(save_file, "Key", string_value);
        } else {
            flipper_format_rewind(flipper_format);
            if(flipper_format_get_value_count(flipper_format, "Key", &uint32_array_size) &&
               uint32_array_size > 0) {
                uint32_array = malloc(sizeof(uint32_t) * uint32_array_size);
                if(flipper_format_read_uint32(
                       flipper_format, "Key", uint32_array, uint32_array_size)) {
                    flipper_format_write_uint32(save_file, "Key", uint32_array, uint32_array_size);
                }
                free(uint32_array);
                uint32_array = NULL;
            }
        }

        // Frequency
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Frequency", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Frequency", &uint32_value, 1);
        }

        // Preset
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_string(flipper_format, "Preset", string_value)) {
            flipper_format_write_string(save_file, "Preset", string_value);
        }

        // Custom preset data if exists
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(
               flipper_format, "Custom_preset_module", &uint32_array_size) &&
           uint32_array_size > 0) {
            if(flipper_format_read_string(flipper_format, "Custom_preset_module", string_value)) {
                flipper_format_write_string(save_file, "Custom_preset_module", string_value);
            }
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(
               flipper_format, "Custom_preset_data", &uint32_array_size) &&
           uint32_array_size > 0) {
            uint8_t* custom_data = malloc(uint32_array_size);
            if(flipper_format_read_hex(
                   flipper_format, "Custom_preset_data", custom_data, uint32_array_size)) {
                flipper_format_write_hex(
                    save_file, "Custom_preset_data", custom_data, uint32_array_size);
            }
            free(custom_data);
        }

        // Protocol-specific fields - TE
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "TE", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "TE", &uint32_value, 1);
        }

        // Serial number
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Serial", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Serial", &uint32_value, 1);
        }

        // Button
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Btn", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Btn", &uint32_value, 1);
        }

        // Counter
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Cnt", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Cnt", &uint32_value, 1);
        }

        // CRC
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "CRC", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "CRC", &uint32_value, 1);
        }

        // Type (for VW)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Type", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Type", &uint32_value, 1);
        }

        // Check (for VW)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Check", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Check", &uint32_value, 1);
        }

        // Raw data arrays
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(flipper_format, "RAW_Data", &uint32_array_size) &&
           uint32_array_size > 0) {
            uint32_array = malloc(sizeof(uint32_t) * uint32_array_size);
            if(flipper_format_read_uint32(
                   flipper_format, "RAW_Data", uint32_array, uint32_array_size)) {
                flipper_format_write_uint32(
                    save_file, "RAW_Data", uint32_array, uint32_array_size);
            }
            free(uint32_array);
        }

        // DataHi/DataLo
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "DataHi", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "DataHi", &uint32_value, 1);
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "DataLo", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "DataLo", &uint32_value, 1);
        }

        // RawCnt for Kia V2
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "RawCnt", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "RawCnt", &uint32_value, 1);
        }

        // Encrypted/Decrypted for Kia V3/V4
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Encrypted", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Encrypted", &uint32_value, 1);
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Decrypted", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Decrypted", &uint32_value, 1);
        }

        // Version for Kia V3/V4
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Version", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "Version", &uint32_value, 1);
        }

        // BS byte for Ford
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "BS", &uint32_value, 1)) {
            flipper_format_write_uint32(save_file, "BS", &uint32_value, 1);
        }

        furi_string_free(string_value);

        if(out_path) {
            furi_string_set(out_path, file_path);
        }

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", furi_string_get_cstr(file_path));

    } while(false);

    flipper_format_free(save_file);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);

    return result;
}

// Static array to hold sorted file entries
static FileEntry* g_file_entries = NULL;
static uint32_t g_file_count = 0;

// Build the sorted file list
static void protopirate_storage_build_file_list(void) {
    // Free previous list
    if(g_file_entries) {
        free(g_file_entries);
        g_file_entries = NULL;
    }
    g_file_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    FileInfo file_info;

    // First pass: count files
    uint32_t total_count = 0;
    if(storage_dir_open(dir, PROTOPIRATE_APP_FOLDER)) {
        char name[256];
        while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
            if(!file_info_is_dir(&file_info) && strstr(name, PROTOPIRATE_APP_EXTENSION)) {
                total_count++;
            }
        }
        storage_dir_close(dir);
    }

    if(total_count == 0) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Allocate array
    g_file_entries = malloc(sizeof(FileEntry) * total_count);
    if(!g_file_entries) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Second pass: collect file info
    FuriString* full_path = furi_string_alloc();
    if(storage_dir_open(dir, PROTOPIRATE_APP_FOLDER)) {
        char name[256];
        while(storage_dir_read(dir, &file_info, name, sizeof(name)) &&
              g_file_count < total_count) {
            if(!file_info_is_dir(&file_info) && strstr(name, PROTOPIRATE_APP_EXTENSION)) {
                // Copy name (without extension for display)
                strncpy(
                    g_file_entries[g_file_count].name,
                    name,
                    sizeof(g_file_entries[g_file_count].name) - 1);
                g_file_entries[g_file_count].name[sizeof(g_file_entries[g_file_count].name) - 1] =
                    '\0';

                // Remove extension
                char* dot = strrchr(g_file_entries[g_file_count].name, '.');
                if(dot) *dot = '\0';

                // Get file timestamp
                furi_string_printf(full_path, "%s/%s", PROTOPIRATE_APP_FOLDER, name);

                // File Count
                g_file_entries[g_file_count].timestamp = total_count - g_file_count;

                g_file_count++;
            }
        }
        storage_dir_close(dir);
    }
    furi_string_free(full_path);

    // Reverse file count so newest files are first
    if(g_file_count > 1) {
        // Reverse the array so newest (last found) is first
        for(uint32_t i = 0; i < g_file_count / 2; i++) {
            FileEntry temp = g_file_entries[i];
            g_file_entries[i] = g_file_entries[g_file_count - 1 - i];
            g_file_entries[g_file_count - 1 - i] = temp;
        }
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "Built file list with %lu entries", g_file_count);
}

uint32_t protopirate_storage_get_file_count() {
    // Rebuild the file list each time we're asked for count
    protopirate_storage_build_file_list();
    return g_file_count;
}

bool protopirate_storage_get_file_by_index(
    uint32_t index,
    FuriString* out_path,
    FuriString* out_name) {
    if(!g_file_entries || index >= g_file_count) {
        return false;
    }

    if(out_path) {
        furi_string_printf(
            out_path,
            "%s/%s%s",
            PROTOPIRATE_APP_FOLDER,
            g_file_entries[index].name,
            PROTOPIRATE_APP_EXTENSION);
    }
    if(out_name) {
        furi_string_set_str(out_name, g_file_entries[index].name);
    }

    return true;
}

bool protopirate_storage_delete_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, file_path);
    FURI_LOG_I(TAG, "Delete file %s: %s", file_path, result ? "OK" : "FAILED");
    furi_record_close(RECORD_STORAGE);
    return result;
}

FlipperFormat* protopirate_storage_load_file(const char* file_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    if(!flipper_format_file_open_existing(flipper_format, file_path)) {
        FURI_LOG_E(TAG, "Failed to open file %s", file_path);
        flipper_format_free(flipper_format);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    return flipper_format;
}

// Call this when exiting the app to free memory
void protopirate_storage_free_file_list(void) {
    if(g_file_entries) {
        free(g_file_entries);
        g_file_entries = NULL;
    }
    g_file_count = 0;
}
