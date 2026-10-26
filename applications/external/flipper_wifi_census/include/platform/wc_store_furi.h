#pragma once

#include "include/ports/wc_store_port.h"

// Storage port backed by furi Storage, rooted at the app's data directory (APP_DATA_PATH).
// Ensures that directory exists on first use. Returns a port value; no allocation to free.
WcStorePort wc_store_furi_port(void);
