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
#define BRUTE_THREAD_STACK_SIZE 8192
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
    Submenu* key_list_submenu;  // Separate submenu for key list
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
    
    // Brute force thread
    FuriThread* brute_thread;
    FuriTimer* brute_timer;  // Timer to update GUI from main thread
    FuriMutex* thread_mutex;  // Mutex to protect thread access
    bool thread_stopping;  // Flag to prevent double-cleanup
} UidBruteSmarter;

// Forward declarations
static void uid_brute_smarter_setup_menu(UidBruteSmarter* instance);
static void uid_brute_smarter_setup_config(UidBruteSmarter* instance);
static void uid_brute_smarter_start_brute_force(UidBruteSmarter* instance);
static bool uid_brute_smarter_load_nfc_file(UidBruteSmarter* instance, const char* path);
static void uid_brute_smarter_show_key_list(UidBruteSmarter* instance);
static uint32_t uid_brute_smarter_key_list_back_callback(void* context);
static uint32_t uid_brute_smarter_brute_back_callback(void* context);
static void uid_brute_smarter_timer_callback(void* context);

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
    
    // Stop and free timer
    if(instance->brute_timer) {
        furi_timer_stop(instance->brute_timer);
        furi_timer_free(instance->brute_timer);
    }
    
    // Stop and free brute force thread if running
    if(instance->brute_thread) {
        instance->thread_stopping = true;
        if(instance->app) {
            instance->app->should_stop = true;
        }
        furi_mutex_acquire(instance->thread_mutex, FuriWaitForever);
        furi_thread_join(instance->brute_thread);
        furi_thread_free(instance->brute_thread);
        furi_mutex_release(instance->thread_mutex);
    }
    
    // Free mutex
    if(instance->thread_mutex) {
        furi_mutex_free(instance->thread_mutex);
    }
    
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
    submenu_free(instance->key_list_submenu);  // Free the key list submenu
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
    instance->app->current_uid = 0;
    instance->app->is_running = false;
    
    // Initialize key storage
    instance->loaded_keys_count = 0;
    for(int i = 0; i < MAX_UIDS; i++) {
        instance->loaded_keys[i].uid = 0;
        instance->loaded_keys[i].name = NULL;
        instance->loaded_keys[i].file_path = NULL;
        instance->loaded_keys[i].loaded_time = 0;
        instance->loaded_keys[i].is_active = false;
    }
    
    // Initialize thread pointer and synchronization
    instance->brute_thread = NULL;
    instance->thread_stopping = false;
    instance->thread_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    // Initialize timer for GUI updates (100ms interval)
    instance->brute_timer = furi_timer_alloc(uid_brute_smarter_timer_callback, FuriTimerTypePeriodic, instance);
    
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
    instance->key_list_submenu = submenu_alloc();  // Allocate separate submenu for key list
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
        instance->view_dispatcher, UidBruteSmarterViewKeyList, submenu_get_view(instance->key_list_submenu));
    
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
    
    DEBUG_LOG("[KEY_LIST] Callback triggered with index: %lu", index);
    
    if(index == 255) {
        // Unload All
        DEBUG_LOG("[KEY_LIST] Unload All selected");
        for(int i = instance->loaded_keys_count - 1; i >= 0; i--) {
            uid_brute_smarter_remove_key(instance, i);
        }
        // Return to main menu after unloading all
        uid_brute_smarter_setup_menu(instance);
        view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    } else if(index < instance->loaded_keys_count) {
        // Remove specific key
        DEBUG_LOG("[KEY_LIST] Removing key at index: %lu", index);
        uid_brute_smarter_remove_key(instance, index);
        uid_brute_smarter_show_key_list(instance);
    }
}

