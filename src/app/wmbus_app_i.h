#pragma once

#include "wmbus_app.h"
#include "wmbus_capture_processor.h"
#include "radio/wmbus_radio_rx_service.h"

#include "../storage/wmbus_keyring.h"
#include "../storage/wmbus_settings.h"
#include "../ui/scenes/wmbus_scene.h"
#include "../ui/views/wmbus_rx_view.h"

#include <gui/gui.h>
#include <gui/modules/byte_input.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <storage/storage.h>

typedef enum {
    WmBusAppViewRx = 0,
    WmBusAppViewConfig,
    WmBusAppViewKeyInput,
    WmBusAppViewWidget,
} WmBusAppView;

typedef enum {
    WmBusCustomEventOpenConfig = WmBusRxViewEventOpenConfig,
    WmBusCustomEventOpenDetails = WmBusRxViewEventOpenDetails,
    WmBusCustomEventToggleDebug = WmBusRxViewEventToggleDebug,
    WmBusCustomEventOpenApplicationRecords = WmBusRxViewEventOpenApplicationRecords,
    WmBusCustomEventConfigOpenKeyInput = 100,
    WmBusCustomEventConfigKeyInputDone,
    WmBusCustomEventConfigOpenAbout,
    WmBusCustomEventExit,
    WmBusCustomEventStay,
} WmBusCustomEvent;

typedef struct WmBusApp WmBusApp;

struct WmBusApp {
    Gui* gui;
    Storage* storage;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* config_list;
    ByteInput* key_input;
    Widget* widget;
    bool detail_application_only;
    WmBusRxView* rx_view;

    WmBusCaptureProcessor* capture_processor;
    WmBusRadioRxService* rx_service;

    WmBusSettings settings;
    WmBusKeyring keyring;
    uint8_t key_input_bytes[WMBUS_KEY_BYTES];
};

bool wmbus_app_apply_runtime_config(WmBusApp* app, bool persist);
bool wmbus_app_add_key(WmBusApp* app, const uint8_t key[WMBUS_KEY_BYTES]);
bool wmbus_app_ensure_config_view(WmBusApp* app);
bool wmbus_app_ensure_key_input_view(WmBusApp* app);
bool wmbus_app_ensure_widget(WmBusApp* app);
