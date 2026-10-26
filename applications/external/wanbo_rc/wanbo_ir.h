#pragma once

#include <stdint.h>

typedef enum {
    WanboCommandPower,
    WanboCommandMouse,
    WanboCommandSettings,
    WanboCommandOk,
    WanboCommandUp,
    WanboCommandDown,
    WanboCommandLeft,
    WanboCommandRight,
    WanboCommandMenu,
    WanboCommandMute,
    WanboCommandReturn,
    WanboCommandHome,
    WanboCommandFocusPlus,
    WanboCommandFocusMinus,
    WanboCommandVolumePlus,
    WanboCommandVolumeMinus,
} WanboCommand;

void wanbo_ir_send(WanboCommand command);
const char* wanbo_ir_get_name(WanboCommand command);