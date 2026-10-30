#pragma once

#include "hub_transport.h"

#include <furi.h>
#include <storage/storage.h>

typedef struct {
    FuriMutex* lock;
    uint32_t requests;
    char last[40]; /* "GET /api/info", for the screen */
} HubStats;

/* Serve one HTTP request on the open connection, then close it. */
void hub_http_handle(HubTransport* t, Storage* storage, HubStats* stats);
