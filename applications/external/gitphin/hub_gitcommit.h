#pragma once

/* Editing a repository on the Flipper (docs/git-design.md, Phase 4): commit one
 * file change, and merge two trees. Reads with hub_gitobj.h, writes loose
 * objects with hub_gitwrite.h. The ref update is NOT done here: the route does
 * the compare-and-swap with the returned commit id.
 *
 * No recursion (explicit loops and a malloc'd stack), small stack frames.
 * Heap (HubGit limit L, T = size of the trees on the path):
 *   commit_file: one tree and its rewritten copy at a time: about 2 * T_max
 *                + 1 KB (trees on the path are read twice, not kept)
 *   tree_merge:  the three trees of the current folder (3 * T) + the output
 *                of every open folder level + about 3 KB
 * A tree bigger than the limit gives HUB_GIT_ERR_TOOLARGE. */

#include "hub_gitobj.h"
#include "hub_gitwrite.h"

#define HUB_GC_ERR_CONFLICT  (-20) /* file where a folder is needed, or a folder where a file is */
#define HUB_GC_ERR_UNCHANGED (-21) /* the edit changes nothing: no commit is made */
#define HUB_GC_ERR_BADPATH   (-22)
#define HUB_GC_ERR_MERGE     (-23) /* merge conflicts, see the path callback */

#define HUB_GC_MAX_DEPTH     32
#define HUB_GC_PATH_MAX      200
#define HUB_GM_MAX_CONFLICTS 50

/* Put (content != NULL) or delete (content == NULL) one file and write a commit
 * on top of tip (NULL = first commit, empty tree).
 *   path    "a/b/c.txt": segments without "/", not empty, not ".", ".." or ".git"
 *   put     missing folders are created, an existing file keeps its mode
 *           (new files are 100644)
 *   delete  folders that become empty are removed
 * author = committer (the time comes from the caller, tz is +0000 unless set).
 * Errors: NOTFOUND (delete of a missing path), CONFLICT (path runs through a
 * file, or names a folder), UNCHANGED, BADPATH, TOOLARGE (a tree over the
 * limit), NOMEM, IO, CORRUPT. out_commit / out_tree may be NULL. */
int hub_git_commit_file(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* tip,
    const char* path,
    const void* content,
    size_t len,
    const char* message,
    const HubGwIdent* who,
    HubOid* out_commit,
    HubOid* out_tree);

/* Report a conflicting path. Return false to stop. */
typedef bool (*HubGitPathCb)(void* ctx, const char* path);

/* Entry-level three-way merge of trees: base (NULL = empty) is the merge base,
 * ours and theirs the two sides. A path changed on one side only takes that
 * side. A subfolder changed on both sides is merged by walking into it. A path
 * changed on both sides in different ways is a conflict (no content merge).
 * On success out gets the merged tree (written to the repo). With conflicts
 * the result is HUB_GC_ERR_MERGE, up to HUB_GM_MAX_CONFLICTS paths go to cb and
 * *nconf counts them; no trees are written then. nconf and cb may be NULL. */
int hub_git_tree_merge(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* base,
    const HubOid* ours,
    const HubOid* theirs,
    HubOid* out,
    HubGitPathCb cb,
    void* ctx,
    unsigned* nconf);

/* Merge commit of two commits: merges the trees (merge_base may be NULL when
 * there is none) and writes a commit with the parents ours_tip, theirs_tip in
 * this order. Same results as hub_git_tree_merge. */
int hub_git_merge_commits(
    HubGit* g,
    const HubGitWriteIo* io,
    const char* repo_dir,
    const HubOid* ours_tip,
    const HubOid* theirs_tip,
    const HubOid* merge_base,
    const char* message,
    const HubGwIdent* who,
    HubOid* out_commit,
    HubOid* out_tree,
    HubGitPathCb cb,
    void* ctx,
    unsigned* nconf);
