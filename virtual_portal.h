#pragma once

#include <notification/notification_messages.h>
#include <furi_hal_light.h>
#include "pof_token.h"

#define POF_TOKEN_LIMIT 16

typedef enum {
    PoFHid,
    PoFXbox360
} PoFType;

typedef enum {
    EventExit = (1 << 0),
    EventReset = (1 << 1),
    EventRx = (1 << 2),
    EventTx = (1 << 3),
    EventTxComplete = (1 << 4),
    EventResetSio = (1 << 5),
    EventTxImmediate = (1 << 6),

    EventAll = EventExit | EventReset | EventRx | EventTx | EventTxComplete | EventResetSio |
               EventTxImmediate,
} PoFEvent;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t delay;
} VirtualPortalLed;

typedef struct {
    PoFToken* tokens[POF_TOKEN_LIMIT];
    uint8_t sequence_number;
    bool active;
    bool speaker;
    NotificationApp* notifications;
    PoFType type;
    VirtualPortalLed left;
    VirtualPortalLed right;
    VirtualPortalLed trap;
} VirtualPortal;

VirtualPortal* virtual_portal_alloc(NotificationApp* notifications);

void virtual_portal_free(VirtualPortal* virtual_portal);
void virtual_portal_cleanup(VirtualPortal* virtual_portal);
void virtual_portal_load_token(VirtualPortal* virtual_portal, PoFToken* pof_token);
void virtual_portal_tick(VirtualPortal* virtual_portal);

int virtual_portal_process_message(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t* response);

int virtual_portal_send_status(VirtualPortal* virtual_portal, uint8_t* response);
