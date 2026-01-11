// helpers/protopirate_storage.c
#include "protopirate_storage.h"
#include <toolbox/stream/file_stream.h>
#include <toolbox/dir_walk.h>

#define TAG "ProtoPirateStorage"
#define MAX_FILES_TO_DISPLAY 50

// Structure to hold file info for sorting
typedef struct {
    char name[64];
    uint32_t timestamp;
} FileEntry;

// Static array to hold sorted file entries
static FileEntry *g_file_entries = NULL;
static uint32_t g_file_count = 0;
static bool g_file_list_valid = false;

// Initialize storage and create app folder if needed
bool protopirate_storage_init(void)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
    return result;
}

// Sanitize protocol name for use as filename
static void sanitize_filename(const char *input, char *output, size_t output_size)
{
    if(!input || !output || output_size == 0) {
        if(output && output_size > 0) output[0] = '\0';
        return;
    }
    
    size_t i = 0, j = 0;
    while(input[i] != '\0' && j < output_size - 1)
    {
        char c = input[i];
        // Replace characters that are invalid in filenames
        if(c == '/' || c == '\\' || c == ':' || c == '*' || 
           c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ' ')
        {
            output[j] = '_';
        }
        else
        {
            output[j] = c;
        }
        i++;
        j++;
    }
    output[j] = '\0';
}

