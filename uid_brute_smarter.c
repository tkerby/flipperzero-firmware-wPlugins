/*
 * Copyright (c) 2024, UID Brute Smarter Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/file_browser.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/helpers/nfc_supported_cards.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/nfc_listener.h>

#include "pattern_engine.h"

#define TAG "UidBruteSmarter"

// Debug logging macros
#define DEBUG_LOG(fmt, ...) FURI_LOG_I(TAG, fmt, ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) FURI_LOG_E(TAG, fmt, ##__VA_ARGS__)
#define WARN_LOG(fmt, ...) FURI_LOG_W(TAG, fmt, ##__VA_ARGS__)

// Configuration constants
#define MAX_UIDS 5
#define MAX_RANGE_SIZE 1000
#define DEFAULT_DELAY_MS 500
#define DEFAULT_PAUSE_EVERY 0
#define DEFAULT_PAUSE_DURATION_S 3
#define NFC_APP_FOLDER "/ext/nfc"

// UI constants
#define POPUP_TIMEOUT_MS 2000
#define BRUTE_THREAD_STACK_SIZE 2048
#define MIN_DELAY_MS 100
#define MAX_DELAY_MS 1000
#define DELAY_STEP_MS 100
#define PAUSE_STEP 10
#define MAX_PAUSE_EVERY 100
#define MAX_PAUSE_DURATION_S 10

// NFC constants
#define STANDARD_UID_LENGTH 4
#define MAX_UID_LENGTH 10
#define MAX_FILE_SIZE (1024 * 1024) // 1MB
#define ATQA_DEFAULT_LOW 0x00
#define ATQA_DEFAULT_HIGH 0x04
#define SAK_DEFAULT 0x08

// Enhanced key storage structure
typedef struct {
    uint32_t uid;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

// Enhanced app structure with key management
typedef struct {
    uint32_t uids[MAX_UIDS];
    uint8_t uid_count;
    PatternType pattern;
    uint32_t start_uid;
    uint32_t end_uid;
    uint32_t step;
    uint32_t current_uid;
    uint32_t delay_ms;
    uint32_t pause_every;
    uint32_t pause_duration_s;
    bool is_running;
    bool should_stop;
} UidBruteSmartApp;

typedef enum {
    UidBruteSmarterViewSplash,
    UidBruteSmarterViewCardLoader,
    UidBruteSmarterViewPattern,
    UidBruteSmarterViewConfig,
    UidBruteSmarterViewBrute,
    UidBruteSmarterViewMenu,
    UidBruteSmarterViewKeyList
} UidBruteSmarterView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    DialogEx* dialog_ex;
    Popup* popup;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    FileBrowser* file_browser;
    Storage* storage;
    Nfc* nfc;
    NfcDevice* nfc_device;
    
    UidBruteSmartApp* app;
    KeyInfo loaded_keys[MAX_UIDS];
    uint8_t loaded_keys_count;
    char file_path[256];
} UidBruteSmarter;

// Forward declarations
static void uid_brute_smarter_setup_menu(UidBruteSmarter* instance);
static void uid_brute_smarter_setup_config(UidBruteSmarter* instance);
static void uid_brute_smarter_start_brute_force(UidBruteSmarter* instance);
static bool uid_brute_smarter_load_nfc_file(UidBruteSmarter* instance, const char* path);
static void uid_brute_smarter_show_key_list(UidBruteSmarter* instance);

static uint32_t uid_brute_smarter_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void uid_brute_smarter_splash_callback(DialogExResult result, void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    if(result == DialogExResultCenter) {
        view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    }
}

static void uid_brute_smarter_show_splash(UidBruteSmarter* instance) {
    dialog_ex_set_header(instance->dialog_ex, "UID Brute Smarter", 64, 3, AlignCenter, AlignTop);
    dialog_ex_set_text(
        instance->dialog_ex,
        "Enhanced UID brute-force tool\nwith key management",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_center_button_text(instance->dialog_ex, "Continue");
    dialog_ex_set_context(instance->dialog_ex, instance);
    dialog_ex_set_result_callback(instance->dialog_ex, uid_brute_smarter_splash_callback);
    
    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewSplash);
}

static void uid_brute_smarter_free(UidBruteSmarter* instance) {
    furi_assert(instance);
    
    // Free loaded key memory
    for(int i = 0; i < instance->loaded_keys_count; i++) {
        free(instance->loaded_keys[i].name);
        free(instance->loaded_keys[i].file_path);
    }
    
    // Remove views
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewSplash);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewCardLoader);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewConfig);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewKeyList);
    
    // Free GUI components
    submenu_free(instance->submenu);
    dialog_ex_free(instance->dialog_ex);
    popup_free(instance->popup);
    text_input_free(instance->text_input);
    variable_item_list_free(instance->variable_item_list);
    
    // Free NFC
    nfc_device_free(instance->nfc_device);
    nfc_free(instance->nfc);
    
    // Close records
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    
    // Free app data
    if(instance->app) {
        free(instance->app);
    }
    
    // Free view dispatcher
    view_dispatcher_free(instance->view_dispatcher);
    
    // Free instance
    free(instance);
}

static UidBruteSmarter* uid_brute_smarter_alloc(void) {
    UidBruteSmarter* instance = malloc(sizeof(UidBruteSmarter));
    
    // Allocate app data
    instance->app = malloc(sizeof(UidBruteSmartApp));
    memset(instance->app, 0, sizeof(UidBruteSmartApp));
    instance->app->delay_ms = DEFAULT_DELAY_MS;
    instance->app->pause_every = DEFAULT_PAUSE_EVERY;
    instance->app->pause_duration_s = DEFAULT_PAUSE_DURATION_S;
    
    // Initialize key storage
    instance->loaded_keys_count = 0;
    for(int i = 0; i < MAX_UIDS; i++) {
        instance->loaded_keys[i].uid = 0;
        instance->loaded_keys[i].name = NULL;
        instance->loaded_keys[i].file_path = NULL;
        instance->loaded_keys[i].loaded_time = 0;
        instance->loaded_keys[i].is_active = false;
    }
    
    // Open GUI record
    instance->gui = furi_record_open(RECORD_GUI);
    
    // Open Storage record
    instance->storage = furi_record_open(RECORD_STORAGE);
    
    // Initialize NFC
    instance->nfc = nfc_alloc();
    instance->nfc_device = nfc_device_alloc();
    
    // Initialize view dispatcher
    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);
    
    // Initialize views
    instance->submenu = submenu_alloc();
    instance->dialog_ex = dialog_ex_alloc();
    instance->popup = popup_alloc();
    instance->text_input = text_input_alloc();
    instance->variable_item_list = variable_item_list_alloc();
    
    // Add views to dispatcher
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewSplash, dialog_ex_get_view(instance->dialog_ex));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewMenu, submenu_get_view(instance->submenu));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewConfig, variable_item_list_get_view(instance->variable_item_list));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewBrute, popup_get_view(instance->popup));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewCardLoader, text_input_get_view(instance->text_input));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewKeyList, submenu_get_view(instance->submenu));
    
    return instance;
}

static void uid_brute_smarter_add_key(UidBruteSmarter* instance, const char* filename, uint32_t uid) {
    DEBUG_LOG("[ADD_KEY] Starting add_key with count: %d, uid: %08lX", instance->loaded_keys_count, uid);
    
    if(instance->loaded_keys_count >= MAX_UIDS) {
        DEBUG_LOG("[ADD_KEY] ERROR: Max keys reached (%d)", MAX_UIDS);
        return;
    }
    
    int index = instance->loaded_keys_count;
    DEBUG_LOG("[ADD_KEY] Adding to index: %d", index);
    
    // Validate inputs
    if(!filename) {
        DEBUG_LOG("[ADD_KEY] ERROR: NULL filename");
        return;
    }
    
    DEBUG_LOG("[ADD_KEY] Filename: %s", filename);
    
    instance->loaded_keys[index].uid = uid;
    instance->loaded_keys[index].loaded_time = furi_get_tick();
    instance->loaded_keys[index].is_active = true;
    
    // Store filename
    size_t filename_len = strlen(filename) + 1;
    DEBUG_LOG("[ADD_KEY] Allocating %zu bytes for filename", filename_len);
    instance->loaded_keys[index].file_path = malloc(filename_len);
    if(!instance->loaded_keys[index].file_path) {
        DEBUG_LOG("[ADD_KEY] ERROR: malloc failed for filename");
        return;
    }
    strcpy(instance->loaded_keys[index].file_path, filename);
    
    // Generate key name
    char key_name[32];
    snprintf(key_name, sizeof(key_name), "Key %d", index + 1);
    size_t name_len = strlen(key_name) + 1;
    DEBUG_LOG("[ADD_KEY] Allocating %zu bytes for key_name: %s", name_len, key_name);
    instance->loaded_keys[index].name = malloc(name_len);
    if(!instance->loaded_keys[index].name) {
        DEBUG_LOG("[ADD_KEY] ERROR: malloc failed for key_name");
        free(instance->loaded_keys[index].file_path);
        return;
    }
    strcpy(instance->loaded_keys[index].name, key_name);
    
    // Add to app uids
    if(instance->app->uid_count >= MAX_UIDS) {
        DEBUG_LOG("[ADD_KEY] ERROR: App UID array full");
        free(instance->loaded_keys[index].file_path);
        free(instance->loaded_keys[index].name);
        return;
    }
    instance->app->uids[instance->app->uid_count] = uid;
    instance->app->uid_count++;
    instance->loaded_keys_count++;
    DEBUG_LOG("[ADD_KEY] Successfully added key %d, total: %d", index, instance->loaded_keys_count);
}

static void uid_brute_smarter_remove_key(UidBruteSmarter* instance, uint8_t index) {
    if(index >= instance->loaded_keys_count) return;
    
    uint32_t removed_uid = instance->loaded_keys[index].uid;
    
    // Free memory
    free(instance->loaded_keys[index].name);
    free(instance->loaded_keys[index].file_path);
    
    // Shift remaining keys
    for(int i = index; i < instance->loaded_keys_count - 1; i++) {
        instance->loaded_keys[i] = instance->loaded_keys[i + 1];
    }
    
    // Update app uids
    for(int i = 0; i < instance->app->uid_count; i++) {
        if(instance->app->uids[i] == removed_uid) {
            for(int j = i; j < instance->app->uid_count - 1; j++) {
                instance->app->uids[j] = instance->app->uids[j + 1];
            }
            instance->app->uid_count--;
            break;
        }
    }
    
    instance->loaded_keys_count--;
}

static void uid_brute_smarter_key_list_callback(void* context, uint32_t index) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    if(index == 255) {
        // Unload All
        for(int i = instance->loaded_keys_count - 1; i >= 0; i--) {
            uid_brute_smarter_remove_key(instance, i);
        }
        uid_brute_smarter_setup_menu(instance);
        view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    } else if(index == 254) {
        // Back button
        uid_brute_smarter_setup_menu(instance);
        view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    } else if(index < instance->loaded_keys_count) {
        // Remove specific key
        uid_brute_smarter_remove_key(instance, index);
        uid_brute_smarter_show_key_list(instance);
    }
}

static void uid_brute_smarter_show_key_list(UidBruteSmarter* instance) {
    submenu_reset(instance->submenu);
    char header_text[32];
    snprintf(header_text, sizeof(header_text), "Loaded Keys (%d)", instance->loaded_keys_count);
    submenu_set_header(instance->submenu, header_text);
    
    for(int i = 0; i < instance->loaded_keys_count; i++) {
        char item_text[64];
        snprintf(item_text, sizeof(item_text), "%s (%08lX)", 
                 instance->loaded_keys[i].name, 
                 instance->loaded_keys[i].uid);
        submenu_add_item(instance->submenu, item_text, i, uid_brute_smarter_key_list_callback, instance);
    }
    
    submenu_add_item(instance->submenu, "Unload All", 255, uid_brute_smarter_key_list_callback, instance);
    submenu_add_item(instance->submenu, "Back", 254, uid_brute_smarter_key_list_callback, instance);
    
    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewKeyList);
}

static bool uid_brute_smarter_load_nfc_file(UidBruteSmarter* instance, const char* path) {
    DEBUG_LOG("Loading NFC file: %s", path);
    
    // Basic file validation
    if(strlen(path) == 0) {
        ERROR_LOG("Empty file path provided");
        return false;
    }
    
    if(!furi_string_end_with_str(furi_string_alloc_set(path), ".nfc")) {
        WARN_LOG("File does not have .nfc extension: %s", path);
        return false;
    }
    
    // Check if file exists and is accessible
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo file_info;
    FS_Error stat_result = storage_common_stat(storage, path, &file_info);
    bool file_exists = (stat_result == FSE_OK);
    furi_record_close(RECORD_STORAGE);
    
    if(!file_exists) {
        ERROR_LOG("File does not exist or is not accessible: %s", path);
        return false;
    }
    
    if(file_info.size == 0) {
        ERROR_LOG("Empty file: %s", path);
        return false;
    }
    
    if(file_info.size > MAX_FILE_SIZE) {
        ERROR_LOG("File too large: %s (%llu bytes)", path, (unsigned long long)file_info.size);
        return false;
    }
    
    // Create a temporary NFC device to avoid corrupting the main one
    NfcDevice* temp_device = nfc_device_alloc();
    if(!temp_device) {
        ERROR_LOG("Failed to allocate temporary NFC device");
        return false;
    }
    
    bool success = false;
    
    // Try to load with error handling
    FURI_LOG_D(TAG, "Attempting nfc_device_load...");
    if(!nfc_device_load(temp_device, path)) {
        ERROR_LOG("nfc_device_load failed for: %s", path);
        goto cleanup;
    }
    
    FURI_LOG_D(TAG, "Checking protocol data...");
    
    const Iso14443_3aData* iso_data = nfc_device_get_data(temp_device, NfcProtocolIso14443_3a);
    if(!iso_data) {
        ERROR_LOG("No ISO14443-3a data found in file: %s", path);
        goto cleanup;
    }
    
    // Validate the data structure
    if(iso_data->uid_len == 0 || iso_data->uid_len > MAX_UID_LENGTH) {
        ERROR_LOG("Invalid UID length: %d (must be 4-10 bytes)", iso_data->uid_len);
        goto cleanup;
    }
    
    if(instance->app->uid_count >= MAX_UIDS) {
        ERROR_LOG("Maximum UIDs reached (%d)", MAX_UIDS);
        goto cleanup;
    }
    
    DEBUG_LOG("ISO14443-3a data found, UID length: %d, ATQA: 0x%04X, SAK: 0x%02X", 
              iso_data->uid_len, iso_data->atqa[0] | (iso_data->atqa[1] << 8), iso_data->sak);
    
    if(iso_data->uid_len == 0 || iso_data->uid_len > STANDARD_UID_LENGTH) {
        ERROR_LOG("Invalid UID length: %d (expected 4 bytes)", iso_data->uid_len);
        goto cleanup;
    }
    
    // Validate UID bytes with bounds checking
    uint32_t uid = 0;
    uint8_t uid_len = MIN(iso_data->uid_len, STANDARD_UID_LENGTH);
    
    if(uid_len == 0) {
        ERROR_LOG("Empty UID in file: %s", path);
        goto cleanup;
    }
    
    DEBUG_LOG("UID bytes:");
    for(uint8_t i = 0; i < uid_len; i++) {
        DEBUG_LOG("  UID[%d] = 0x%02X", i, iso_data->uid[i]);
        uid |= ((uint32_t)iso_data->uid[i]) << (8 * (uid_len - 1 - i));
    }
    
    // Basic UID validation (not all zeros or all 0xFF)
    uint32_t all_zeros = 0x00000000;
    uint32_t all_ones = 0xFFFFFFFF;
    
    if(uid == all_zeros || uid == all_ones) {
        ERROR_LOG("Invalid UID value: 0x%08lX", uid);
        goto cleanup;
    }
    
    DEBUG_LOG("Extracted valid UID: 0x%08lX", uid);
    
    // Store the key with enhanced info
    uid_brute_smarter_add_key(instance, path, uid);
    success = true;

cleanup:
    nfc_device_free(temp_device);
    return success;
}

static void uid_brute_smarter_menu_callback(void* context, uint32_t index) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    DEBUG_LOG("Menu callback triggered with index: %lu", index);
    
    const char* menu_names[] = {"Load Cards", "View Keys", "Configure", "Start Brute Force"};
    if(index < 4) {
        DEBUG_LOG("Main menu item: %s", menu_names[index]);
    } else {
        DEBUG_LOG("Key list item: %lu", index);
    }
    switch(index) {
        case 0: // Load Cards
            {
                DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
                FuriString* file_path = furi_string_alloc();
                FuriString* nfc_folder = furi_string_alloc_set(NFC_APP_FOLDER);
                
                DialogsFileBrowserOptions options;
                dialog_file_browser_set_basic_options(&options, ".nfc", NULL);
                options.base_path = NFC_APP_FOLDER;
                
                bool success = dialog_file_browser_show(
                    dialogs, 
                    file_path, 
                    nfc_folder,
                    &options);
                
                if(success) {
                    const char* path = furi_string_get_cstr(file_path);
                    DEBUG_LOG("User selected NFC file: %s", path);
                    
                    if(uid_brute_smarter_load_nfc_file(instance, path)) {
                        popup_set_header(instance->popup, "Card Loaded", 64, 20, AlignCenter, AlignTop);
                        char progress_text[32];
                        snprintf(progress_text, sizeof(progress_text), "%d UIDs loaded", 
                                 instance->loaded_keys_count);
                        popup_set_text(instance->popup, progress_text, 64, 40, AlignCenter, AlignTop);
                    } else {
                        popup_set_header(instance->popup, "Error", 64, 20, AlignCenter, AlignTop);
                        popup_set_text(instance->popup, "Load failed!\nCheck .nfc format", 64, 40, AlignCenter, AlignTop);
                    }
                    popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
                    popup_enable_timeout(instance->popup);
                    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
                }
                
                furi_string_free(file_path);
                furi_string_free(nfc_folder);
                furi_record_close(RECORD_DIALOGS);
            }
            break;
        case 1: // View Keys
            uid_brute_smarter_show_key_list(instance);
            break;
        case 2: // Configure
            uid_brute_smarter_setup_config(instance);
            view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewConfig);
            break;
        case 3: // Start Brute Force
            if(instance->app->uid_count > 0) {
                uid_brute_smarter_start_brute_force(instance);
            } else {
                popup_set_header(instance->popup, "Error", 64, 20, AlignCenter, AlignTop);
                popup_set_text(instance->popup, "No cards loaded!\nLoad .nfc files first", 64, 40, AlignCenter, AlignTop);
                popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
                popup_enable_timeout(instance->popup);
                view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
            }
            break;
    }
}

static int32_t uid_brute_smarter_brute_worker(void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    PatternResult result;
    if(!pattern_engine_detect(instance->app->uids, instance->app->uid_count, &result)) {
        return -1;
    }
    
    instance->app->is_running = true;
    instance->app->should_stop = false;
    
    uint32_t range[MAX_RANGE_SIZE];
    uint16_t actual_size;
    
    if(!pattern_engine_build_range(&result, range, MAX_RANGE_SIZE, &actual_size)) {
        return -1;
    }
    
    popup_set_header(instance->popup, "Brute Force", 64, 10, AlignCenter, AlignTop);
    
    for(uint16_t i = 0; i < actual_size && !instance->app->should_stop; i++) {
        char progress_text[64];
        snprintf(progress_text, sizeof(progress_text), "UID: %08lX\nProgress: %d/%d", 
                 range[i], i + 1, actual_size);
        popup_set_text(instance->popup, progress_text, 64, 30, AlignCenter, AlignCenter);
        
        // Emulate the UID
        Iso14443_3aData iso_data = {
            .uid_len = STANDARD_UID_LENGTH,
            .atqa = {ATQA_DEFAULT_LOW, ATQA_DEFAULT_HIGH},
            .sak = SAK_DEFAULT
        };
        
        // Convert UID to bytes (big-endian to little-endian)
        iso_data.uid[0] = (range[i] >> 24) & 0xFF;
        iso_data.uid[1] = (range[i] >> 16) & 0xFF;
        iso_data.uid[2] = (range[i] >> 8) & 0xFF;
        iso_data.uid[3] = range[i] & 0xFF;
        
        // Set the data in NFC device
        nfc_device_set_data(instance->nfc_device, NfcProtocolIso14443_3a, &iso_data);
        
        // Start NFC emulation
        NfcListener* listener = nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_3a, &iso_data);
        if(listener) {
            nfc_listener_start(listener, NULL, NULL);
            
            // Emulate for the specified delay
            furi_delay_ms(instance->app->delay_ms);
            
            // Stop emulation
            nfc_listener_stop(listener);
            nfc_listener_free(listener);
        }
        
        // Handle pause
        if(instance->app->pause_every > 0 && (i + 1) % instance->app->pause_every == 0) {
            popup_set_text(instance->popup, "Pausing...", 64, 40, AlignCenter, AlignCenter);
            furi_delay_ms(instance->app->pause_duration_s * 1000);
        }
    }
    
    instance->app->is_running = false;
    
    if(!instance->app->should_stop) {
        popup_set_header(instance->popup, "Complete", 64, 20, AlignCenter, AlignTop);
        popup_set_text(instance->popup, "Brute force completed", 64, 40, AlignCenter, AlignTop);
        popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
        popup_enable_timeout(instance->popup);
    } else {
        popup_set_header(instance->popup, "Stopped", 64, 20, AlignCenter, AlignTop);
        popup_set_text(instance->popup, "Brute force stopped", 64, 40, AlignCenter, AlignTop);
        popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
        popup_enable_timeout(instance->popup);
    }
    
    return 0;
}

static void uid_brute_smarter_start_brute_force(UidBruteSmarter* instance) {
    // Reset stop flag
    instance->app->should_stop = false;
    
    // Show brute force screen
    popup_set_header(instance->popup, "Starting...", 64, 20, AlignCenter, AlignTop);
    popup_set_text(instance->popup, "Initializing brute force", 64, 40, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
    
    // Start brute force worker
    FuriThread* brute_thread = furi_thread_alloc();
    furi_thread_set_name(brute_thread, "BruteWorker");
    furi_thread_set_stack_size(brute_thread, BRUTE_THREAD_STACK_SIZE);
    furi_thread_set_context(brute_thread, instance);
    furi_thread_set_callback(brute_thread, uid_brute_smarter_brute_worker);
    furi_thread_start(brute_thread);
    
    // Wait for thread to complete or user to stop
    furi_thread_join(brute_thread);
    furi_thread_free(brute_thread);
}

static void uid_brute_smarter_config_delay_change(VariableItem* item) {
    UidBruteSmarter* instance = (UidBruteSmarter*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    // Map index to delay (MIN_DELAY_MS to MAX_DELAY_MS in steps)
    instance->app->delay_ms = MIN_DELAY_MS + (index * DELAY_STEP_MS);
    char delay_str[16];
    snprintf(delay_str, sizeof(delay_str), "%lu ms", instance->app->delay_ms);
    variable_item_set_current_value_text(item, delay_str);
}

static void uid_brute_smarter_config_pause_every_change(VariableItem* item) {
    UidBruteSmarter* instance = (UidBruteSmarter*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    instance->app->pause_every = index * PAUSE_STEP; // 0 to MAX_PAUSE_EVERY in steps
    char pause_str[16];
    snprintf(pause_str, sizeof(pause_str), "%lu", instance->app->pause_every);
    variable_item_set_current_value_text(item, pause_str);
}

static void uid_brute_smarter_config_pause_duration_change(VariableItem* item) {
    UidBruteSmarter* instance = (UidBruteSmarter*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    instance->app->pause_duration_s = MIN(index, MAX_PAUSE_DURATION_S); // 0 to MAX_PAUSE_DURATION_S seconds
    char duration_str[16];
    snprintf(duration_str, sizeof(duration_str), "%lu s", instance->app->pause_duration_s);
    variable_item_set_current_value_text(item, duration_str);
}

static void uid_brute_smarter_setup_config(UidBruteSmarter* instance) {
    variable_item_list_reset(instance->variable_item_list);
    
    VariableItem* item;
    char value_str[16];
    
    // Delay setting
    item = variable_item_list_add(
        instance->variable_item_list, "Delay", 10, uid_brute_smarter_config_delay_change, instance);
    uint8_t delay_index = (instance->app->delay_ms - MIN_DELAY_MS) / DELAY_STEP_MS;
    if(delay_index > 9) delay_index = (DEFAULT_DELAY_MS - MIN_DELAY_MS) / DELAY_STEP_MS; // Default index
    snprintf(value_str, sizeof(value_str), "%lu ms", instance->app->delay_ms);
    variable_item_set_current_value_index(item, delay_index);
    variable_item_set_current_value_text(item, value_str);
    
    // Pause every N attempts
    item = variable_item_list_add(
        instance->variable_item_list, "Pause Every", 11, uid_brute_smarter_config_pause_every_change, instance);
    uint8_t pause_index = instance->app->pause_every / PAUSE_STEP;
    snprintf(value_str, sizeof(value_str), "%lu", instance->app->pause_every);
    variable_item_set_current_value_index(item, pause_index);
    variable_item_set_current_value_text(item, value_str);
    
    // Pause duration
    item = variable_item_list_add(
        instance->variable_item_list, "Pause Duration", 11, uid_brute_smarter_config_pause_duration_change, instance);
    snprintf(value_str, sizeof(value_str), "%lu s", instance->app->pause_duration_s);
    variable_item_set_current_value_index(item, instance->app->pause_duration_s);
    variable_item_set_current_value_text(item, value_str);
    
    view_set_previous_callback(
        variable_item_list_get_view(instance->variable_item_list), 
        uid_brute_smarter_exit_callback);
}

static void uid_brute_smarter_setup_menu(UidBruteSmarter* instance) {
    submenu_reset(instance->submenu);
    submenu_set_header(instance->submenu, "UID Brute Smarter");
    submenu_add_item(instance->submenu, "Load Cards", 0, uid_brute_smarter_menu_callback, instance);
    submenu_add_item(instance->submenu, "View Keys", 1, uid_brute_smarter_menu_callback, instance);
    submenu_add_item(instance->submenu, "Configure", 2, uid_brute_smarter_menu_callback, instance);
    submenu_add_item(instance->submenu, "Start Brute Force", 3, uid_brute_smarter_menu_callback, instance);
    
    view_set_previous_callback(submenu_get_view(instance->submenu), uid_brute_smarter_exit_callback);
}

int32_t uid_brute_smarter_app(void* p) {
    UNUSED(p);
    
    UidBruteSmarter* instance = uid_brute_smarter_alloc();
    
    // Setup menu
    uid_brute_smarter_setup_menu(instance);
    
    // Show splash screen
    uid_brute_smarter_show_splash(instance);
    
    // Run view dispatcher
    view_dispatcher_run(instance->view_dispatcher);
    
    // Cleanup
    uid_brute_smarter_free(instance);
    
    return 0;
}