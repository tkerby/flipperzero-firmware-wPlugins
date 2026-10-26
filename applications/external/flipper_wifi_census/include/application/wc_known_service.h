#pragma once

#include "include/domain/wc_known.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Load the known-devices registry. A missing file is not an error: `db` comes back empty.
// Returns false only if a file exists but is corrupt.
bool wc_known_service_load(const WcStorePort *store, WcKnownDb *db);

bool wc_known_service_save(const WcStorePort *store, const WcKnownDb *db);

// Mark a device (from a capture) as known: load, derive a rule, append, save. Returns false
// if the device yields no durable rule (randomized MAC with no directed SSID) or the db is
// full or a write fails.
bool wc_known_service_mark(const WcStorePort *store, const WcSignature *sig, const char *label);

// Mark a network (SSID) as known: load, append an SSID rule, save. Returns false if the SSID
// is empty, the db is full, or a write fails.
bool wc_known_service_mark_ssid(const WcStorePort *store, const char *ssid, const char *label);

// Remove the known rule at `index`: load, remove, save. Returns false on a bad index or write.
bool wc_known_service_remove(const WcStorePort *store, uint16_t index);

// Rename the known rule at `index`: load, set its label, save. Returns false on a bad index,
// an empty label, or a write failure.
bool wc_known_service_rename(const WcStorePort *store, uint16_t index, const char *label);