// Find next available filename for a protocol
bool protopirate_storage_get_next_filename(
    const char *protocol_name,
    FuriString *out_filename)
{
    if(!protocol_name || !out_filename) return false;
    
    Storage *storage = furi_record_open(RECORD_STORAGE);
    FuriString *temp_path = furi_string_alloc();
    uint32_t index = 0;
    bool found = false;

    // Sanitize protocol name for filename
    char safe_name[64];
    sanitize_filename(protocol_name, safe_name, sizeof(safe_name));
    
    // Handle empty protocol name
    if(safe_name[0] == '\0') {
        strncpy(safe_name, "Unknown", sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
    }

    // Find next available index (up to 9999)
    while(!found && index < 9999)
    {
        furi_string_printf(
            temp_path,
            "%s/%s_%04lu%s",
            PROTOPIRATE_APP_FOLDER,
            safe_name,
            (unsigned long)index,
            PROTOPIRATE_APP_EXTENSION);

        if(!storage_file_exists(storage, furi_string_get_cstr(temp_path)))
        {
            furi_string_set(out_filename, temp_path);
            found = true;
        }
        else
        {
            index++;
        }
    }

    furi_string_free(temp_path);
    furi_record_close(RECORD_STORAGE);
    return found;
}

// Save a capture to a new file
bool protopirate_storage_save_capture(
    FlipperFormat *flipper_format,
    const char *protocol_name,
    FuriString *out_path)
{
    if(!flipper_format || !protocol_name) {
        FURI_LOG_E(TAG, "Invalid parameters");
        return false;
    }
    
    if(!protopirate_storage_init()) {
        FURI_LOG_E(TAG, "Failed to create app folder");
        return false;
    }

    FuriString *file_path = furi_string_alloc();
    if(!file_path) return false;

    if(!protopirate_storage_get_next_filename(protocol_name, file_path)) {
        FURI_LOG_E(TAG, "Failed to get next filename");
        furi_string_free(file_path);
        return false;
    }

    Storage *storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat *save_file = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        // Create new file
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

        // Allocate temporary string for reading values
        FuriString *string_value = furi_string_alloc();
        if(!string_value) break;
        
        uint32_t uint32_value;
        uint32_t uint32_array_size = 0;

        // Copy all fields from source to destination

        // Protocol name
        if(flipper_format_read_string(flipper_format, "Protocol", string_value))
            flipper_format_write_string(save_file, "Protocol", string_value);

        // Bit count
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Bit", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Bit", &uint32_value, 1);

        // Key data - try string first, then hex array
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_string(flipper_format, "Key", string_value)) {
            flipper_format_write_string(save_file, "Key", string_value);
        } else {
            flipper_format_rewind(flipper_format);
            if(flipper_format_get_value_count(flipper_format, "Key", &uint32_array_size) && uint32_array_size > 0) {
                uint8_t *key_data = malloc(uint32_array_size);
                if(key_data) {
                    if(flipper_format_read_hex(flipper_format, "Key", key_data, uint32_array_size))
                        flipper_format_write_hex(save_file, "Key", key_data, uint32_array_size);
                    free(key_data);
                }
            }
        }

        // Frequency
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Frequency", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Frequency", &uint32_value, 1);

        // Preset (modulation)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_string(flipper_format, "Preset", string_value))
            flipper_format_write_string(save_file, "Preset", string_value);

        // Custom preset module if exists
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(flipper_format, "Custom_preset_module", &uint32_array_size) && uint32_array_size > 0) {
            if(flipper_format_read_string(flipper_format, "Custom_preset_module", string_value))
                flipper_format_write_string(save_file, "Custom_preset_module", string_value);
        }

        // Custom preset data if exists
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(flipper_format, "Custom_preset_data", &uint32_array_size) && uint32_array_size > 0) {
            uint8_t *custom_data = malloc(uint32_array_size);
            if(custom_data) {
                if(flipper_format_read_hex(flipper_format, "Custom_preset_data", custom_data, uint32_array_size))
                    flipper_format_write_hex(save_file, "Custom_preset_data", custom_data, uint32_array_size);
                free(custom_data);
            }
        }

        // TE (timing element)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "TE", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "TE", &uint32_value, 1);

        // Serial number
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Serial", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Serial", &uint32_value, 1);

        // Button
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Btn", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Btn", &uint32_value, 1);

        // Counter
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Cnt", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Cnt", &uint32_value, 1);

        // CRC
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "CRC", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "CRC", &uint32_value, 1);

        // Type (for VW protocols)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Type", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Type", &uint32_value, 1);

        // Check (for VW protocols)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Check", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Check", &uint32_value, 1);

        // RAW_Data arrays
        flipper_format_rewind(flipper_format);
        if(flipper_format_get_value_count(flipper_format, "RAW_Data", &uint32_array_size) && uint32_array_size > 0) {
            int32_t *raw_array = malloc(sizeof(int32_t) * uint32_array_size);
            if(raw_array) {
                if(flipper_format_read_int32(flipper_format, "RAW_Data", raw_array, uint32_array_size))
                    flipper_format_write_int32(save_file, "RAW_Data", raw_array, uint32_array_size);
                free(raw_array);
            }
        }

        // DataHi (high bits)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "DataHi", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "DataHi", &uint32_value, 1);

        // DataLo (low bits)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "DataLo", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "DataLo", &uint32_value, 1);

        // RawCnt (for Kia V2)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "RawCnt", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "RawCnt", &uint32_value, 1);

        // Encrypted (for Kia V3/V4)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Encrypted", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Encrypted", &uint32_value, 1);

        // Decrypted (for Kia V3/V4)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Decrypted", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Decrypted", &uint32_value, 1);

        // Version (for Kia V3/V4)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Version", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "Version", &uint32_value, 1);

        // BS byte (for Ford)
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "BS", &uint32_value, 1))
            flipper_format_write_uint32(save_file, "BS", &uint32_value, 1);

        furi_string_free(string_value);

        // Return the saved path if requested
        if(out_path) furi_string_set(out_path, file_path);

        // Invalidate file list cache since we added a new file
        g_file_list_valid = false;

        result = true;
        FURI_LOG_I(TAG, "Saved capture to %s", furi_string_get_cstr(file_path));

    } while(false);

    flipper_format_free(save_file);
    furi_string_free(file_path);
    furi_record_close(RECORD_STORAGE);
    return result;
}

