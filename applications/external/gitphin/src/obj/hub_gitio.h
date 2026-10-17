#pragma once

/* Tiny read-only file I/O layer for the Git object reader.
 * Two implementations:
 *   flipper_app/hub_gitio_furi.c   Flipper (Storage API)
 *   pc/tests/gitio_stdio.c         host (stdio / dirent)
 * The `io` argument is the platform handle: a Storage* on the Flipper,
 * ignored (may be NULL) on the host. */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct HubGitFile HubGitFile;
typedef struct HubGitDir HubGitDir;

/* Open a file for reading. NULL when it does not exist or cannot be opened. */
HubGitFile* hub_gitio_open(void* io, const char* path);
void hub_gitio_close(HubGitFile* f);
uint64_t hub_gitio_size(HubGitFile* f);
/* Absolute position from the start of the file. */
bool hub_gitio_seek(HubGitFile* f, uint64_t offset);
/* Read up to len bytes at the current position. 0 at the end or on error. */
size_t hub_gitio_read(HubGitFile* f, void* buf, size_t len);

/* Directory listing. Names only (no path), "." and ".." are never returned. */
HubGitDir* hub_gitio_dir_open(void* io, const char* path);
bool hub_gitio_dir_read(HubGitDir* d, char* name, size_t cap, bool* is_dir);
void hub_gitio_dir_close(HubGitDir* d);

/* Directory listing with the size of each file (size may be NULL). */
bool hub_gitio_dir_read_ex(HubGitDir* d, char* name, size_t cap, bool* is_dir, uint64_t* size);

/* ---- write side (only used by the spill / cache path) ---- */

/* Create (or truncate) a file for reading and writing (seek, then read or
 * write works). NULL on failure. */
HubGitFile* hub_gitio_create(void* io, const char* path);
/* Returns the number of bytes written (len on success). */
size_t hub_gitio_write(HubGitFile* f, const void* buf, size_t len);
/* Make a folder. True when it was made or already exists. */
bool hub_gitio_mkdir(void* io, const char* path);
/* Rename, replacing an existing target. */
bool hub_gitio_rename(void* io, const char* from, const char* to);
/* Delete a file. True when the file is gone afterwards (also if it never existed). */
bool hub_gitio_remove(void* io, const char* path);

/* Open an existing file for reading and writing (seek, then read or write). NULL if missing. */
HubGitFile* hub_gitio_open_rw(void* io, const char* path);
