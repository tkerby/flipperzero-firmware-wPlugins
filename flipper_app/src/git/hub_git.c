#include "hub_git.h"
#include "../doc/hub_gitdoc.h"
#include "../obj/hub_gitobj.h"
#include "../http/hub_plugin.h"
#include "../ext/hub_gitext.h"
#include "../pack/hub_gitpack.h"
#include "../pack/hub_gitgraph.h"
#include "../http/hub_mem.h"

/* Git hosting, phase 1: bare repositories on the SD card, served over the git
 * dumb HTTP protocol. The PC side does all the real git work (see
 * docs/git-design.md). Here we only store files and keep refs consistent. */

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_git_int.h"
#include "hub_git_util.c"
#include "hub_git_refs.c"
#include "hub_git_routes.c"
#include "hub_git_pack.c"
#include "hub_git_dispatch.c"
