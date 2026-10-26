#pragma once

/* Pure helpers of the Git document store (hub_gitdoc.c). No Flipper headers,
 * so they build and test on the host (pc/tests/gitdoc_test.c). */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define HUB_GITDOC_KEY_MAX  160
#define HUB_GITDOC_BODY_MAX (64 * 1024)

/* Document key: segments of [A-Za-z0-9._-] separated by one "/", no empty
 * segment, no ".." anywhere, no segment that starts with ".", no segment that
 * ends with ".part" (that suffix is the temp file), at most 160 chars. */
bool hub_gitdoc_key_ok(const char* key);
/* "" (the doc root), or a valid key, optionally followed by one "/". */
bool hub_gitdoc_prefix_ok(const char* prefix);

/* CRC-32 (zlib / IEEE). Start with crc = 0. Same value as python zlib.crc32. */
uint32_t hub_crc32(uint32_t crc, const void* data, size_t len);
/* ETag text including the quotes, for example "\"cbf43926\"". out >= 11 bytes. */
void hub_gitdoc_etag(uint32_t crc, char out[11]);

/* Preconditions of a write. if_match / if_none_match are the raw header values
 * (NULL when the header is absent).
 *   If-None-Match: *        create only: the document must not exist
 *   If-Match: "<etag>"      replace: the document must exist with this ETag
 *                           (a list "a", "b" is allowed; "*" = any existing;
 *                           W/ tags never match: strong comparison)
 * need_match = true (delete) accepts only If-Match.
 * Returns 0 when the write may go on, 412 when the document changed, 400 when
 * no usable precondition was sent. */
int hub_gitdoc_precond(
    const char* if_match,
    const char* if_none_match,
    bool need_match,
    bool exists,
    uint32_t cur_crc);

/* Header scan. line = "Name: value". Returns the value (leading blanks
 * skipped) when the name matches (case-insensitive), else NULL. */
const char* hub_gitdoc_hdr(const char* line, const char* name);
/* Walk the request head after hub_http.c parsed it (lines are NUL separated,
 * a '\n' may follow each NUL). head..head+len is the text before the blank
 * line. Sets the two header values (pointers into head) or leaves them. */
void hub_gitdoc_scan_head(
    const char* head,
    size_t len,
    const char** if_match,
    const char** if_none_match);
