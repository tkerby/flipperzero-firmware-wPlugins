#pragma once

/* Internal: a cursor over the entries of one tree object (used by
 * hub_gitdiff.c and hub_gitcommit.c). The tree is read whole into RAM (one
 * malloc, up to the HubGit limit). The cursor position is a byte offset, so a
 * walk can be suspended (the buffer freed) and resumed later. */

#include "hub_gitobj.h"
#include "hub_gitwrite.h"

typedef struct {
    uint8_t* buf; /* NULL for the empty tree */
    size_t len;
    size_t pos; /* start of the head entry */
    size_t next; /* start of the entry after the head */
    bool valid; /* head entry parsed */
    HubTreeEntry e;
} HubTreeCur;

static inline bool cur_is_tree(const HubTreeCur* c) {
    return c->e.mode == 0040000;
}

static inline void cur_load(HubTreeCur* c) {
    c->valid = false;
    if(c->pos >= c->len) return;
    HubTreeIter it;
    hub_tree_iter_init(&it, c->buf + c->pos, c->len - c->pos);
    if(hub_tree_next(&it, &c->e) == 1) {
        c->valid = true;
        c->next = (size_t)(it.p - c->buf);
    }
}

/* Read the tree (oid NULL = empty tree) and put the head at offset pos. */
static inline int cur_open(HubGit* g, const HubOid* oid, size_t pos, HubTreeCur* c) {
    c->buf = NULL;
    c->len = 0;
    c->pos = pos;
    c->valid = false;
    if(!oid) return HUB_GIT_OK;
    uint8_t type;
    size_t size;
    int rc = hub_git_read(g, oid, &type, &c->buf, &size);
    if(rc != HUB_GIT_OK) {
        c->buf = NULL;
        return rc;
    }
    if(type != HUB_OBJ_TREE) {
        hub_git_free(c->buf);
        c->buf = NULL;
        return HUB_GIT_ERR_CORRUPT;
    }
    c->len = size;
    cur_load(c);
    return HUB_GIT_OK;
}

static inline void cur_advance(HubTreeCur* c) {
    c->pos = c->next;
    cur_load(c);
}

static inline void cur_close(HubTreeCur* c) {
    if(c->buf) hub_git_free(c->buf);
    c->buf = NULL;
    c->valid = false;
}

static inline int cur_cmp(const HubTreeCur* a, const HubTreeCur* b) {
    return hub_gitwrite_ent_cmp(
        a->e.name, a->e.name_len, cur_is_tree(a), b->e.name, b->e.name_len, cur_is_tree(b));
}
