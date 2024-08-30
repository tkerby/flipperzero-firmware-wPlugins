#pragma once
#include "../nfc_eink_screen_i.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>

typedef enum {
    NfcEinkWaveshareListenerStateIdle,
    NfcEinkWaveshareListenerStateWaitingForConfig,
    NfcEinkWaveshareListenerStateReadingBlocks,
    NfcEinkWaveshareListenerStateUpdatedSuccefully,
} NfcEinkWaveshareListenerStates;

typedef struct {
    NfcEinkWaveshareListenerStates listener_state;

} NfcEinkWaveshareSpecificContext;

#define eink_waveshare_on_done(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeFinish)

#define eink_waveshare_on_config_received(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeConfigurationReceived)

#define eink_waveshare_on_target_detected(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetDetected)

#define eink_waveshare_on_target_lost(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetLost)

#define eink_waveshare_on_block_processed(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeBlockProcessed)

#define eink_waveshare_on_updating(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeUpdating)

#define eink_waveshare_on_error(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeFailure)
