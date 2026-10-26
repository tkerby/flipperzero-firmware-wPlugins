#define HUB_PLUGIN_BUILD
#include "hub_gitext.h"
#include "../http/hub_plugin.h"
#include "../git/hub_git.h"
#include "../obj/hub_gitobj.h"
#include "../write/hub_gitdiff.h"
#include "../write/hub_gitcommit.h"
#include "hub_gitsearch.h"
#include "../write/hub_gitwrite.h"

#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../http/hub_mem.h"

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_gitext_int.h"
#include "hub_gitext_common.c"
#ifdef HUB_EXT_QUERY
#include "hub_gitext_diff.c"
#endif /* HUB_EXT_QUERY */
#ifdef HUB_EXT_WRITE
#include "hub_gitext_write.c"
#endif /* HUB_EXT_WRITE */
#ifdef HUB_EXT_QUERY
#include "hub_gitext_log.c"
#endif /* HUB_EXT_QUERY */

#include "hub_gitext_dispatch.c"
