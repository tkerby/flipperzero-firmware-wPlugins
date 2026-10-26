#pragma once

/* Git object writer: loose objects, builders for tree and commit content.
 *
 * Objects are written as zlib streams made of stored (uncompressed) deflate
 * blocks, at most 65535 bytes each, with a correct Adler-32. The file goes to
 * "<repo>/objects/tmp_obj_<n>" first and is renamed to "objects/xx/<38 hex>"
 * when the id is known. An object that already exists is not written again.
 *
 * Portable: all file access goes through HubGitWriteIo (Flipper:
 * hub_gitwrite_furi.c, host: pc/tests/gitwrite_stdio.c). Nothing recurses,
 * stack frames are small, buffers are malloc'd. Not thread safe per writer. */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* object types (same numbers as the pack format and HUB_OBJ_* in hub_gitobj.h) */
#define HUB_GW_COMMIT 1
#define HUB_GW_TREE   2
#define HUB_GW_BLOB   3
#define HUB_GW_TAG    4

/* results (0 = ok) */
#define HUB_GW_OK         0
#define HUB_GW_ERR_IO     (-1)
#define HUB_GW_ERR_NOMEM  (-2)
#define HUB_GW_ERR_BADARG (-3)
#define HUB_GW_ERR_SIZE   (-4) /* streamed byte count differs from the size given */

#define HUB_GW_REPO_MAX 190 /* longest repo path */

typedef struct {
    void* ctx; /* passed to every function (Storage* on the Flipper) */
    bool (*exists)(void* ctx, const char* path);
    /* make one folder; true when it exists afterwards (parents already exist) */
    bool (*mkdir)(void* ctx, const char* path);
    /* create or truncate a file for writing; NULL on error */
    void* (*open)(void* ctx, const char* path);
    bool (*write)(void* ctx, void* file, const void* buf, size_t len);
    /* flush and close; false when the data may not be on disk */
    bool (*close)(void* ctx, void* file);
    bool (*rename)(void* ctx, const char* from, const char* to);
    void (*remove)(void* ctx, const char* path);
} HubGitWriteIo;

/* Flipper only (hub_gitwrite_furi.c): fill io for a Storage* (r->storage). */
void hub_gitwrite_io_furi(HubGitWriteIo* io, void* storage);

/* Write one object whose content is in memory. id gets the 20 byte object id.
 * *existed (may be NULL) tells that the object was already there. */
int hub_gitwrite_object(
    const HubGitWriteIo* io,
    const char* repo_dir,
    int type,
    const void* data,
    size_t len,
    uint8_t id[20],
    bool* existed);

/* Streamed write for content that is not in memory: the total size must be
 * known. Call update until exactly `size` bytes were given, then end.
 * end() always frees the writer. abort() drops the temp file and frees it. */
typedef struct HubGitWriter HubGitWriter;
HubGitWriter*
    hub_gitwrite_begin(const HubGitWriteIo* io, const char* repo_dir, int type, size_t size);
bool hub_gitwrite_update(HubGitWriter* w, const void* data, size_t len);
int hub_gitwrite_end(HubGitWriter* w, uint8_t id[20], bool* existed);
void hub_gitwrite_abort(HubGitWriter* w);

/* Object id of memory content, no file access. */
void hub_gitwrite_hash(int type, const void* data, size_t len, uint8_t id[20]);
/* 40 lowercase hex characters plus NUL. */
void hub_gitwrite_hex(const uint8_t id[20], char out[41]);
/* 40 hex characters (any case) to 20 bytes. false when not valid hex. */
bool hub_gitwrite_unhex(const char* hex, uint8_t id[20]);

/* ---------- tree ---------- */

#define HUB_GW_MODE_TREE   0040000
#define HUB_GW_MODE_FILE   0100644
#define HUB_GW_MODE_EXEC   0100755
#define HUB_GW_MODE_LINK   0120000
#define HUB_GW_MODE_COMMIT 0160000 /* submodule */

typedef struct {
    const char* name; /* one path segment: no "/", not empty, not ".", ".." or ".git" */
    uint32_t mode; /* HUB_GW_MODE_* */
    uint8_t id[20];
} HubGitTreeEnt;

/* Parse "100644" / "40000" / "040000" (octal text) to a mode. 0 when unknown. */
uint32_t hub_gitwrite_mode_parse(const char* s);

/* Sort the entries in place (git order: names compare as if trees end with
 * "/") and return the tree content (malloc'd, free() it). NULL on a bad entry,
 * a duplicate name or no memory. n = 0 gives an empty tree (len = 0, non-NULL). */
uint8_t* hub_gitwrite_tree_build(HubGitTreeEnt* ents, size_t n, size_t* len);

/* git's tree order for two entry names (a tree counts as if its name ended
 * with "/"): < 0, 0 or > 0. Names are given with their length. */
int hub_gitwrite_ent_cmp(
    const char* an,
    size_t al,
    bool a_tree,
    const char* bn,
    size_t bl,
    bool b_tree);
/* Append one raw tree entry "<octal mode> <name>\0<20 bytes>" at out.
 * Returns the bytes written (at most 8 + nlen + 21). */
size_t hub_gitwrite_ent_put(
    uint8_t* out,
    uint32_t mode,
    const char* name,
    size_t nlen,
    const uint8_t id[20]);

/* ---------- commit ---------- */

typedef struct {
    const char* name;
    const char* email;
    int64_t time; /* Unix seconds, not negative */
    const char* tz; /* "+0000" style; NULL means "+0000" */
} HubGwIdent;

/* Commit content (malloc'd, free() it). parents is an array of n ids (n may be
 * 0). "<>" and line breaks are taken out of name and email. A missing newline
 * at the end of a non-empty message is added. NULL on a bad argument (empty
 * name, bad tz) or no memory. */
char* hub_gitwrite_commit_build(
    const uint8_t tree[20],
    const uint8_t (*parents)[20],
    size_t nparents,
    const HubGwIdent* author,
    const HubGwIdent* committer,
    const char* message,
    size_t* len);
