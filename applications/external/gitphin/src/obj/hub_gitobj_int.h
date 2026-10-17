#pragma once

/* Object reader: limits, the handle and the pack entry types.
 * Internal to hub_gitobj.c and its parts; not for other modules. */

#define MAX_PACKS  64
#define IDX_HDR    8
#define IDX_FANOUT 1024
#define IDX_TABLE  (IDX_HDR + IDX_FANOUT)
#define HEAD_PEEK  32 /* bytes of a loose object to inflate for its header */
#define PEEL_MAX   8
#define SYMREF_MAX 6

typedef struct {
    char name[46]; /* "pack-" + 40 hex */
    uint8_t state; /* 0 not checked, 1 ok, 2 bad */
    uint32_t nobj;
} IdxEnt;

struct HubGit {
    void* io;
    size_t max_obj;
    uint64_t cache_max;

    IdxEnt* idx;
    unsigned nidx;
    unsigned last_pack;
    bool idx_loaded;
    bool too_many; /* more than MAX_PACKS idx files exist */

    HubGitFile* pack_f;
    int pack_i;

    HubGitFile* gf; /* graph.bin */
    uint8_t* fan; /* its fanout table (1 KB), NULL if there was no memory */
    uint32_t gcount;
    uint8_t gstate; /* 0 not tried, 1 open, 2 none */
    bool stale, quiet;

    char base[HUB_GIT_PATH_MAX + 1];
    char path[HUB_GIT_PATH_MAX + HUB_GIT_REF_MAX + 64];
    char refname[HUB_GIT_REF_MAX + 8];
    char cand[HUB_GIT_REF_MAX + 16];
    char line[256];
    uint8_t buf[256];
};

typedef struct {
    int pack; /* -1 = loose object */
    uint64_t off;
    HubOid oid; /* used for loose objects */
} Loc;

typedef struct {
    uint8_t type; /* 1..4 plain, 6 ofs-delta, 7 ref-delta */
    uint32_t size;
    uint64_t data_off;
    uint64_t base_off;
    HubOid base_oid;
} PackHdr;

typedef struct {
    int pack;
    uint32_t dsize;
    uint64_t data_off;
} ChainEnt;

typedef struct {
    ChainEnt* v;
    unsigned n, cap;
} Chain;