static void uid_brute_smarter_show_key_list(UidBruteSmarter* instance) {
    DEBUG_LOG("[KEY_LIST] Showing key list with %d keys", instance->loaded_keys_count);
    
    // Use the dedicated key_list_submenu
    submenu_reset(instance->key_list_submenu);
    char header_text[32];
    snprintf(header_text, sizeof(header_text), "Loaded Keys (%d)", instance->loaded_keys_count);
    submenu_set_header(instance->key_list_submenu, header_text);
    
    for(int i = 0; i < instance->loaded_keys_count; i++) {
        char item_text[64];
        snprintf(item_text, sizeof(item_text), "%s (%08lX)", 
                 instance->loaded_keys[i].name, 
                 instance->loaded_keys[i].uid);
        submenu_add_item(instance->key_list_submenu, item_text, i, uid_brute_smarter_key_list_callback, instance);
        DEBUG_LOG("[KEY_LIST] Added key item: %s", item_text);
    }
    
    // Add action buttons
    if(instance->loaded_keys_count > 0) {
        submenu_add_item(instance->key_list_submenu, "Unload All", 255, uid_brute_smarter_key_list_callback, instance);
        DEBUG_LOG("[KEY_LIST] Added 'Unload All' button");
    }
    
    // Set proper back navigation to return to main menu
    view_set_previous_callback(submenu_get_view(instance->key_list_submenu), uid_brute_smarter_key_list_back_callback);
    
    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewKeyList);
    DEBUG_LOG("[KEY_LIST] Switched to key list view");
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
            DEBUG_LOG("[MENU] Start Brute Force selected");
            DEBUG_LOG("[MENU] UID count: %d", instance->app ? instance->app->uid_count : -1);
            if(instance->app && instance->app->uid_count > 0) {
                DEBUG_LOG("[MENU] Calling uid_brute_smarter_start_brute_force...");
                uid_brute_smarter_start_brute_force(instance);
                DEBUG_LOG("[MENU] Returned from uid_brute_smarter_start_brute_force");
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

static void uid_brute_smarter_timer_callback(void* context) {
    DEBUG_LOG("[TIMER] === TIMER CALLBACK FIRED ===");
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    DEBUG_LOG("[TIMER] Instance: %p, App: %p", instance, instance ? instance->app : NULL);
    // Safety check
    if(!instance || !instance->app) {
        DEBUG_LOG("[TIMER] NULL instance or app, returning");
        return;
    }
    
    // Check if we're already stopping
    if(instance->thread_stopping) {
        DEBUG_LOG("[TIMER] Thread stopping, skipping");
        return;
    }
    
    DEBUG_LOG("[TIMER] Acquiring mutex...");
    // Acquire mutex for thread access (wait up to 10ms to avoid blocking too long)
    if(furi_mutex_acquire(instance->thread_mutex, 10) != FuriStatusOk) {
        // Mutex is busy, skip this timer tick
        DEBUG_LOG("[TIMER] Mutex busy, skipping tick");
        return;
    }
    
    DEBUG_LOG("[TIMER] Thread ptr: %p", instance->brute_thread);
    if(!instance->brute_thread) {
        // Thread was cleaned up, stop timer
        DEBUG_LOG("[TIMER] No thread, stopping timer");
        furi_timer_stop(instance->brute_timer);
        furi_mutex_release(instance->thread_mutex);
        return;
    }
    
    FuriThreadState state = furi_thread_get_state(instance->brute_thread);
    DEBUG_LOG("[TIMER] Thread state: %d", state);
    
    // Check if thread is still starting
    if(state == FuriThreadStateStarting) {
        DEBUG_LOG("[TIMER] Thread still starting, skipping update");
        furi_mutex_release(instance->thread_mutex);
        return;
    }
    
    if(state == FuriThreadStateStopped) {
        // Thread finished, show completion message
        DEBUG_LOG("[TIMER] Brute force thread finished");
        furi_timer_stop(instance->brute_timer);
        
        // Show completion message
        if(!instance->app->should_stop) {
            popup_set_header(instance->popup, "Complete", 64, 20, AlignCenter, AlignTop);
            popup_set_text(instance->popup, "Brute force completed", 64, 40, AlignCenter, AlignTop);
        } else {
            popup_set_header(instance->popup, "Stopped", 64, 20, AlignCenter, AlignTop);
            popup_set_text(instance->popup, "Brute force stopped", 64, 40, AlignCenter, AlignTop);
        }
        popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
        popup_enable_timeout(instance->popup);
        
        // Mark thread for cleanup but don't free it here - let back callback do it
        instance->thread_stopping = true;
    } else if(instance->app && instance->app->is_running) {
        // Update progress display
        char progress_text[64];
        snprintf(progress_text, sizeof(progress_text), "UID: %08lX\nRunning...", 
                 instance->app->current_uid);
        DEBUG_LOG("[TIMER] Updating display with UID: %08lX", instance->app->current_uid);
        popup_set_text(instance->popup, progress_text, 64, 30, AlignCenter, AlignCenter);
    } else {
        DEBUG_LOG("[TIMER] Not updating: is_running=%d", instance->app ? instance->app->is_running : -1);
    }
    
    furi_mutex_release(instance->thread_mutex);
    DEBUG_LOG("[TIMER] === TIMER CALLBACK COMPLETE ===");
}

static int32_t uid_brute_smarter_brute_worker(void* context) {
    // VERY FIRST THING - log that we entered the function
    DEBUG_LOG("[BRUTE_WORKER] === WORKER FUNCTION ENTERED ===");
    
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    if(!instance || !instance->app) {
        ERROR_LOG("[BRUTE_WORKER] ERROR: Invalid context");
        return -1;
    }
    
    // Verify NFC resources are available from main instance
    if(!instance->nfc || !instance->nfc_device) {
        ERROR_LOG("[BRUTE_WORKER] ERROR: NFC resources not initialized");
        return -1;
    }
    
    DEBUG_LOG("[BRUTE_WORKER] Starting brute force worker with %d UIDs", instance->app->uid_count);
    for(int i = 0; i < instance->app->uid_count; i++) {
        DEBUG_LOG("[BRUTE_WORKER] UID[%d] = %08lX", i, instance->app->uids[i]);
    }
    
    PatternResult result;
    DEBUG_LOG("[BRUTE_WORKER] Calling pattern_engine_detect...");
    if(!pattern_engine_detect(instance->app->uids, instance->app->uid_count, &result)) {
        ERROR_LOG("Pattern detection failed");
        // DO NOT update GUI from worker thread!
        return -1;
    }
    DEBUG_LOG("[BRUTE_WORKER] Pattern detected: type=%d, start=%08lX, end=%08lX, step=%lu, range_size=%lu", 
              result.type, result.start_uid, result.end_uid, result.step, result.range_size);
    
    // Don't set is_running here - it's already set in main thread
    // instance->app->is_running = true;  // REMOVED - set in main thread
    // instance->app->should_stop = false;  // Already set in main thread
    
    uint32_t range[MAX_RANGE_SIZE];
    uint16_t actual_size;
    
    DEBUG_LOG("[BRUTE_WORKER] Calling pattern_engine_build_range...");
    if(!pattern_engine_build_range(&result, range, MAX_RANGE_SIZE, &actual_size)) {
        ERROR_LOG("Failed to build valid range for brute force");
        // DO NOT update GUI from worker thread!
        return -1;
    }
    DEBUG_LOG("[BRUTE_WORKER] Range built successfully: actual_size=%d", actual_size);
    if(actual_size > 0) {
        DEBUG_LOG("[BRUTE_WORKER] First UID: %08lX, Last UID: %08lX", range[0], range[actual_size-1]);
    }
    
    // DO NOT update GUI from worker thread!
    
    // Allocate ISO14443-3A data using protocol API (safer than raw malloc)
    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    if(!iso_data) {
        ERROR_LOG("[BRUTE_WORKER] Failed to allocate memory for ISO data");
        return -1;
    }
    
    DEBUG_LOG("[BRUTE_WORKER] Starting brute force loop with %d UIDs", actual_size);
    
    // Track consecutive failures for emergency abort
    int consecutive_failures = 0;
    const int MAX_CONSECUTIVE_FAILURES = 5;
    
    for(uint16_t i = 0; i < actual_size && !instance->app->should_stop; i++) {
        DEBUG_LOG("[BRUTE_WORKER] Processing UID %d/%d: %08lX", i + 1, actual_size, range[i]);
        
        // DO NOT update GUI from worker thread - it causes bus faults!
        // Update current UID early so timer can display it
        instance->app->current_uid = range[i];
        
        // Safety check: ensure we don't run forever
        if(i >= MAX_RANGE_SIZE - 1) {
            ERROR_LOG("Safety check triggered: stopping brute force at MAX_RANGE_SIZE");
            break;
        }
        
        // Create and configure NFC device with the UID
        DEBUG_LOG("[BRUTE_WORKER] Creating ISO14443-3a data for UID %08lX", range[i]);
        
        // Reset and set fields via protocol API to ensure validity
        iso14443_3a_reset(iso_data);
        const uint8_t uid_bytes[4] = {
            (uint8_t)((range[i] >> 24) & 0xFF),
            (uint8_t)((range[i] >> 16) & 0xFF),
            (uint8_t)((range[i] >> 8) & 0xFF),
            (uint8_t)(range[i] & 0xFF),
        };
        iso14443_3a_set_uid(iso_data, uid_bytes, STANDARD_UID_LENGTH);
        const uint8_t atqa_bytes[2] = {ATQA_DEFAULT_LOW, ATQA_DEFAULT_HIGH};
        iso14443_3a_set_atqa(iso_data, atqa_bytes);
        iso14443_3a_set_sak(iso_data, SAK_DEFAULT);
        
        DEBUG_LOG("[BRUTE_WORKER] UID bytes: %02X %02X %02X %02X", 
                  uid_bytes[0], uid_bytes[1], uid_bytes[2], uid_bytes[3]);
        
        // Validate NFC device before setting data
        if(!instance->nfc_device) {
            ERROR_LOG("[BRUTE_WORKER] NFC device is NULL");
            iso14443_3a_free(iso_data);
            return -1;
        }
        
        // Set the data in NFC device
        DEBUG_LOG("[BRUTE_WORKER] Setting NFC device data...");
        nfc_device_clear(instance->nfc_device);
        
        // Set data (returns void, no error checking possible)
        nfc_device_set_data(instance->nfc_device, NfcProtocolIso14443_3a, iso_data);
        
        // Start NFC emulation - simplified approach
        DEBUG_LOG("[BRUTE_WORKER] Starting NFC emulation for UID %08lX...", range[i]);
        
        // Try to start emulation with error handling and retry logic
        NfcListener* listener = NULL;
        bool emulation_success = false;
        int retry_count = 0;
        const int MAX_RETRIES = 3;
        
        while(retry_count < MAX_RETRIES && !emulation_success) {
            retry_count++;
            
            DEBUG_LOG("[BRUTE_WORKER] Emulation attempt %d/%d for UID %08lX", retry_count, MAX_RETRIES, range[i]);
            
            // Small delay between retries to let NFC hardware settle
            if(retry_count > 1) {
                DEBUG_LOG("[BRUTE_WORKER] Waiting 100ms before retry...");
                furi_delay_ms(100);
                
                // Clear NFC device on retry to reset state
                nfc_device_clear(instance->nfc_device);
                
                // Re-set the UID data
                nfc_device_set_data(instance->nfc_device, NfcProtocolIso14443_3a, iso_data);
            }
            
            do {
                DEBUG_LOG("[BRUTE_WORKER] About to call nfc_listener_alloc...");
                DEBUG_LOG("[BRUTE_WORKER] Instance NFC ptr: %p, Protocol: %d", 
                          instance->nfc, NfcProtocolIso14443_3a);
                
                // Verify NFC handle is valid before allocation
                if(!instance->nfc) {
                    ERROR_LOG("[BRUTE_WORKER] NFC handle is NULL");
                    break;
                }
                
                // Use data from NfcDevice to ensure correct internal layout
                const Iso14443_3aData* device_data =
                    nfc_device_get_data(instance->nfc_device, NfcProtocolIso14443_3a);
                
                // Allocate listener with correct API (3 parameters)
                DEBUG_LOG("[BRUTE_WORKER] Calling nfc_listener_alloc with device data: %p...", device_data);
                listener = nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_3a, device_data);
                if(!listener) {
                    ERROR_LOG("[BRUTE_WORKER] Failed to allocate NFC listener (attempt %d/%d)", retry_count, MAX_RETRIES);
                    break;
                }
                
                DEBUG_LOG("[BRUTE_WORKER] NFC listener allocated: %p, starting emulation...", listener);
                
                // Start listener with a timeout check
                DEBUG_LOG("[BRUTE_WORKER] Calling nfc_listener_start...");
                
                // Start the listener (this should return quickly)
                nfc_listener_start(listener, NULL, NULL);
                
                // If we got here, listener started successfully
                DEBUG_LOG("[BRUTE_WORKER] nfc_listener_start returned successfully");
            
                DEBUG_LOG("[BRUTE_WORKER] NFC emulation started, waiting %lu ms...", instance->app->delay_ms);
                
                // Update current UID for display AFTER starting emulation
                instance->app->current_uid = range[i];
                
                // Emulate for the specified delay with periodic checks
                uint32_t elapsed = 0;
                const uint32_t check_interval = 50; // Check every 50ms
                while(elapsed < instance->app->delay_ms && !instance->app->should_stop) {
                    furi_delay_ms(check_interval);
                    elapsed += check_interval;
                }
                
                DEBUG_LOG("[BRUTE_WORKER] Delay complete for UID %08lX", range[i]);
                
                emulation_success = true;
                DEBUG_LOG("[BRUTE_WORKER] NFC emulation completed successfully");
            } while(0);
            
            // If emulation succeeded, break out of retry loop
            if(emulation_success) {
                break;
            }
        }
        
        // Cleanup with proper timing
        if(listener) {
            DEBUG_LOG("[BRUTE_WORKER] Stopping NFC listener...");
            nfc_listener_stop(listener);
            
            // Small delay to ensure listener fully stops
            furi_delay_ms(10);
            
            DEBUG_LOG("[BRUTE_WORKER] Freeing NFC listener...");
            nfc_listener_free(listener);
            listener = NULL;
            
            // Another small delay to let NFC hardware settle
            furi_delay_ms(10);
        }
        
        if(!emulation_success) {
            ERROR_LOG("[BRUTE_WORKER] NFC emulation failed for UID %08lX after %d attempts", range[i], retry_count);
            
            // Increment failure counter
            consecutive_failures++;
            
            if(consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                ERROR_LOG("[BRUTE_WORKER] Too many consecutive failures (%d), aborting", consecutive_failures);
                instance->app->should_stop = true;
                break;
            }
            
            // Try to reset NFC state for next UID
            DEBUG_LOG("[BRUTE_WORKER] Attempting NFC recovery...");
            nfc_device_clear(instance->nfc_device);
            furi_delay_ms(200); // Longer delay for recovery
            
            // Continue with next UID instead of crashing
        } else {
            // Reset failure counter on success
            consecutive_failures = 0;
        }
        
        // Handle pause
        if(instance->app->pause_every > 0 && (i + 1) % instance->app->pause_every == 0) {
            // Just pause, don't update GUI from worker thread
            furi_delay_ms(instance->app->pause_duration_s * 1000);
        }
        
        // Small delay to prevent overwhelming the system
        furi_delay_ms(10);
    }
    
    instance->app->is_running = false;
    
    DEBUG_LOG("[BRUTE_WORKER] Brute force finished, was_stopped=%d", instance->app->should_stop);
    
    // Free allocated memory
    iso14443_3a_free(iso_data);
    
    // Clear the NFC device for cleanup
    nfc_device_clear(instance->nfc_device);
    
    // DO NOT touch GUI from worker thread! This causes bus faults!
    // The main thread will handle returning to menu
    
    return 0;
}

