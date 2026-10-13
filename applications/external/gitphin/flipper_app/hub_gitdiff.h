#pragma once

/* Tree diff and branch compare on top of hub_gitobj.h (docs/git-design.md,
 * Phase 3). Portable, no recursion, every buffer is malloc'd.
 *
 * Heap (each reads at most one tree per side at a time; HubGit limit L):
 *   diff:    up to 2 * L (the two trees being compared) + about 3 KB
 *   compare: about 21 * cap + 4 * cap bytes (default cap 2000: 50 KB) plus one
 *            commit object at a time (a few hundred bytes) */

#include "hub_gitobj.h"

#define HUB_GD_MAX_FILES 300
#define HUB_GD_MAX_DEPTH 40
#define HUB_GD_PATH_MAX  300

typedef struct {
    const char* path; /* valid during the callback only */
    char status; /* 'A', 'D', 'M' or 'T' */
    HubOid old_oid, new_oid; /* all zero when absent */
    uint32_t old_mode, new_mode; /* 0 when absent */
} HubGitDiffFile;

/* Return false to stop the walk (result HUB_GIT_ERR_WRITE). */
typedef bool (*HubGitDiffCb)(void* ctx, const HubGitDiffFile* f);

/* Files that differ between two trees (either may be NULL = empty tree), in
 * git tree order, folders expanded in place. Equal subtrees are skipped,
 * trees themselves are not reported, no rename detection. 'T' = the kind of
 * entry changed under the same path (file, symlink, gitlink). A file that
 * becomes a folder (or the reverse) is a D plus A entries.
 * After max_files callbacks (0 = HUB_GD_MAX_FILES) the walk stops and
 * *truncated is set when more differences exist. */
int hub_git_diff_trees(
    HubGit* g,
    const HubOid* a,
    const HubOid* b,
    HubGitDiffCb cb,
    void* ctx,
    size_t max_files,
    bool* truncated);

/* ---------- compare ---------- */

#define HUB_GD_AHEAD_MAX   50
#define HUB_GD_COMPARE_CAP 2000

typedef struct {
    size_t cap; /* in: ids kept in RAM (0 = HUB_GD_COMPARE_CAP) */
    bool has_merge_base;
    HubOid merge_base;
    bool ff; /* base is an ancestor of head */
    bool capped; /* the cap was reached: counts are lower bounds */
    uint32_t ahead_count; /* commits in base..head */
    uint32_t behind_count; /* commits in head..base */
    unsigned n_ahead; /* entries in ahead[] */
    HubOid ahead[HUB_GD_AHEAD_MAX]; /* newest first (committer time) */
} HubGitCompare;

/* Both ids must be commits. The merge base is a best common ancestor; with
 * several (criss-cross merges) the one with the newest committer time. */
int hub_git_compare(HubGit* g, const HubOid* base, const HubOid* head, HubGitCompare* out);
