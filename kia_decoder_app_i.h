// kia_decoder_app_i.h
#pragma once

#include "helpers/kia_decoder_types.h"
#include "scenes/kia_decoder_scene.h"
#include "views/kia_decoder_receiver.h"
#include "views/kia_decoder_receiver_info.h"
#include "kia_decoder_history.h"
#include "helpers/radio_device_loader.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/devices/devices.h>

typedef struct KiaDecoderApp KiaDecoderApp;

typedef struct {
    SubGhzWorker* worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    KiaHistory* history;
    const SubGhzDevice* radio_device;
    KiaTxRxState txrx_state;
    KiaHopperState hopper_state;
    KiaRxKeyState rx_key_state;
    uint8_t hopper_idx_frequency;
    uint8_t hopper_timeout;
    uint16_t idx_menu_chosen;
} KiaDecoderTxRx;

struct KiaDecoderApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    KiaReceiver* kia_receiver;
    KiaReceiverInfo* kia_receiver_info;
    KiaDecoderTxRx* txrx;
    SubGhzSetting* setting;
    KiaLock lock;
};

void kia_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);

bool kia_set_preset(KiaDecoderApp* app, const char* preset);

void kia_get_frequency_modulation(
    KiaDecoderApp* app,
    FuriString* frequency,
    FuriString* modulation);

void kia_begin(KiaDecoderApp* app, uint8_t* preset_data);
uint32_t kia_rx(KiaDecoderApp* app, uint32_t frequency);
void kia_idle(KiaDecoderApp* app);
void kia_rx_end(KiaDecoderApp* app);
void kia_sleep(KiaDecoderApp* app);
void kia_hopper_update(KiaDecoderApp* app);