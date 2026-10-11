#pragma once

/* Search a tree: file names and file contents (docs/git-design.md, Phase 4).
 * Iterative walk (no recursion), contents are scanned as a stream with
 * hub_git_stream(), so blobs of any size work. Matching is a case-insensitive
 * (ASCII) substring search. Binary files (a NUL in the first 8000 bytes) are
 * not scanned. The matcher works on the byte stream: only the first
 * HUB_GS_TEXT_MAX bytes of a line are kept for the result, but a match is found
 * anywhere in the line.
 *
 * Heap: one tree at a time (up to the HubGit limit) + about 9 KB (8000 byte
 * head buffer, line buffer, stack) + what hub_git_stream() needs for the blob
 * (32 KB inflate window for big non-delta blobs). */

#include "hub_gitobj.h"

#define HUB_GS_QUERY_MAX    100
#define HUB_GS_TEXT_MAX     200
#define HUB_GS_BINARY_PROBE 8000
#define HUB_GS_MAX_DEPTH    40
#define HUB_GS_PATH_MAX     300

typedef struct {
    const char* path; /* valid during the callback only */
    uint32_t line; /* 1-based; 0 = the file name matched */
    const char* text; /* the line without "\n" (cut to HUB_GS_TEXT_MAX), "" for a name hit */
    size_t text_len;
    bool cut; /* the line was longer than text_len */
} HubGitHit;

typedef bool (*HubGitHitCb)(void* ctx, const HubGitHit* hit); /* false = stop */
typedef bool (*HubGitStopCb)(void* ctx); /* true = stop (time budget) */

typedef struct {
    /* in */
    const char* query; /* 1..HUB_GS_QUERY_MAX bytes */
    size_t max_hits; /* 0 = no limit */
    size_t max_blob; /* blobs bigger than this are not scanned (0 = no limit) */
    bool no_names; /* set to turn name matching off */
    bool no_content; /* set to turn content matching off */
    HubGitHitCb on_hit;
    void* hit_ctx;
    HubGitStopCb should_stop; /* may be NULL; asked before each blob and each chunk */
    void* stop_ctx;
    /* out */
    size_t hits;
    uint32_t scanned; /* blobs whose content was read */
    uint32_t skipped; /* regular files not scanned (too big) */
    uint32_t binary; /* blobs found to be binary */
    bool truncated; /* stopped early: max_hits, budget or the callback */
    bool timed_out; /* should_stop said so */
} HubGitSearch;

/* Search below a tree. Returns 0 or a HUB_GIT_ERR_* code. A stop (max_hits,
 * budget) is not an error: see truncated. Fill s->query and the callbacks, the
 * rest may stay zero (memset the struct first). */
int hub_git_search(HubGit* g, const HubOid* tree, HubGitSearch* s);
