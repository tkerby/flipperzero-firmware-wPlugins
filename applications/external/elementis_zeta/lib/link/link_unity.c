// Unity build of the link plugin (ez_link.fal, contract section 45): ONE translation unit, compiled with SHORT_CALLS_PLUGIN + SMALL7 + -fwhole-program (application.fam, fap_private_libs),
// like every other plugin (contract 44). The sources are unchanged (the host harness still compiles them one by one); the file-local names are prefixed (lkp_ link_proto.c, lg_ link_game.c,
// sl_ scene_link.c, lkir_ link_ir.c) so that they do not clash here; only the entry point (link_plugin.c) is externally_visible.
#include "../../link_phy.c"
#include "../../link_proto.c"
#include "../../link_game.c"
#include "../../hud_gfx.c"
#include "../../scene_link.c"
#include "../../link_ir.c"
#include "../../link_plugin.c"
