#pragma once

#include <furi.h> // For FuriString, uint32_t, bool, FuriHalResources (like InputKey potentially if used directly) etc.
#include <gui/view_dispatcher.h> // For ViewDispatcher
#include <gui/modules/submenu.h> // For Submenu
#include <gui/modules/file_browser.h> // For FileBrowser type

// Forward declarations for Flipper system types used as pointers in KeyloggerApp
typedef struct Gui Gui;
typedef struct Storage Storage;
typedef struct DialogsApp DialogsApp;

// App-specific includes (ensure these are self-contained or use forward declarations for types they expose)
#include "keystroke_exfil.h" // Defines KeystrokeExfilContext
#include "payload_manager.h" // Defines PayloadManager

// Forward declare our own view module types (used as pointers in KeyloggerApp)
typedef struct ProgressView ProgressView;
typedef struct SuccessFailureView SuccessFailureView;
typedef struct HelpView HelpView;
typedef struct FileViewerView FileViewerView;
typedef struct KeylogBrowserView KeylogBrowserView;

// Define views for the application (enum)
typedef enum {
    KeyloggerViewPreparation,
    KeyloggerViewProgress,
    KeyloggerViewSuccess,
    KeyloggerViewFailure,
    KeyloggerViewHelp,
    KeyloggerViewFileViewer, // Consolidated File Viewer View ID
    KeyloggerViewFileBrowser,
    KeyloggerViewKeylogBrowser, // Our new custom browser
} KeyloggerView;

// Custom events for the application
typedef enum {
    KeyloggerEventSuccessView = 0, // Custom event to switch to success view
    KeyloggerEventFailureView, // Custom event to switch to failure view
    // Add other custom events if needed
} KeyloggerCustomEvent;

// Forward declarations for types used in the app

// Application context structure
typedef struct KeyloggerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Storage* storage;
    DialogsApp* dialogs;
    FileBrowser* file_browser;

    // Views
    Submenu* preparation_submenu;
    ProgressView* progress_view;
    SuccessFailureView* success_failure_view;
    HelpView* help_view;
    FileViewerView* file_viewer_view;
    KeylogBrowserView* keylog_browser_view;

    // Core Logic Modules
    PayloadManager* payload_manager;
    KeystrokeExfilContext* exfil_context;

    // Configuration
    FuriString* primary_payload_path;
    bool use_usb;
    FuriString* exfiltration_folder_path;
    uint32_t operation_mode; // 0=Deploy, 1=Retrieve, 2=Custom

    // State for file browser
    bool selecting_primary_payload;

    // State for file viewer deferred loading
    FuriString* selected_file_path;

    // State for execution
    uint32_t current_view_id;
    FuriThread* execution_thread; // Worker thread for payload execution

    // Execution results (set by worker thread, read by main thread)
    bool execution_completed;
    bool execution_success;
    bool execution_partial_success;
    bool execution_thread_completed; // Flag to track when worker thread has finished

} KeyloggerApp;
