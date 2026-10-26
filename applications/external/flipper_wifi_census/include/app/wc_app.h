#pragma once

#include "include/application/wc_capture_service.h"
#include "include/application/wc_compare_service.h"
#include "include/application/wc_known_service.h"
#include "include/application/wc_scan_service.h"
#include "include/application/wc_settings_service.h"
#include "include/platform/wc_serial_furi.h"
#include "include/views/wc_scroll_list.h"

#include <gui/gui.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#define WC_TEXT_BUF_SIZE 64
#define WC_RESULT_TEXT_SIZE 1024
// Rows a list can show: the census ceiling, so a full capture is browsable end to end.
#define WC_MAX_LIST WC_CENSUS_MAX_DEVICES
// Entries the app can hold NAMES for: capture files and probed SSIDs, the only two whose text
// has no other copy in memory. Independent of the device ceiling, which they do not follow.
#define WC_MAX_NAMED 100
_Static_assert(WC_MAX_LIST + 1 <= WC_SCROLL_LIST_MAX, "list view too small for the longest list");
// Auto-save rotates a bit before the hard ceiling so the ~timer-tick window before rotation
// still has room and does not drop devices.
#define WC_AUTOSAVE_ROTATE_AT (WC_CENSUS_MAX_DEVICES - 20)

typedef enum {
    WcViewList,
    WcViewTextInput,
    WcViewTextBox,
    WcViewWidget,
    WcViewVarList,
} WcViewId;

typedef enum {
    WcCustomEventScanTick = 100,
    WcCustomEventTextDone = 200,
    WcCustomEventButton = 300,
} WcCustomEvent;

// What a label-entry (MarkLabel) applies to when the user confirms.
typedef enum {
    WcMarkDevice, // create a rule from the selected device
    WcMarkSsid,   // create a rule from the selected probed network
    WcMarkRename, // rename the selected known entry
} WcMarkMode;

typedef struct {
    Gui *gui;
    ViewDispatcher *view_dispatcher;
    SceneManager *scene_manager;
    WcScrollList *list_view;
    TextInput *text_input;
    TextBox *text_box;
    Widget *widget;
    VariableItemList *var_item_list;

    // Ports and services.
    WcStorePort store;
    WcClockPort clock;
    WcSerialFuri *serial;
    uint32_t baud;
    WcScanService scan;
    FuriTimer *scan_timer;
    uint32_t scan_started; // epoch when the current scan began (for capture duration)
    bool scan_link_ok;     // false when the scan could not open the USART (busy)
    bool autosave;         // rotate to a new file when the census fills, instead of dropping
    char autosave_base[WC_TEXT_BUF_SIZE]; // base name for the current auto-save run
    uint16_t autosave_idx;                // next auto-save file index
    uint16_t autosave_failed;             // chunks the SD refused, shown on the scan screen

    // Browsing / comparison state.
    char text_buf[WC_TEXT_BUF_SIZE]; // capture/label entry
    char selected_file[WC_TEXT_BUF_SIZE];
    char import_path[128]; // absolute path of a pcap picked in the file browser
    // Compare and Merge are both "pick two captures, then act", so they share the two slots
    // the picker scenes fill instead of carrying a field each.
    char picked[2][WC_TEXT_BUF_SIZE];
    char result_text[WC_RESULT_TEXT_SIZE];
    WcCensus *browse_census; // loaded capture, heap, freed on scene exit
    uint16_t selected_device;
    WcKnownDb known;
    WcMarkMode mark_mode;                    // what the next MarkLabel confirm applies to
    char selected_ssid[WC_SSID_MAX_LEN + 1]; // network picked in the Networks list
    uint16_t selected_known;                 // index picked in the Known list

    // Scratch for the selection lists. list_names holds what has no other copy in memory: the
    // capture file names from the SD listing, and the SSIDs of the networks list. Device and
    // known rows are rendered straight from the census and the registry instead.
    char list_names[WC_MAX_NAMED][WC_TEXT_BUF_SIZE];
    uint16_t list_counts[WC_MAX_NAMED]; // devices per SSID, for the networks rows
    uint16_t list_count;
    uint16_t list_overflow;       // entries the list could not hold, so the screen can say so
    char menu_autosave_label[24]; // the list borrows it, so it cannot live on the stack

    char debug_buf[512]; // rolling raw serial lines for the Serial debug view
} WcApp;

// Entry point body (called from main.c). Runs the whole app and returns 0.
int32_t wc_app_run(void);