// Build the sorted file list from storage
static void protopirate_storage_build_file_list(void)
{
    // Free previous list
    if(g_file_entries) {
        free(g_file_entries);
        g_file_entries = NULL;
    }
    g_file_count = 0;

    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *dir = storage_file_alloc(storage);
    FileInfo file_info;

    // First pass: count files
    uint32_t total_count = 0;
    if(storage_dir_open(dir, PROTOPIRATE_APP_FOLDER)) {
        char name[256];
        while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
            if(!file_info_is_dir(&file_info) && strstr(name, PROTOPIRATE_APP_EXTENSION))
                total_count++;
        }
        storage_dir_close(dir);
    }

    if(total_count == 0) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        g_file_list_valid = true;
        return;
    }

    // Allocate array for file entries
    g_file_entries = malloc(sizeof(FileEntry) * total_count);
    if(!g_file_entries) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    // Second pass: collect file info
    if(storage_dir_open(dir, PROTOPIRATE_APP_FOLDER)) {
        char name[256];
        while(storage_dir_read(dir, &file_info, name, sizeof(name)) && g_file_count < total_count) {
            if(!file_info_is_dir(&file_info) && strstr(name, PROTOPIRATE_APP_EXTENSION)) {
                // Copy name (without extension for display)
                strncpy(g_file_entries[g_file_count].name, name, sizeof(g_file_entries[g_file_count].name) - 1);
                g_file_entries[g_file_count].name[sizeof(g_file_entries[g_file_count].name) - 1] = '\0';
                
                // Remove extension for display
                char *dot = strrchr(g_file_entries[g_file_count].name, '.');
                if(dot) *dot = '\0';

                // Store index for sorting (will be reversed)
                g_file_entries[g_file_count].timestamp = total_count - g_file_count;
                g_file_count++;
            }
        }
        storage_dir_close(dir);
    }

    // Reverse array so newest files (last enumerated) appear first
    for(uint32_t i = 0; i < g_file_count / 2; i++) {
        FileEntry temp = g_file_entries[i];
        g_file_entries[i] = g_file_entries[g_file_count - 1 - i];
        g_file_entries[g_file_count - 1 - i] = temp;
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    g_file_list_valid = true;
    FURI_LOG_I(TAG, "Built file list with %lu entries", (unsigned long)g_file_count);
}

// Get count of saved capture files
uint32_t protopirate_storage_get_file_count(void)
{
    // Rebuild list if cache is invalid
    if(!g_file_list_valid) protopirate_storage_build_file_list();
    return g_file_count;
}

// Invalidate the file list cache (will rebuild on next access)
void protopirate_storage_invalidate_cache(void)
{
    g_file_list_valid = false;
}

// Get file info by index
bool protopirate_storage_get_file_by_index(uint32_t index, FuriString *out_path, FuriString *out_name)
{
    // Rebuild list if cache is invalid
    if(!g_file_list_valid) protopirate_storage_build_file_list();
    
    if(!g_file_entries || index >= g_file_count) return false;

    if(out_path)
        furi_string_printf(out_path, "%s/%s%s", PROTOPIRATE_APP_FOLDER, g_file_entries[index].name, PROTOPIRATE_APP_EXTENSION);
    if(out_name)
        furi_string_set_str(out_name, g_file_entries[index].name);
    return true;
}

// Delete a capture file
bool protopirate_storage_delete_file(const char *file_path)
{
    if(!file_path) return false;
    
    Storage *storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);
    
    // Invalidate cache since we deleted a file
    if(result) g_file_list_valid = false;
    
    FURI_LOG_I(TAG, "Delete file %s: %s", file_path, result ? "OK" : "FAILED");
    return result;
}

// Load a capture file
// IMPORTANT: Caller MUST call protopirate_storage_close_file() when done!
FlipperFormat *protopirate_storage_load_file(const char *file_path)
{
    if(!file_path) return NULL;
    
    Storage *storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat *ff = flipper_format_file_alloc(storage);

    if(!flipper_format_file_open_existing(ff, file_path)) {
        FURI_LOG_E(TAG, "Failed to open %s", file_path);
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    
    // Note: Storage record stays open until protopirate_storage_close_file() is called
    return ff;
}

// Close a loaded file and release storage resources
// Must be called after protopirate_storage_load_file()
void protopirate_storage_close_file(FlipperFormat *flipper_format)
{
    if(flipper_format) flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);
}

// Check if a file exists
bool protopirate_storage_file_exists(const char *file_path)
{
    if(!file_path) return false;
    
    Storage *storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_file_exists(storage, file_path);
    furi_record_close(RECORD_STORAGE);
    
    return exists;
}

// Free the internal file list cache
// Call this when exiting the app or when you need to refresh the list
void protopirate_storage_free_file_list(void)
{
    if(g_file_entries) {
        free(g_file_entries);
        g_file_entries = NULL;
    }
    g_file_count = 0;
    g_file_list_valid = false;
}