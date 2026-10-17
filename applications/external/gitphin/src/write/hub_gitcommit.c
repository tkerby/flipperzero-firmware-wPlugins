#include "hub_gitcommit.h"
#include "../obj/hub_gittreecur.h"

#include <stdlib.h>
#include <string.h>
#include "../http/hub_mem.h"

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_gitcommit_file.c"
#include "hub_gitcommit_merge.c"
