#pragma once

/* Git document store: GET/PUT /api/git/doc, POST /api/git/doc-delete,
 * GET /api/git/docs. Files live in HUB_GIT_ROOT/<name>.git/hub/<key>.
 * See docs/git-design.md, "Document store". */

#include "hub_req.h"
#include "hub_gitdocutil.h"

/* Returns true when the request was handled (the answer is sent). */
bool hub_gitdoc_route(HubReq* r);
