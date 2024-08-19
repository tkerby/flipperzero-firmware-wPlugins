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
    NfcEinkScreenEventTypeConfigurationReceived,
    NfcEinkScreenEventTypeDone,
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
    NfcEinkScreenSpecificContext* screen_context;
} NfcEinkScreenData;

typedef NfcDevice* (*EinkScreenAllocCallback)();
typedef void (*EinkScreenFreeCallback)(NfcDevice* instance);
//typedef void (*EinkScreenInitCallback)(NfcEinkScreenDescriptor* descriptor, NfcEinkType type);
typedef void (*EinkScreenInitCallback)(NfcEinkScreenData* data, NfcEinkType type);

typedef void (*NfcEinkScreenDoneCallback)(void* context);
typedef void (*NfcEinkScreenEventCallback)(NfcEinkScreenEventType type, void* context);

typedef struct {
    NfcEinkScreenDoneCallback done_callback;
    void* context;
} NfcEinkScreenDoneEvent;

/* typedef struct {
    //NfcEinkScreenEventType type;
    NfcEinkScreenEventCallback callback;
    void* context;
} NfcEinkScreenInternalEvent; */

typedef struct {
    EinkScreenAllocCallback alloc_nfc_device;
    EinkScreenFreeCallback free;
    EinkScreenInitCallback init;
    NfcGenericCallback listener_callback;
    NfcGenericCallback poller_callback;
} NfcEinkScreenHandlers;

typedef struct {
    NfcDevice* nfc_device;
    NfcEinkScreenData* data;
    BitBuffer* tx_buf;
    BitBuffer* rx_buf;
    const NfcEinkScreenHandlers* handlers;
    NfcEinkScreenDoneEvent done_event;

    NfcEinkScreenEventCallback internal_event_callback;
    bool was_update; ///TODO: Candidates to move
    uint8_t update_cnt; //to protocol specific instance
    //NfcEinkScreenInternalEvent internal_event;
} NfcEinkScreen;
