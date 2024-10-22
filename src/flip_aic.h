#pragma once

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

#define TAG "FlipAIC"

typedef enum {
    FlipAICViewSubmenu,
} FlipAICView;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;

    Nfc* nfc;

    NfcScanner* nfc_scanner;
    NfcProtocol nfc_protocol;
    NfcPoller* nfc_poller;

    // NfcDevice* nfc_device;
    // NfcListener* nfc_listener;
} FlipAIC;
