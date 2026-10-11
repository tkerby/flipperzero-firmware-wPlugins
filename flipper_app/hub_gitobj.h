#pragma once

/* Git object reader for one bare repository (loose objects, packs v2 with
 * .idx v2, OFS_DELTA and REF_DELTA), plus parse helpers and ref resolution.
 *
 * Portable: all file access goes through hub_gitio.h. No SHA-1 is computed and
 * nothing is verified by hash. Every malloc failure is a clean error.
 * Nothing here recurses. Delta chains are resolved in a loop.
 *
 * Memory: objects held in RAM are limited to HUB_GIT_MAX_OBJ bytes (also the
 * limit for every intermediate buffer of a delta chain). A delta needs, at the
 * peak, the base, the delta data and the result at the same time, plus about
 * 4.5 KB for the inflate state. Non-delta objects of any size can be streamed
 * with hub_git_stream() (ring window of at most 8 KB; a stream that declares a bigger
 * window goes through hub/cache/<sha> first).
 *
 * One HubGit handle is not thread safe. Use it from one thread at a time.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef HUB_GIT_MAX_OBJ
#define HUB_GIT_MAX_OBJ (16 * 1024)
#endif
#ifndef HUB_GIT_CACHE_MAX
#define HUB_GIT_CACHE_MAX (8 * 1024 * 1024) /* hub/cache folder cap */
#endif
#define HUB_GIT_MAX_DEPTH   64 /* delta chain */
#define HUB_GIT_MAX_PARENTS 4 /* parents kept by the commit parser */
#define HUB_GIT_PATH_MAX    200 /* repo path */
#define HUB_GIT_REF_MAX     200

/* error codes (0 = ok) */
#define HUB_GIT_OK           0
#define HUB_GIT_ERR_NOTFOUND (-1)
#define HUB_GIT_ERR_TOOLARGE (-2) /* bigger than the RAM limit */
#define HUB_GIT_ERR_CORRUPT  (-3)
#define HUB_GIT_ERR_NOMEM    (-4)
#define HUB_GIT_ERR_IO       (-5)
#define HUB_GIT_ERR_BADARG   (-6)
#define HUB_GIT_ERR_WRITE    (-7) /* the stream sink returned false */
#define HUB_GIT_ERR_TOOMANY \
    (-8) /* more packs than the reader keeps and the object was not in the first ones: consolidate (hub_git_pack_merge) */
#define HUB_GIT_ERR_FAR \
    (-9) /* internal: the stream needs a far window (not returned by the public calls) */

/* object types (same numbers as in the pack format) */
#define HUB_OBJ_COMMIT 1
#define HUB_OBJ_TREE   2
#define HUB_OBJ_BLOB   3
#define HUB_OBJ_TAG    4

typedef struct {
    uint8_t b[20];
} HubOid;

typedef struct HubGit HubGit;

/* Open the bare repo at repo_dir (for example HUB_GIT_ROOT "/x.git"). io is the
 * platform handle for hub_gitio (Storage* on the Flipper, NULL on the host).
 * NULL when it is not a repo, the path is too long, or memory is short. */
HubGit* hub_git_open(void* io, const char* repo_dir);
void hub_git_close(HubGit* g);
/* Change the RAM limit of this handle (default HUB_GIT_MAX_OBJ). */
void hub_git_set_limit(HubGit* g, size_t max_obj);
/* Cap of <repo>/hub/cache in bytes (default HUB_GIT_CACHE_MAX). Older files are
 * deleted when it is exceeded. The folder can also be deleted at any time. */
void hub_git_set_cache_limit(HubGit* g, uint64_t bytes);
size_t hub_git_limit(const HubGit* g);

bool hub_oid_from_hex(const char* hex, HubOid* oid); /* exactly 40 hex digits */
void hub_oid_to_hex(const HubOid* oid, char out[41]);

/* Type and size of an object (the size after delta resolution) without
 * reading its data. No size limit applies. */
int hub_git_info(HubGit* g, const HubOid* oid, uint8_t* type, size_t* size);

/* Read a whole object into a malloc'd buffer (size + 1 bytes, the last one is
 * 0). The caller frees it with hub_git_free(). TOOLARGE when size > limit. */
int hub_git_read(HubGit* g, const HubOid* oid, uint8_t* type, uint8_t** data, size_t* size);
void hub_git_free(void* p);

/* Send the content of an object to a sink in chunks. Works for every object,
 * whatever its size. Non-delta objects are inflated straight to the sink (32 KB
 * window). Deltas that fit the RAM limit are resolved in RAM. Bigger ones are
 * resolved through temp files in <repo>/hub/tmp (about 38 KB of heap), the
 * result is cached as a plain file <repo>/hub/cache/<40hex> and later requests
 * stream that file. Temp files are removed on success and on error, and stale
 * ones when a handle opens. Errors: NOMEM, IO (for example SD full), CORRUPT.
 * type and size are set before the first sink call.
 * Return false from the sink to stop (result WRITE). */
typedef bool (*HubGitSink)(void* ctx, const uint8_t* data, size_t len);
int hub_git_stream(
    HubGit* g,
    const HubOid* oid,
    HubGitSink sink,
    void* ctx,
    uint8_t* type,
    size_t* size);

/* ---------- refs ---------- */

/* Read one ref by full name ("HEAD", "refs/heads/main"). Loose file first,
 * then packed-refs. Symbolic refs are followed. Not peeled. */
