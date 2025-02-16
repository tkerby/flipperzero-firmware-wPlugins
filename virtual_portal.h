#pragma once

#include <notification/notification_messages.h>
#include <furi_hal_light.h>
#include "pof_token.h"

#define POF_TOKEN_LIMIT 16

typedef struct {
    PoFToken* tokens[POF_TOKEN_LIMIT];
    uint8_t sequence_number;
    bool active;
    bool speaker;
    NotificationApp* notifications;
} VirtualPortal;

VirtualPortal* virtual_portal_alloc(NotificationApp* notifications);

void virtual_portal_free(VirtualPortal* virtual_portal);
void virtual_portal_cleanup(VirtualPortal* virtual_portal);
void virtual_portal_load_token(VirtualPortal* virtual_portal, PoFToken* pof_token);

int virtual_portal_process_message(
    VirtualPortal* virtual_portal,
    uint8_t* message,
    uint8_t* response);

int virtual_portal_send_status(VirtualPortal* virtual_portal, uint8_t* response);
