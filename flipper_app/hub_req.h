#pragma once

/* Internal: the request state and the helpers that route files share.
 * Implemented in hub_http.c. */

#include "hub_http.h"

#include <furi.h>
#include <storage/storage.h>

#define HUB_HEAD_MAX     4096
#define HUB_IO_CHUNK     512
#define HUB_PATH_MAX     256
#define HUB_READ_TIMEOUT 5000

typedef struct {
    HubTransport* t;
    Storage* storage;

    uint8_t head[HUB_HEAD_MAX + 1];
    size_t head_len;

    const char* method;
    const char* path;
    const char* query;

    /* headers the WebDAV methods need (point into head, NULL when absent) */
    const char* h_depth;
    const char* h_destination;
    const char* h_overwrite;
    const char* h_lock_token;
    const char* h_if;
    const char* h_expect;
    const char* h_if_match;
    const char* h_if_none_match;
    const char* h_accept_enc;

    size_t content_length;
    size_t body_read;
    const uint8_t* pre; /* body bytes that came in with the head */
    size_t pre_len;

    bool head_sent;
    uint8_t io[HUB_IO_CHUNK];
} HubReq;

/* Find key in a query string and percent-decode its value into out. */
bool hub_query_get(const char* query, const char* key, char* out, size_t out_size);
/* Only the SD card and internal storage, and no way out of them. */
bool hub_path_ok(const char* path);
/* Append v to s as a quoted JSON string. */
void hub_json_str(FuriString* s, const char* v);
/* Same for bytes of any content (invalid UTF-8 becomes U+FFFD). */
void hub_json_bytes(FuriString* s, const char* v, size_t n);
/* 503 "low memory" unless the heap has need bytes free (and a block of 3/4 of it). */
bool hub_heap_ok(HubReq* r, size_t need);
#define HUB_HEAP_RESERVE (8 * 1024) /* the system (GUI, storage, USB) always keeps this much */

/* SD card space: false (and 507 "SD card full" sent) when less than 16 MB
 * would be left after writing `incoming` more bytes. */
bool hub_sd_ok(HubReq* r, uint64_t incoming);
bool hub_sd_free_ok(Storage* storage, uint64_t incoming); /* same, no answer */

/* Worker heartbeat: call it inside long loops (at least every few seconds).
 * It also answers PING frames of the bridge while a request is busy. The GUI
 * shows "Worker stuck" when the beat is older than 60 s. */
extern volatile uint32_t hub_beat_tick;
/* What the worker is doing, for the screen. */
#define HUB_ACT_IDLE    0
#define HUB_ACT_REQUEST 1
#define HUB_ACT_PACK    2
extern volatile uint8_t hub_activity;
extern volatile bool hub_long_op; /* a route that may be silent for a long time */
void hub_beat(void);

/* Transfer progress for the screen. The worker is the only writer; the GUI reads
 * the 32-bit fields without a lock (aligned word reads are atomic). A session
 * is a run of requests to one repo with gaps under 3 s. done/total belong to
 * the current request (total 0 = unknown); objects/bytes to the session. */
#define HUB_PG_NONE 0
#define HUB_PG_PUSH 1 /* WebDAV PUT/MOVE/... under /git/ */
#define HUB_PG_PULL 2 /* dumb HTTP GET under /git/ */
#define HUB_PG_BIG  3 /* one request over 16 KB with a known length */
typedef struct {
    volatile uint8_t kind;
    volatile uint32_t done, total, objects, bytes, start, last, repo;
    /* pull session: size of all packs of the repo (0 = unknown) and bytes of
     * finished answers, so the screen can show the whole clone, not one file */
    volatile uint32_t sess_total, sess_done;
} HubProgress;
extern HubProgress hub_prog;

/* Read up to len bytes of the request body. Returns 0 at the end. */
size_t hub_body_read(HubReq* r, uint8_t* buf, size_t len);

bool hub_send_head(HubReq* r, int code, const char* ctype, size_t len, const char* extra);
#define HUB_CHUNKED    ((size_t) - 1) /* len for hub_send_head: Transfer-Encoding: chunked */
#define HUB_CHUNK_SIZE 512
/* Chunked writer: pieces go out in chunks of at most 512 bytes. The caller keeps
 * the struct (about 540 bytes) on the heap or in its own state. */
typedef struct {
    HubReq* r;
    size_t n;
    bool ok;
    char b[8 + HUB_CHUNK_SIZE + 2];
} HubChunk;
bool hub_chunk_begin(HubChunk* c, HubReq* r, const char* ctype, const char* extra);
void hub_chunk_write(HubChunk* c, const char* d, size_t n);
/* A string literal: the length cannot be wrong. */
#define hub_chunk_lit(c, s) hub_chunk_write((c), "" s, sizeof(s) - 1)
void hub_chunk_json(HubChunk* c, const char* v, size_t n);
void hub_chunk_fs(HubChunk* c, FuriString* s);
bool hub_chunk_end(HubChunk* c, bool good);
/* Cache-Control line for answers addressed by a full 40-hex id (never change). */
const char* hub_cache_hdr(const char* ref);

void hub_send_body(HubReq* r, int code, const char* ctype, const char* body, size_t len);
void hub_send_json(HubReq* r, int code, FuriString* json);
void hub_send_error(HubReq* r, int code, const char* msg);
void hub_send_ok(HubReq* r);

/* Send a file; the type comes from the extension. Sends the error itself. */
void hub_serve_file(HubReq* r, const char* path, bool download);
/* Same, with the content type given by the caller. */
void hub_serve_file_as(HubReq* r, const char* path, const char* ctype, bool download);

/* Deadline for a long route: after it, the object reader aborts and the
 * answer becomes 503 "too slow on the device". */
void hub_deadline_start(uint32_t ms);
void hub_deadline_clear(void);
bool hub_deadline_expired(void);
bool hub_beat_hook(void); /* for hub_gitobj_beat: beat, false when the deadline passed */
