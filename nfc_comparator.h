#pragma once
#include <furi.h>
#include <furi_hal.h>

#include "nfc_comparator_icons.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/popup.h>

#include <nfc_device.h>
#include <nfc_listener.h>
#include <nfc_scanner.h>

#include <notification/notification_messages.h>

#include <storage/storage.h>

#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/path.h>

#include "scenes/nfc_comparator_scene.h"

#include "lib/reader_worker/nfc_comparator_reader_worker.h"

#define NFC_ITEM_LOCATION "/ext/nfc/"

typedef enum {
   NfcComparatorView_Submenu,
   NfcComparatorView_FileBrowser,
   NfcComparatorView_Popup
} NfcComparatorViews;

typedef struct {
   SceneManager* scene_manager;
   ViewDispatcher* view_dispatcher;
   Submenu* submenu;
   FileBrowser* file_browser;
   FuriString* file_browser_output;
   Popup* popup;
   NfcComparatorReaderWorker* worker;
   NotificationApp* notification_app;
} NfcComparator;

typedef enum {
   NfcComparatorLedState_Running,
   NfcComparatorLedState_complete,
   NfcComparatorLedState_error
} NfcComparatorLedState;

void start_led(NfcComparator* nfc_comparator, NfcComparatorLedState state);
void stop_led(NfcComparator* nfc_comparator);