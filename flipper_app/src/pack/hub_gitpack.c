#include "hub_gitpack.h"

#include "hub_deflate.h"
#include "../doc/hub_gitdocutil.h"
#include "../obj/hub_gitio.h"
#include "../obj/hub_gitobj.h"
#include "../util/hub_sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../http/hub_mem.h"

/* One translation unit: the parts below are included here, in this order, so
 * the build is the same as one file (statics, inlining, merged strings).
 * See src/README.md. */
#include "hub_gitpack_int.h"
#include "hub_gitpack_out.c"
#include "hub_gitpack_loose.c"
#include "hub_gitpack_msrc.c"
#include "hub_gitpack_merge.c"
