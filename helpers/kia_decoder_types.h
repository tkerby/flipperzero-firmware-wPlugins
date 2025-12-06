// helpers/kia_decoder_types.h
#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef enum {
    KiaDecoderViewVariableItemList,
    KiaDecoderViewSubmenu,
    KiaDecoderViewWidget,
    KiaDecoderViewReceiver,
    KiaDecoderViewReceiverInfo,
} KiaDecoderView;

typedef enum {
    // Custom events for views
    KiaCustomEventViewReceiverOK,
    KiaCustomEventViewReceiverConfig,
    KiaCustomEventViewReceiverBack,
    KiaCustomEventViewReceiverUnlock,
    // Custom events for scenes
    KiaCustomEventSceneReceiverUpdate,
    KiaCustomEventSceneSettingLock,
} KiaCustomEvent;

typedef enum {
    KiaLockOff,
    KiaLockOn,
} KiaLock;

typedef enum {
    KiaTxRxStateIDLE,
    KiaTxRxStateRx,
    KiaTxRxStateSleep,
} KiaTxRxState;

typedef enum {
    KiaHopperStateOFF,
    KiaHopperStateRunning,
    KiaHopperStatePause,
    KiaHopperStateRSSITimeOut,
} KiaHopperState;

typedef enum {
    KiaRxKeyStateIDLE,
    KiaRxKeyStateBack,
    KiaRxKeyStateStart,
    KiaRxKeyStateAddKey,
} KiaRxKeyState;