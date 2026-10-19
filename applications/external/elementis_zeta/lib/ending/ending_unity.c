// Unity build of the ending plugin (ez_ending.fal, contract section 50): ONE translation unit, compiled with SHORT_CALLS_PLUGIN + SMALL7 + -fwhole-program (application.fam, fap_private_libs), like every other
// plugin (contract 44). The sources are unchanged (the host harness still compiles them one by one); only the entry point (ending_plugin.c) is externally_visible. box.c is here for box_name() alone (the nickname
// -> codes typecast the Hall of Legacy shares with the PC box): everything else of it is unreferenced and the whole-program build drops it.
#include "../../hof.c"
#include "../../box.c"
#include "../../plugin_fs.c"
#include "../../hud_gfx.c"
#include "../../scene_ending.c"
#include "../../ending_plugin.c"
