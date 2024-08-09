#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/nfc_common.h>
#include <nfc/nfc_device.h>

typedef enum {
    NfcEinkManufacturerWaveshare,
    NfcEinkManufacturerGoodisplay,

    NfcEinkManufacturerNum
} NfcEinkManufacturer;

typedef enum {
    NfcEinkTypeWaveshare2n13inch,
    NfcEinkTypeWaveshare2n9inch,
    //NfcEinkTypeWaveshare4n2inch,

    NfcEinkTypeNum
} NfcEinkType;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t data_block_size;
    NfcEinkType screen_type;
    NfcEinkManufacturer screen_manufacturer;
    const char* name;
} NfcEinkScreenDescriptor; //TODO:NfcEinkScreenBase

typedef struct {
    uint8_t* image_data;
    uint16_t image_size;
    uint16_t received_data;
    NfcEinkScreenDescriptor base;
} NfcEinkScreenData;

typedef NfcDevice* (*EinkScreenAllocCallback)();
typedef void (*EinkScreenFreeCallback)(NfcDevice* instance);
typedef bool (*EinkScreenInitCallback)(NfcEinkScreenData* screen);

typedef void (*NfcEinkScreenDoneCallback)(void* context);

typedef struct {
    NfcEinkScreenDoneCallback done_callback;
    void* context;
} NfcEinkScreenDoneEvent;

typedef struct {
    EinkScreenAllocCallback alloc;
    EinkScreenFreeCallback free;
    EinkScreenInitCallback init;
    NfcGenericCallback listener_callback;
    NfcGenericCallback poller_callback;
} NfcEinkScreenHandlers;

typedef struct {
    NfcDevice* nfc_device;
    NfcEinkScreenData* data;
    BitBuffer* tx_buf;
    const NfcEinkScreenHandlers* handlers;
    NfcEinkScreenDoneEvent done_event;
} NfcEinkScreen;
