#pragma once

/* Commit-graph builder (the reader is hub_git_commit_meta in hub_gitobj.h).
 *
 * One call is one slice: it adds new commits (reachable from the refs and not
 * in <repo>/hub/graph.bin) until the stop callback says so or HUB_GB_CAP
 * records are collected, then streams the old file and the sorted new records
 * into a temp file and renames it over graph.bin. Commits whose parents were
 * not reached yet stay in the file as "pending" ids (the walk goes on from
 * them next time), so the graph never needs more than a slice of RAM.
 * A call that adds nothing and has nothing pending writes nothing.
 *
 * Heap: about 13.6 KB of records, 2.5 KB of ids, the repo handle and one
 * commit object. Stack frames are small, nothing recurses. */

#include "hub_gitpack.h"

#define HUB_GB_CAP 200 /* new records per slice */

typedef struct {
    unsigned added; /* records written in this call */
    bool more; /* pending commits remain: call again */
    bool stopped; /* the slice ended on the stop callback */
} HubGitGraphResult;

/* Returns 0 or a HUB_GIT_ERR_* code (records collected before an error are kept). */
int hub_git_graph_update(
    void* io,
    const char* repo_dir,
    HubGitPackStop stop,
    void* ctx,
    HubGitGraphResult* res);
