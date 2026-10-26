#pragma once
// Asset file access + heap statistics. assets.c is the only file (besides main.c) that
// includes storage headers; everything else reads asset files through read_at().
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// `path` is either an absolute path ("/ext/...", "/assets/...") or a path relative to the app
// assets folder (e.g. "map_01.bin", "lang/es/txt_00.bin"), in which case APP_ASSETS_PATH is
// prepended.
// Reads exactly `len` bytes starting at `offset`; returns true only if all were read.
bool read_at(const char* path, uint32_t offset, void* buf, uint32_t len);
// Size in bytes, or -1 if the file cannot be opened.
int32_t file_size(const char* path);

// A file kept open for several reads in a row (e.g. the glyphs of one dialog page). Same path rule as
// read_at(). Close it with asset_close(); NULL on failure. NEVER keep one open across frames / scenes:
// Flipper's Storage never cleans up after an app, a handle that is still open when the app exits
// stays "open" until reboot, and the exported storage_file_open() then waits forever for the next
// open of that path. At most a few files can be open at once; NULL when that is exceeded.
typedef struct AssetFile AssetFile;
AssetFile* asset_open(const char* path);
bool asset_read(AssetFile* f, uint32_t offset, void* buf, uint32_t len); // all `len` bytes or false
void asset_close(AssetFile* f);
// Closes every AssetFile still open. Called when the game thread ends (idempotent).
void assets_shutdown(void);

// The writers below are NOT in the resident app (nothing resident writes a file): plugin_fs.c (write_file: UI + PC plugins) and box_fs.c (the
// other three: PC plugin) on the device, tools/host/shim_assets.c on the host. Contract 25.6.
// Writes `len` bytes to the ABSOLUTE path (e.g. "/ext/apps_data/elementis_zeta/settings.bin"): creates the
// missing parent folders, writes "<path>.tmp", then replaces `path` with it. Returns true on success.
bool write_file(const char* path, const void* data, uint32_t len);

// M5-C (PC box, contract 25): in-place helpers for box.bin and its staged copies. ABSOLUTE paths.
// write_at: writes `len` bytes at `off` of the file, creating it when it does not exist (the file grows when off + len passes the end; a gap is
// never left: callers extend a file sequentially). Not atomic: the callers work on a copy and rename it over the real file (rename_file).
bool write_at(const char* path, uint32_t off, const void* buf, uint32_t len);
// Replaces `to` with `from` (the old `to` is removed first: the firmware's rename does not overwrite). False when `from` cannot be renamed.
bool rename_file(const char* from, const char* to);
// Removes the file (true also when it did not exist).
bool remove_file(const char* path);

// Milliseconds tick (furi_get_tick on the device), to time card reads.
uint32_t asset_tick(void);

typedef struct {
    uint32_t free; // memmgr_get_free_heap
    uint32_t max_blk; // memmgr_heap_get_max_free_block
    uint32_t min_free; // memmgr_get_minimum_free_heap
} HeapStats;

void heap_stats_now(HeapStats* out);
// Formats "F87.1 M70.0 m80.0" (KB, one decimal). Returns chars written.
int heap_format(const HeapStats* h, char* out, size_t cap);

// True when a single heap block of `bytes` (plus a small safety margin) can be allocated without
// the allocator failing (the firmware heap crashes on exhaustion, so check before malloc).
bool heap_can_alloc(uint32_t bytes);
