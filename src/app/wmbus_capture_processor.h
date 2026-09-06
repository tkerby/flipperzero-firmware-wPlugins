#pragma once

#include "../protocol/packet/wmbus_packet.h"

#include "../protocol/capture/wmbus_capture.h"
#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "../storage/wmbus_settings.h"

typedef struct WmBusCaptureProcessor WmBusCaptureProcessor;

typedef void (*WmBusPacketCallback)(
    void* context,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record);

WmBusCaptureProcessor* wmbus_capture_processor_alloc(
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketCallback callback,
    void* context);
void wmbus_capture_processor_free(WmBusCaptureProcessor* processor);
bool wmbus_capture_processor_apply_config(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store);
bool wmbus_capture_processor_submit_frame(
    WmBusCaptureProcessor* processor,
    const WmBusPhyFrame* phy_frame);
