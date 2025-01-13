#pragma once
#include <furi.h>
#include <furi_hal.h>

#include <string.h>

#include "nfc_comparator_icons.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>

#include <nfc_device.h>
#include <nfc_listener.h>
#include <nfc_scanner.h>

#include "scenes/nfc_comparator_scene.h"

#include "lib/reader_worker/nfc_comparator_reader_worker.h"

typedef enum {
   NfcComparatorView_Submenu
} NfcComparatorViews;

typedef struct {
   SceneManager* scene_manager;
   ViewDispatcher* view_dispatcher;
   Submenu* submenu;
} NfcComparator;
