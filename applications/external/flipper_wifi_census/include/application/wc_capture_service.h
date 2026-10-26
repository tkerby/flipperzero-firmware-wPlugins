#pragma once

#include "include/domain/wc_capture_codec.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Persist a census as "<basename>.wcen" (binary) plus a "<basename>.csv" sidecar for the PC.
// Returns false if the basename is too long or either write fails.
bool wc_capture_service_save(const WcStorePort *store, const char *basename,
                             const WcCaptureMeta *meta, const WcCensus *c);

// Load a binary capture file (name including the .wcen extension) into meta + census.
bool wc_capture_service_load(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                             WcCensus *c);

// Called once per stored device while streaming. Returning false aborts the walk: a capture is
// taken whole or refused, never delivered short.
typedef bool (*WcCaptureDeviceFn)(void *ctx, const WcSignature *d);

// Walk a stored capture record by record, without ever holding the file. `meta` and the device
// count come from its header; `max_devices` refuses a capture bigger than the caller can take,
// before anything is read.
bool wc_capture_service_stream(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                               uint16_t max_devices, WcCaptureDeviceFn on_device, void *ctx);

// Devices a stored capture declares, read from its header alone (0 if unreadable). A file with
// more than this build's ceiling is refused by the loader, and without this the refusal is
// indistinguishable from a corrupt file.
uint16_t wc_capture_service_device_count(const WcStorePort *store, const char *filename);

// Whether "<basename>.wcen" already exists. Saving truncates, and the default name has only
// minute resolution, so two scans in the same minute would overwrite each other.
bool wc_capture_service_exists(const WcStorePort *store, const char *basename);
