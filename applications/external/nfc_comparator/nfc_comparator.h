#pragma once

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
#include "lib/led_worker/nfc_comparator_led_worker.h"

#define NFC_ITEM_LOCATION "/ext/nfc/"

typedef enum {
    NfcComparatorView_Submenu,
    NfcComparatorView_FileBrowser,
    NfcComparatorView_Popup,
    NfcComparatorView_Count
} NfcComparatorViews;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    FileBrowser* file_browser;
    FuriString* file_browser_output;
    Popup* popup;
    NotificationApp* notification_app;
    FuriThread* thread;
    NfcComparatorReaderWorker* nfc_comparator_reader_worker;
} NfcComparator;
