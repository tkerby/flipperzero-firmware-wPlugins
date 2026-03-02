#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

#define FPWN_MODULES_DIR   EXT_PATH("flipperpwn/modules")
#define FPWN_MAX_MODULES   64
#define FPWN_MAX_OPTIONS   8
#define FPWN_MAX_LINE_LEN  512
#define FPWN_OPT_NAME_LEN  32
#define FPWN_OPT_VALUE_LEN 128
#define FPWN_OPT_DESC_LEN  64
#define FPWN_NAME_LEN      64
#define FPWN_DESC_LEN      256
#define FPWN_PATH_LEN      256

/* =========================================================================
 * Enums
 * ========================================================================= */

typedef enum {
    FPwnViewMainMenu,
    FPwnViewCategoryMenu,
    FPwnViewModuleList,
    FPwnViewModuleInfo,
    FPwnViewOptions,
    FPwnViewExecute,
} FPwnView;

typedef enum {
    FPwnCategoryRecon,
    FPwnCategoryCredential,
    FPwnCategoryExploit,
    FPwnCategoryPost,
    FPwnCategoryCount,
} FPwnCategory;

typedef enum {
    FPwnPlatformWindows = (1 << 0),
    FPwnPlatformMac = (1 << 1),
    FPwnPlatformLinux = (1 << 2),
    FPwnPlatformAll = 0x07,
} FPwnPlatform;

typedef enum {
    FPwnOSUnknown,
    FPwnOSWindows,
    FPwnOSMac,
    FPwnOSLinux,
} FPwnOS;

/* =========================================================================
 * Data types
 * ========================================================================= */

typedef struct {
    char name[FPWN_OPT_NAME_LEN];
    char value[FPWN_OPT_VALUE_LEN];
    char description[FPWN_OPT_DESC_LEN];
} FPwnOption;

/* Lightweight module metadata — loaded from scanning .fpwn headers */
typedef struct {
    char name[FPWN_NAME_LEN];
    char description[FPWN_DESC_LEN];
    FPwnCategory category;
    uint8_t platforms; /* bitmask of FPwnPlatform */
    char file_path[FPWN_PATH_LEN];

    /* Options (populated when module is selected) */
    FPwnOption options[FPWN_MAX_OPTIONS];
    uint8_t option_count;
    bool options_loaded;
} FPwnModule;

/* Execution status passed to the execute view */
typedef struct {
    char status[128];
    uint32_t lines_done;
    uint32_t lines_total;
    bool finished;
    bool error;
} FPwnExecModel;

/* Main app state */
typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    Submenu* category_menu;
    Submenu* module_list;
    Widget* module_info;
    VariableItemList* options_list;
    View* execute_view;

    /* Services */
    Storage* storage;
    NotificationApp* notifications;

    /* Module catalog */
    FPwnModule modules[FPWN_MAX_MODULES];
    uint32_t module_count;

    /* State */
    FPwnCategory current_category;
    int32_t selected_module_index;
    FPwnOS detected_os;
    FPwnOS manual_os; /* 0 = auto-detect */

    /* Execution */
    FuriThread* exec_thread;
    bool abort_requested;
    FuriMutex* mutex;
} FPwnApp;

/* =========================================================================
 * Payload engine (payload_engine.c)
 * ========================================================================= */

/* Scan FPWN_MODULES_DIR for .fpwn files, populate app->modules[].
 * Only reads metadata headers (NAME, CATEGORY, etc.) — not full payloads. */
void fpwn_modules_scan(FPwnApp* app);

/* Load full module details (options, payload sections) for the given index.
 * Populates module->options[] and sets module->options_loaded. */
bool fpwn_module_load_full(FPwnApp* app, uint32_t index);

/* Execute the selected module's payload on a background thread.
 * Detects OS (or uses manual_os), selects platform section,
 * substitutes {{OPTION}} values, types HID keystrokes. */
int32_t fpwn_payload_execute_thread(void* ctx);

/* =========================================================================
 * OS detection (os_detect.c)
 * ========================================================================= */

/* Attempt to detect the host OS via USB HID timing heuristics.
 * Must be called when USB HID is active. */
FPwnOS fpwn_os_detect(void);

/* Return a human-readable name for the OS. */
const char* fpwn_os_name(FPwnOS os);

/* =========================================================================
 * App helpers (flipperpwn.c) — used across translation units
 * ========================================================================= */

/* Resolve the effective OS: manual_os wins over detected_os.
 * Used by payload_engine.c to select the correct payload section. */
FPwnOS fpwn_effective_os(const FPwnApp* app);

/* =========================================================================
 * Entry point
 * ========================================================================= */

int32_t flipperpwn_app(void* p);
