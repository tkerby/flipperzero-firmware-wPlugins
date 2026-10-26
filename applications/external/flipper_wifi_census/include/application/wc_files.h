#pragma once

// Shared file-naming conventions for the storage layer. Capture files carry the user's
// label plus these extensions; the known-devices registry and the settings are fixed files.
#define WC_CAP_EXT ".wcen"
#define WC_CSV_EXT ".csv"
#define WC_KNOWN_FILE "known.db"
#define WC_SETTINGS_FILE "settings.db"
#define WC_BASENAME_MAX 40 // longest capture label the UI allows
