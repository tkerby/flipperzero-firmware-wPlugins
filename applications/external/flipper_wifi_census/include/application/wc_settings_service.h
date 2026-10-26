#pragma once

#include "include/ports/wc_store_port.h"

#include <stdbool.h>
#include <stdint.h>

// User options that outlive a run of the app. Small and flat on purpose: two scalars do not
// warrant a domain codec of their own.
typedef struct {
    uint32_t baud;
    bool autosave;
} WcSettings;

#define WC_SETTINGS_BAUD_DEFAULT 115200

// Load the stored options. A missing file is not an error: `s` comes back with the defaults.
// Returns false only if a file exists but is corrupt (`s` still holds the defaults).
bool wc_settings_service_load(const WcStorePort *store, WcSettings *s);

bool wc_settings_service_save(const WcStorePort *store, const WcSettings *s);
