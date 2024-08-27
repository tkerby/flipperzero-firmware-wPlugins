#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/nfc_common.h>
#include <nfc/nfc_device.h>

typedef enum {
    //NfcEinkManufacturerGeneric,
    NfcEinkManufacturerWaveshare,
    NfcEinkManufacturerGoodisplay,

    NfcEinkManufacturerNum
} NfcEinkManufacturer;

typedef enum {
    NfcEinkTypeUnknown
} NfcEinkType;

typedef enum {
    NfcEinkScreenEventTypeTargetDetected,
    NfcEinkScreenEventTypeTargetLost,

    NfcEinkScreenEventTypeConfigurationReceived,
    NfcEinkScreenEventTypeBlockProcessed,

    NfcEinkScreenEventTypeFinish,
    NfcEinkScreenEventTypeFailure,
} NfcEinkScreenEventType;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t data_block_size;
    uint8_t screen_type;
    NfcEinkManufacturer screen_manufacturer;
    const char* name;
} NfcEinkScreenDescriptor; //TODO:NfcEinkScreenBase

typedef void NfcEinkScreenSpecificContext;

typedef struct {
    uint8_t* image_data;
    uint16_t image_size;
    uint16_t received_data;
    NfcEinkScreenDescriptor base;
} NfcEinkScreenData;

typedef struct {
    NfcDevice* nfc_device;
    NfcEinkScreenSpecificContext* screen_context;
} NfcEinkScreenDevice;

typedef NfcEinkScreenDevice* (*EinkScreenAllocCallback)();
typedef void (*EinkScreenFreeCallback)(NfcEinkScreenDevice* instance);
//typedef void (*EinkScreenInitCallback)(NfcEinkScreenDescriptor* descriptor, NfcEinkType type);
typedef void (*EinkScreenInitCallback)(NfcEinkScreenData* data, NfcEinkType type);

typedef void (*NfcEinkScreenEventCallback)(NfcEinkScreenEventType type, void* context);

typedef void* NfcEinkScreenEventContext;

typedef struct {
    EinkScreenAllocCallback alloc;
    EinkScreenFreeCallback free;
    EinkScreenInitCallback init;
    NfcGenericCallback listener_callback;
    NfcGenericCallback poller_callback;
} NfcEinkScreenHandlers;

typedef struct {
    NfcEinkScreenData* data;
    NfcEinkScreenDevice* device;
    BitBuffer* tx_buf;
    BitBuffer* rx_buf;
    const NfcEinkScreenHandlers* handlers;
    NfcEinkScreenEventCallback event_callback;
    NfcEinkScreenEventContext event_context;

    bool was_update; ///TODO: Candidates to move
    uint8_t update_cnt; //to protocol specific instance
    uint8_t response_cnt;
} NfcEinkScreen;
