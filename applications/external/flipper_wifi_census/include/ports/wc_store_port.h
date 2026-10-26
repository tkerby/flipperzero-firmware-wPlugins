#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Called once per stored file name during a listing. `name` is valid only for the call.
typedef void (*WcStoreNameFn)(void *ctx, const char *name);

// Opaque streaming writer, so a large file is written in small chunks without ever holding
// the whole serialized blob in RAM. Defined by the adapter.
typedef struct WcFileWriter WcFileWriter;

// Port: persistent storage for capture files and the known-devices db. Names are bare file
// names within the app's data directory (no paths); the adapter maps them under
// APP_DATA_PATH and enforces that. Adapter (platform/) wraps furi Storage; tests use an
// in-memory fake.
typedef struct {
    void *self;
    bool (*write_file)(void *self, const char *name, const uint8_t *data, size_t len);
    // Reads up to `cap` bytes; returns bytes read, or 0 on any failure (missing/too big).
    size_t (*read_file)(void *self, const char *name, uint8_t *buf, size_t cap);
    // Size of a stored file in bytes, or 0 if missing (lets a caller size a read buffer).
    size_t (*file_size)(void *self, const char *name);
    // Size a file by ABSOLUTE path (a pcap the user picked anywhere on the SD via the file
    // browser). No app-dir prefix is applied — the path is used verbatim.
    size_t (*file_size_path)(void *self, const char *path);
    // Read up to `cap` bytes from `offset`; 0 at end of file or on error. How a big file is
    // consumed without being held whole, which a FAP cannot count on doing.
    size_t (*read_range)(void *self, const char *name, size_t offset, uint8_t *buf, size_t cap);
    size_t (*read_range_path)(void *self, const char *path, size_t offset, uint8_t *buf,
                              size_t cap);

    bool (*rename_file)(void *self, const char *from, const char *to);
    bool (*delete_file)(void *self, const char *name);
    uint16_t (*list)(void *self, WcStoreNameFn cb, void *ctx);

    // Streaming write: open (truncate/create) -> write chunks -> close. `open_write` returns
    // NULL on failure; `write` returns false on a short/failed write; `close` frees the writer
    // and returns whether the file closed cleanly.
    WcFileWriter *(*open_write)(void *self, const char *name);
    bool (*write)(WcFileWriter *w, const uint8_t *data, size_t len);
    bool (*close)(WcFileWriter *w);
} WcStorePort;
