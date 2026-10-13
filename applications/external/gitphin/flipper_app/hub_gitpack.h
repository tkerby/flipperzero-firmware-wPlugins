#pragma once

/* Pack the loose objects of a bare repo into one pack + idx (no deltas).
 *
 * Why: git http-push stores every object loose and a dumb clone then needs one
 * GET per object. One pack is one GET.
 *
 * One call packs loose objects into a NEW pack (objects/pack/pack-<sha>.pack
 * and .idx) until max_objects is reached, the stop callback returns true, or
 * there is nothing left. Objects packed so far are KEPT when it stops early.
 * The loose files that went into the pack are deleted, and so are the empty
 * objects/xx folders. Afterwards the caller regenerates info/refs and
 * objects/info/packs.
 *
 * Time: the stop callback is polled before each object, so a deadline is just
 *     bool stop(void* ctx) { return furi_get_tick() > deadline; }
 * One object is never interrupted, so a call lasts at most the deadline plus
 * the time of one object. A call that stops before the first object writes
 * nothing (packed = 0, stopped = true, return value 0).
 *
 * Cursor: a zero-initialised HubGitPackCursor remembers the next objects/xx
 * folder to look at, so a job of many calls scans every folder only once. Make
 * a new one (memset 0) for every new job. NULL scans from folder 00 each time.
 *
 * Crash safety: the pack file is written under a tmp_pack_* name, its header
 * count and trailer are finished, then pack and idx are renamed (pack first).
 * Loose files are deleted only after both renames. A crash leaves at worst
 * duplicate objects (legal) and stale tmp_pack_* files that the next call
 * removes. A real error (IO, NOMEM, corrupt object) abandons the batch and
 * changes nothing.
 *
 * Heap, measured on the host: objects up to ram_limit are read into RAM and
 * compressed (the data buffer + 8 KB compressor; the inflate state is freed
 * before). Bigger objects are streamed (35 KB inflate window and state) and
 * stored as uncompressed deflate blocks. Add 30 bytes per object of the batch,
 * the handle (2 KB) and 1 KB of paths. About 40 KB with max_objects = 50.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define HUB_GITPACK_MAX_OBJS 512 /* hard cap per call */
#define HUB_GITPACK_STOPPED  (-100) /* not returned any more; kept for old code */

typedef struct {
    uint16_t dir; /* next objects/xx folder (0..256); zero it to start a job */
    void* job; /* the open tmp pack between slices (malloc'd by the packer, free() it to drop) */
} HubGitPackCursor;

typedef struct {
    unsigned packed; /* objects written to the new pack */
    bool more; /* there may be more loose objects: call again */
    unsigned remaining; /* loose objects seen in this call and not packed (a lower bound) */
    uint32_t pack_bytes;
    bool stopped;
    bool finished; /* the pack was finished in this call (name is set) */
    char name[46]; /* "pack-<40 hex>", empty until the pack is finished */
} HubGitPackResult;

/* progress(ctx, done, total) after each object. Both callbacks may be NULL.
 * Loop: while(res.more && res.packed > 0 && !time_is_up). */
typedef void (*HubGitPackProgress)(void* ctx, unsigned done, unsigned total);
typedef bool (*HubGitPackStop)(void* ctx);

/* io: platform handle of hub_gitio (Storage* on the Flipper, NULL on the host).
 * Returns 0 or a HUB_GIT_ERR_* code. */
int hub_git_pack_loose(
    void* io,
    const char* repo_dir,
    unsigned max_objects,
    size_t
        ram_limit, /* objects up to this size are compressed, bigger ones stored; 0 = HUB_GIT_MAX_OBJ */
    HubGitPackCursor* cursor, /* may be NULL */
    HubGitPackProgress progress,
    HubGitPackStop stop,
    void* ctx,
    HubGitPackResult* res);

/* ---------- consolidation: merge small packs into one ----------
 *
 * Idle packing makes one small pack per slice. Merging copies the entry bytes
 * of up to 16 small packs RAW (no inflate, no recompression) into one new pack
 * and builds its idx by a streamed k-way merge of the source idx files. Then
 * the source packs are deleted (idx first).
 *
 * Trigger (caller): hub_git_pack_stats() says nsmall >= 8, say.
 *
 * Time slices: call hub_git_pack_merge in a loop with the same cursor while
 * res.more. The copy phase polls stop() every 1 KB and resumes where it was
 * (the temp file tmp_merge_p stays on disk, the SHA-1 state is in the cursor).
 * The last two calls are not interruptible: the idx build (about 4 sequential
 * passes over the source idx files, time grows with the object count) and the
 * commit. If the app restarts, the cursor is lost: the next merge deletes
 * tmp_merge_* and starts again from the beginning.
 *
 * Duplicate ids across sources would make the pack header count differ from the
 * idx (git verify-pack rejects that), so a source that shares an id with an
 * earlier source is left out of the merge (it stays a separate pack).
 *
 * Crash safety: new pack, then new idx are renamed; only then are the sources
 * deleted. At every point the repo is readable (duplicates across packs are
 * legal).
 *
 * Heap: about 7 KB (merge state 4.2 KB, 1 KB copy buffer, paths). The cursor is
 * about 1.1 KB: allocate it, do not put it on the 4 KB worker stack.
 * Bytes below 2 GB only; sources with 8-byte offsets are reported CORRUPT.
 */

#include "hub_sha1.h"

#define HUB_MERGE_MAX_SRC 16

typedef struct {
    uint8_t phase; /* 0 = start (zero the struct), 1 = copying, 2 = build idx */
    uint8_t nsrc;
    uint8_t cur_src;
    uint32_t cur_off; /* bytes of the current source entry area copied */
    uint32_t total; /* bytes in tmp_merge_p so far */
    uint32_t count; /* objects in the new pack */
    HubSha1 sha;
    char names[HUB_MERGE_MAX_SRC][46]; /* "pack-<40 hex>" */
    uint32_t dest[HUB_MERGE_MAX_SRC]; /* offset in the new pack of the source's byte 12 */
    uint32_t elen[HUB_MERGE_MAX_SRC]; /* entry bytes of the source: size - 32 */
} HubGitMergeCursor;

typedef struct {
    unsigned merged_packs; /* source packs merged and deleted (set in the last call) */
    unsigned objects;
    bool more; /* call again with the same cursor */
    bool stopped; /* this slice ended on the stop callback */
    uint32_t pack_bytes;
    char name[46]; /* "pack-<40 hex>" of the result, set in the last call */
} HubGitMergeResult;

/* Count the packs of a repo: total, and those with a .pack smaller than
 * small_limit (0 = 1 MB). */
int hub_git_pack_stats(
    void* io,
    const char* repo_dir,
    uint32_t small_limit,
    unsigned* total,
    unsigned* nsmall);

/* small_limit: sources must have a .pack smaller than this (0 = 1 MB).
 * max_srcs: up to this many of the smallest (0 or > 16 = 16). With fewer than
 * 2 usable sources nothing happens (more = false, merged_packs = 0).
 * A real error abandons the merge: temp files are removed, the cursor is reset,
 * the sources are untouched. */
int hub_git_pack_merge(
    void* io,
    const char* repo_dir,
    HubGitMergeCursor* cur,
    uint32_t small_limit,
    unsigned max_srcs,
    HubGitPackStop stop,
    void* ctx,
    HubGitMergeResult* res);
