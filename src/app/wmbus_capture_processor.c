#include "wmbus_capture_processor.h"

#include "../protocol/packet/wmbus_packet.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WmBusDecoder"

#define WMBUS_CAPTURE_PROCESSOR_QUEUE_DEPTH 6U
#define WMBUS_CAPTURE_PROCESSOR_STACK_SIZE  (6U * 1024U)

typedef enum {
    WmBusProcessorEventFrame = 0,
    WmBusProcessorEventApplyConfig,
    WmBusProcessorEventStop,
} WmBusProcessorEventType;

typedef struct {
    WmBusProcessorEventType type;
    union {
        WmBusPhyFrame frame;
        struct {
            WmBusSettings settings;
            WmBusCryptoKeyStore key_store;
        } config;
    } data;
} WmBusProcessorEvent;

struct WmBusCaptureProcessor {
    WmBusPacketCallback callback;
    void* context;
    FuriMessageQueue* queue;
    FuriThread* thread;
    uint32_t dropped_frames;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
};

static void wmbus_capture_processor_process_frame(
    WmBusCaptureProcessor* processor,
    const WmBusProcessorEvent* event) {
    WmBusPacketRecord record = {0};
    if(!wmbus_packet_process_phy_frame(&event->data.frame, &processor->key_store, &record)) {
        return;
    }

    processor->callback(processor->context, &processor->settings, &record);
}

static int32_t wmbus_capture_processor_thread(void* context) {
    WmBusCaptureProcessor* processor = context;
    WmBusProcessorEvent event;

    while(furi_message_queue_get(processor->queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(event.type == WmBusProcessorEventStop) break;
        if(event.type == WmBusProcessorEventApplyConfig) {
            processor->settings = event.data.config.settings;
            processor->key_store = event.data.config.key_store;
        } else if(event.type == WmBusProcessorEventFrame) {
            wmbus_capture_processor_process_frame(processor, &event);
        }
    }

    return 0;
}

WmBusCaptureProcessor* wmbus_capture_processor_alloc(
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketCallback callback,
    void* context) {
    if(!settings || !key_store || !callback) return NULL;

    WmBusCaptureProcessor* processor = malloc(sizeof(*processor));
    if(!processor) {
        return NULL;
    }

    memset(processor, 0, sizeof(*processor));
    processor->callback = callback;
    processor->context = context;
    processor->settings = *settings;
    processor->key_store = *key_store;
    processor->queue =
        furi_message_queue_alloc(WMBUS_CAPTURE_PROCESSOR_QUEUE_DEPTH, sizeof(WmBusProcessorEvent));
    if(!processor->queue) {
        free(processor);
        return NULL;
    }

    processor->thread = furi_thread_alloc_ex(
        "WmBusProcess",
        WMBUS_CAPTURE_PROCESSOR_STACK_SIZE,
        wmbus_capture_processor_thread,
        processor);
    if(!processor->thread) {
        furi_message_queue_free(processor->queue);
        free(processor);
        return NULL;
    }
    furi_thread_set_priority(processor->thread, FuriThreadPriorityLow);
    furi_thread_start(processor->thread);
    return processor;
}

void wmbus_capture_processor_free(WmBusCaptureProcessor* processor) {
    if(!processor) return;

    WmBusProcessorEvent event = {.type = WmBusProcessorEventStop};
    furi_message_queue_put(processor->queue, &event, FuriWaitForever);
    furi_thread_join(processor->thread);
    furi_thread_free(processor->thread);
    furi_message_queue_free(processor->queue);
    free(processor);
}

bool wmbus_capture_processor_submit_frame(
    WmBusCaptureProcessor* processor,
    const WmBusPhyFrame* phy_frame) {
    if(!processor || !phy_frame) {
        return false;
    }

    WmBusProcessorEvent event = {
        .type = WmBusProcessorEventFrame,
        .data.frame = *phy_frame,
    };

    if(furi_message_queue_put(processor->queue, &event, 0U) != FuriStatusOk) {
        processor->dropped_frames++;
        if(processor->dropped_frames == 1U ||
           (processor->dropped_frames & (processor->dropped_frames - 1U)) == 0U) {
            FURI_LOG_W(
                TAG,
                "processor queue full: dropped=%lu mode=%c",
                (unsigned long)processor->dropped_frames,
                phy_frame->mode == WmBusRxModeT ? 'T' : 'C');
        }
        return false;
    }

    return true;
}

bool wmbus_capture_processor_apply_config(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store) {
    if(!processor || !settings || !key_store) return false;

    WmBusProcessorEvent event = {
        .type = WmBusProcessorEventApplyConfig,
        .data.config =
            {
                .settings = *settings,
                .key_store = *key_store,
            },
    };
    return furi_message_queue_put(processor->queue, &event, FuriWaitForever) == FuriStatusOk;
}
