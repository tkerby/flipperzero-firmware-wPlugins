#pragma once

#include "sink/wmbus_packet_sink.h"

#include "../protocol/capture/wmbus_capture.h"
#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "../storage/wmbus_settings.h"

typedef struct WmBusCaptureProcessor WmBusCaptureProcessor;

WmBusCaptureProcessor* wmbus_capture_processor_alloc(
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store);
void wmbus_capture_processor_free(WmBusCaptureProcessor* processor);
bool wmbus_capture_processor_add_sink(
    WmBusCaptureProcessor* processor,
    const WmBusPacketSink* sink);
bool wmbus_capture_processor_start(WmBusCaptureProcessor* processor);
bool wmbus_capture_processor_apply_config(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store);
bool wmbus_capture_processor_submit_frame(
    WmBusCaptureProcessor* processor,
    const WmBusPhyFrame* phy_frame);
