#pragma once

/* Packer: limits and the pack job types.
 * Internal to hub_gitpack.c and its parts; not for other modules. */

#define PATHLEN (HUB_GIT_PATH_MAX + 50)
#define IDX_BUF 512

typedef struct {
    HubGitFile* f;
    uint32_t crc;
    uint32_t total;
    bool err;
} PackOut;

typedef struct {
    PackOut po;
    uint8_t type;
    size_t size;
    HubDeflate* df; /* compressor of a big object */
    PackOut so; /* scratch file of a big object (tmp_pack_o) */
    uint64_t limit; /* the scratch file may not grow past this: the object is left loose */
    bool nomem, over;
    char pa[PATHLEN], pb[PATHLEN], pc[PATHLEN];
    uint8_t buf[512];
} Run;

/* State of the open pack between slices. The objects stay loose (and readable)
 * until the pack is finished, so dropping this (a restart) loses only time:
 * the next slice deletes the tmp_pack_* files and starts again. */
#define JOB_MAX 384 /* finish the pack when it holds this many objects */
#define TAB_REC 28 /* tmp_pack_t record: id[20], offset u32, crc u32 */
typedef struct {
    uint8_t phase; /* 1 append, 2 hash, 3 index, 4 delete loose */
    bool patched;
    int16_t last_dir;
    uint32_t count, total, hpos, dpos;
    HubSha1 sha;
    uint8_t sum[20];
    char name[46];
} PackJob;
