// views/kia_decoder_receiver_info.h
#pragma once

#include <gui/view.h>
#include "../helpers/kia_decoder_types.h"

typedef struct KiaReceiverInfo KiaReceiverInfo;

typedef void (*KiaReceiverInfoCallback)(KiaCustomEvent event, void* context);

void kia_view_receiver_info_set_callback(
    KiaReceiverInfo* receiver_info,
    KiaReceiverInfoCallback callback,
    void* context);

KiaReceiverInfo* kia_view_receiver_info_alloc(void);
void kia_view_receiver_info_free(KiaReceiverInfo* receiver_info);
View* kia_view_receiver_info_get_view(KiaReceiverInfo* receiver_info);

void kia_view_receiver_info_set_protocol_name(
    KiaReceiverInfo* receiver_info,
    const char* protocol_name);

void kia_view_receiver_info_set_info_text(
    KiaReceiverInfo* receiver_info,
    const char* info_text);