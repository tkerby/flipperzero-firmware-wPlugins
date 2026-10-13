#pragma once

/* Git routes of phases 3 and 4 that need object work (docs/git-design.md):
 *   GET  /api/git/diff?name=&sha=            commit against its first parent
 *   GET  /api/git/diff?name=&base=&head=     two refs / commits
 *   GET  /api/git/compare?name=&base=&head=
 *   GET  /api/git/search?name=&ref=&q=&max=
 *   POST /api/git/commit-file?name=&branch=&old=&path=&message=&author=&email=&action=put|delete[&new_branch=]
 *   POST /api/git/merge?name=&base=&head=&message=&author=&email=
 * Returns true when the request was handled (the answer is sent). */

#include "hub_req.h"

bool hub_gitext_route(HubReq* r);
