#pragma once

#include "../../protocol/capture/wmbus_capture.h"

typedef struct WmBusRadioRxService WmBusRadioRxService;

typedef struct {
    void* context;
    void (*handle_frame)(void* context, const WmBusPhyFrame* phy_frame);
    void (*set_freq_valid)(void* context, bool freq_valid);
    void (*set_live_rssi)(void* context, int rssi);
} WmBusRadioRxCallbacks;

WmBusRadioRxService*
    wmbus_radio_rx_service_alloc(const WmBusRadioRxCallbacks* callbacks, WmBusRxMode mode);

void wmbus_radio_rx_service_free(WmBusRadioRxService* service);

bool wmbus_radio_rx_service_apply_mode(WmBusRadioRxService* service, WmBusRxMode mode);
