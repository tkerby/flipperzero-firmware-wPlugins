/*
 * nb_backend_flipper.h - NbBackend implemented on top of the Flipper NFC stack.
 */
#pragma once
#include "core/nb_backend.h"

typedef struct FbBackend FbBackend;

/* Takes over the NFC hardware until freed. */
FbBackend* fb_backend_alloc(void);
void fb_backend_free(FbBackend* f);

/* The table the protocol engines use. Valid until fb_backend_free(). */
const NbBackend* fb_backend_get(FbBackend* f);