static void uid_brute_smarter_start_brute_force(UidBruteSmarter* instance) {
    DEBUG_LOG("[START_BRUTE] === ENTERING START_BRUTE_FORCE ===");
    DEBUG_LOG("[START_BRUTE] Instance ptr: %p", instance);
    
    // Safety checks
    if(!instance) {
        ERROR_LOG("[START_BRUTE] ERROR: instance is NULL");
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] App ptr: %p", instance->app);
    if(!instance->app) {
        ERROR_LOG("[START_BRUTE] ERROR: instance->app is NULL");
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Loaded keys: %d, App UIDs: %d", instance->loaded_keys_count, instance->app->uid_count);
    
    DEBUG_LOG("[START_BRUTE] Checking NFC: nfc=%p, nfc_device=%p", instance->nfc, instance->nfc_device);
    if(!instance->nfc || !instance->nfc_device) {
        ERROR_LOG("[START_BRUTE] ERROR: NFC not initialized");
        popup_set_header(instance->popup, "Error", 64, 20, AlignCenter, AlignTop);
        popup_set_text(instance->popup, "NFC not initialized", 64, 40, AlignCenter, AlignTop);
        popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
        popup_enable_timeout(instance->popup);
        view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
        return;
    }
    
    // Reset NFC device to ensure clean state
    DEBUG_LOG("[START_BRUTE] Resetting NFC device for clean state...");
    nfc_device_clear(instance->nfc_device);
    furi_delay_ms(50); // Small delay to ensure NFC hardware is ready
    
    DEBUG_LOG("[START_BRUTE] Resetting flags...");
    // Reset stop flags and current UID
    instance->app->should_stop = false;
    instance->app->is_running = true;  // Set to true BEFORE starting worker thread
    instance->app->current_uid = 0;
    instance->thread_stopping = false;
    
    DEBUG_LOG("[START_BRUTE] Checking mutex: %p", instance->thread_mutex);
    // Ensure mutex is available
    if(!instance->thread_mutex) {
        ERROR_LOG("[START_BRUTE] ERROR: thread_mutex is NULL");
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Checking timer: %p", instance->brute_timer);
    // Ensure timer is available
    if(!instance->brute_timer) {
        ERROR_LOG("[START_BRUTE] ERROR: brute_timer is NULL");
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Checking popup: %p", instance->popup);
    // Ensure popup is available
    if(!instance->popup) {
        ERROR_LOG("[START_BRUTE] ERROR: popup is NULL");
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Acquiring mutex for thread creation...");
    // Acquire mutex before creating thread to prevent race conditions
    furi_mutex_acquire(instance->thread_mutex, FuriWaitForever);
    
    DEBUG_LOG("[START_BRUTE] Setting up popup UI...");
    // Show brute force screen
    popup_set_header(instance->popup, "Brute Force", 64, 10, AlignCenter, AlignTop);
    popup_set_text(instance->popup, "Starting...", 64, 30, AlignCenter, AlignCenter);
    
    DEBUG_LOG("[START_BRUTE] Getting popup view...");
    // Set back callback for brute force view with context
    View* popup_view = popup_get_view(instance->popup);
    DEBUG_LOG("[START_BRUTE] Popup view ptr: %p", popup_view);
    if(!popup_view) {
        ERROR_LOG("[START_BRUTE] ERROR: popup_view is NULL");
        furi_mutex_release(instance->thread_mutex);
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Setting view context and callback...");
    view_set_context(popup_view, instance);
    view_set_previous_callback(popup_view, uid_brute_smarter_brute_back_callback);
    
    DEBUG_LOG("[START_BRUTE] Switching to brute force view...");
    view_dispatcher_switch_to_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
    
    DEBUG_LOG("[START_BRUTE] Allocating thread...");
    // Create thread BEFORE starting timer to avoid race condition
    instance->brute_thread = furi_thread_alloc();
    if(!instance->brute_thread) {
        ERROR_LOG("[START_BRUTE] ERROR: Failed to allocate thread");
        furi_mutex_release(instance->thread_mutex);
        popup_set_header(instance->popup, "Error", 64, 20, AlignCenter, AlignTop);
        popup_set_text(instance->popup, "Thread allocation failed", 64, 40, AlignCenter, AlignTop);
        popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
        popup_enable_timeout(instance->popup);
        return;
    }
    
    DEBUG_LOG("[START_BRUTE] Thread allocated: %p", instance->brute_thread);
    DEBUG_LOG("[START_BRUTE] Setting thread properties...");
    furi_thread_set_name(instance->brute_thread, "BruteWorker");
    furi_thread_set_stack_size(instance->brute_thread, BRUTE_THREAD_STACK_SIZE);
    furi_thread_set_context(instance->brute_thread, instance);
    furi_thread_set_callback(instance->brute_thread, uid_brute_smarter_brute_worker);
    
    DEBUG_LOG("[START_BRUTE] Starting timer (100ms interval)...");
    // Start timer to update GUI from main thread BEFORE starting thread
    furi_timer_start(instance->brute_timer, 100);
    DEBUG_LOG("[START_BRUTE] Timer started, result should be FuriStatusOk");
    
    DEBUG_LOG("[START_BRUTE] Starting thread execution...");
    // Start the thread - it will run in background
    furi_thread_start(instance->brute_thread);
    
    // Release mutex now that thread is created and timer is running
    furi_mutex_release(instance->thread_mutex);
    
    DEBUG_LOG("[START_BRUTE] === EXITING START_BRUTE_FORCE SUCCESS ===");
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

static uint32_t uid_brute_smarter_config_back_callback(void* context) {
    UNUSED(context);
    
    DEBUG_LOG("[CONFIG] Back button pressed, returning to main menu");
    return UidBruteSmarterViewMenu;
}

static uint32_t uid_brute_smarter_key_list_back_callback(void* context) {
    UNUSED(context);
    
    DEBUG_LOG("[KEY_LIST] Back button pressed, returning to main menu");
    
    // Simply return to the main menu view - no need to rebuild since it's a separate submenu
    return UidBruteSmarterViewMenu;
}

static uint32_t uid_brute_smarter_brute_back_callback(void* context) {
    DEBUG_LOG("[BACK] === BACK CALLBACK TRIGGERED ===");
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    DEBUG_LOG("[BACK] Instance: %p", instance);
    
    if(!instance) {
        ERROR_LOG("[BACK] NULL instance in back callback!");
        return UidBruteSmarterViewMenu;
    }
    
    DEBUG_LOG("[BACK] Stopping brute force...");
    
    // Set stopping flag first to prevent race conditions
    instance->thread_stopping = true;
    
    // Stop the timer
    if(instance->brute_timer) {
        furi_timer_stop(instance->brute_timer);
    }
    
    // Signal the brute force thread to stop
    if(instance->app) {
        instance->app->should_stop = true;
        instance->app->is_running = false; // Ensure running flag is cleared
    }
    
    // Acquire mutex for thread cleanup
    furi_mutex_acquire(instance->thread_mutex, FuriWaitForever);
    
    // If thread exists, clean it up
    if(instance->brute_thread) {
        FuriThreadState state = furi_thread_get_state(instance->brute_thread);
        if(state == FuriThreadStateRunning) {
            DEBUG_LOG("[BRUTE] Waiting for thread to stop...");
            // Release mutex while waiting for thread to avoid deadlock
            furi_mutex_release(instance->thread_mutex);
            furi_thread_join(instance->brute_thread);
            furi_mutex_acquire(instance->thread_mutex, FuriWaitForever);
        }
        
        // Now safe to free the thread
        furi_thread_free(instance->brute_thread);
        instance->brute_thread = NULL;
    }
    
    // Reset NFC device state after stopping brute force
    if(instance->nfc_device) {
        DEBUG_LOG("[BACK] Clearing NFC device state...");
        nfc_device_clear(instance->nfc_device);
    }
    
    // Reset the stopping flag
    instance->thread_stopping = false;
    
    furi_mutex_release(instance->thread_mutex);
    
    // Return to main menu
    return UidBruteSmarterViewMenu;
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
    
    // Set previous callback to handle back button
    view_set_previous_callback(
        variable_item_list_get_view(instance->variable_item_list), 
        uid_brute_smarter_config_back_callback);
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

// Removed - not needed with proper view handling

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