int hub_git_ref_read(HubGit* g, const char* name, HubOid* oid);
/* Follow annotated tags until the object is not a tag. */
int hub_git_peel(HubGit* g, HubOid* oid);
/* Resolve like the doc says: 40-hex, HEAD, refs/..., refs/heads/x, refs/tags/x.
 * The result is peeled to the commit. */
int hub_git_resolve(HubGit* g, const char* ref, HubOid* oid);
/* The tree of a commit / tag / tree object. */
int hub_git_tree_of(HubGit* g, const HubOid* oid, HubOid* tree);

/* ---------- parsing (pointers point into the data buffer) ---------- */

typedef struct {
    const char* name;
    size_t name_len;
    const char* email;
    size_t email_len;
    int64_t time;
    char tz[8]; /* "+0200", 0-terminated */
} HubGitIdent;

typedef struct {
    HubOid tree;
    HubOid parent[HUB_GIT_MAX_PARENTS];
    unsigned nparents; /* the real count, may be more than HUB_GIT_MAX_PARENTS */
    HubGitIdent author;
    HubGitIdent committer;
    const char* msg;
    size_t msg_len;
} HubGitCommit;

typedef struct {
    HubOid object;
    uint8_t type; /* HUB_OBJ_* of the target, 0 if unknown */
    const char* tag;
    size_t tag_len;
    bool has_tagger;
    HubGitIdent tagger;
    const char* msg;
    size_t msg_len;
} HubGitTag;

int hub_git_parse_commit(const uint8_t* data, size_t len, HubGitCommit* c);
int hub_git_parse_tag(const uint8_t* data, size_t len, HubGitTag* t);

/* Tree entries: iterate in place, no copy. */
typedef struct {
    const uint8_t* p;
    const uint8_t* end;
} HubTreeIter;

typedef struct {
    uint32_t mode; /* octal value, for example 0100644 */
    const char* name; /* 0-terminated, inside the tree data */
    size_t name_len;
    const HubOid* oid; /* inside the tree data */
} HubTreeEntry;

void hub_tree_iter_init(HubTreeIter* it, const uint8_t* data, size_t len);
/* 1 = entry returned, 0 = end, HUB_GIT_ERR_CORRUPT = bad tree */
int hub_tree_next(HubTreeIter* it, HubTreeEntry* e);
/* "tree", "blob", "link" (120000) or "commit" (160000) */
const char* hub_tree_mode_type(uint32_t mode);

/* Walk the entries of a tree object once, in git order, without holding the
 * tree or a list of entries. A tree that fits the RAM limit is read whole; a
 * bigger one is inflated through a small state machine (mode digits, name up
 * to the NUL, 20-byte id), so there is no size limit. name is 0-terminated and
 * valid during the callback only. Return false from cb to stop (result WRITE).
 * Needs about 300 bytes for the parser, plus the tree or the inflate window. */
typedef bool (
    *HubTreeCb)(void* ctx, uint32_t mode, const char* name, size_t name_len, const HubOid* oid);
int hub_git_tree_walk(HubGit* g, const HubOid* tree, HubTreeCb cb, void* ctx);

/* Find a path ("a/b/c.txt", "" = root) below a tree. Reads one tree at a time.
 * mode of the root is 040000. NOTFOUND when a part is missing or a non-tree is
 * in the way. */
int hub_git_tree_find(HubGit* g, const HubOid* tree, const char* path, HubOid* oid, uint32_t* mode);

/* ---------- commit graph (<repo>/hub/graph.bin) ----------
 * Cache of parents and committer time of commits, so walks need no object
 * reads. File: header 16 bytes (magic "HGRF", version 1, count, npend, all
 * big endian), fanout 256 * u32 (cumulative count by first id byte), then
 * count records of HUB_GRAPH_REC bytes sorted by id, then npend ids of
 * commits that are still to be added (written by hub_git_graph_update only).
 * Record: id[20], time u32, parent1[20], parent2[20] (zeros if none),
 * flags u32: bit 0 = more than 2 parents, bits 8..15 = parent count (max 255).
 * A missing or damaged file is ignored: everything works, only slower. */
#define HUB_GRAPH_REC  68
#define HUB_GRAPH_HDR  16
#define HUB_GRAPH_RECS (HUB_GRAPH_HDR + 1024) /* offset of the first record */

typedef struct {
    HubOid parent[HUB_GIT_MAX_PARENTS];
    unsigned nparents; /* real count (parent[] holds the first HUB_GIT_MAX_PARENTS) */
    uint32_t time; /* committer time */
    bool cached; /* the id is in the graph */
} HubCommitMeta;

/* Parents and committer time of a commit. From the graph when it is there,
 * else from the object (the repo is then reported as stale to
 * hub_gitobj_stale). Commits with more than 2 parents are always read from
 * the object. */
int hub_git_commit_meta(HubGit* g, const HubOid* oid, HubCommitMeta* out);
/* Do not report this handle as stale (for the graph builder itself). */
void hub_git_graph_ignore(HubGit* g);
/* Called by hub_git_close with the repo path when a lookup missed the graph. */
extern void (*hub_gitobj_stale)(const char* repo_dir);

/* Optional hook of the host: called during every file read of the reader
 * (cheap). It may update a heartbeat and answer PINGs; it returns false to
 * abort the work (the call then fails with an error code). NULL = none. */
extern bool (*hub_gitobj_beat)(void);
