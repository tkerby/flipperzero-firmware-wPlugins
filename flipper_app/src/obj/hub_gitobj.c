#include "hub_gitobj.h"

#include "hub_gitio.h"
#include "../util/hub_inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../http/hub_mem.h"

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_gitobj_int.h"
#include "hub_gitobj_idx.c"
#include "hub_gitobj_read.c"
#include "hub_gitobj_sink.c"
#include "hub_gitobj_spill.c"
#include "hub_gitobj_refs.c"
#include "hub_gitobj_parse.c"
#include "hub_gitobj_graph.c"
