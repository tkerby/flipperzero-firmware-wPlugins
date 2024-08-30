#include "nfc_eink_screen.h"
#include "nfc_eink_tag.h"

typedef struct {
    uint8_t* image_data;
    uint16_t image_size;
    uint16_t received_data;
    NfcEinkScreenDescriptor base;
} NfcEinkScreenData;

typedef void NfcEinkScreenSpecificContext;

typedef struct {
    NfcDevice* nfc_device;
    uint16_t block_total;
    uint16_t block_current;
    NfcEinkScreenType screen_type;
    NfcEinkScreenSpecificContext* screen_context;
} NfcEinkScreenDevice;

typedef NfcEinkScreenDevice* (*EinkScreenAllocCallback)();
typedef void (*EinkScreenFreeCallback)(NfcEinkScreenDevice* instance);
typedef void (*EinkScreenInitCallback)(NfcEinkScreenData* data, NfcEinkScreenType type);

typedef struct {
    EinkScreenAllocCallback alloc;
    EinkScreenFreeCallback free;
    EinkScreenInitCallback init; ///TODO: this can be removed as it is no longer used
    NfcGenericCallback listener_callback;
    NfcGenericCallback poller_callback;
} NfcEinkScreenHandlers;

struct NfcEinkScreen {
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
};

void nfc_eink_screen_vendor_callback(NfcEinkScreen* instance, NfcEinkScreenEventType type);
