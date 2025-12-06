// views/kia_decoder_receiver.h
#pragma once

#include <gui/view.h>
#include "../helpers/kia_decoder_types.h"

typedef struct KiaReceiver KiaReceiver;

typedef void (*KiaReceiverCallback)(KiaCustomEvent event, void* context);

void kia_view_receiver_set_callback(
    KiaReceiver* receiver,
    KiaReceiverCallback callback,
    void* context);

KiaReceiver* kia_view_receiver_alloc(void);
void kia_view_receiver_free(KiaReceiver* receiver);
View* kia_view_receiver_get_view(KiaReceiver* receiver);

void kia_view_receiver_add_item_to_menu(KiaReceiver* receiver, const char* name, uint8_t type);

void kia_view_receiver_add_data_statusbar(
    KiaReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio);

uint16_t kia_view_receiver_get_idx_menu(KiaReceiver* receiver);
void kia_view_receiver_set_idx_menu(KiaReceiver* receiver, uint16_t idx);
void kia_view_receiver_set_rssi(KiaReceiver* receiver, float rssi);
void kia_view_receiver_set_lock(KiaReceiver* receiver, KiaLock lock);