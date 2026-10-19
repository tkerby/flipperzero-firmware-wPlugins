// Unity build of the UI plugin (ez_ui.fal): ONE translation unit like the battle plugin's (lib/battle/battle_unity.c), compiled with -fno-function-sections and
// -fwhole-program (application.fam). Why (contract 44): the loader reads the plugin's relocation data (.fast.rel.*, tools/../fastfap.py) whole into the heap at the load, and
// that is the heap's low point for this plugin (the `ui_open` row). The data has 13 bytes for each distinct target symbol that is DEFINED in the module (a global function or
// table other files call or point at) + 3 bytes per reference; a call between two translation units of the plugin is such a target, a call inside one unit is a plain `bl`
// with no entry at all, and a pointer to a static is the section's entry (shared by all). The unity build also lets the compiler drop what the plugin never calls (the shared
// files plugin_fs.c / settings_io.c carry functions only the other plugin uses) and share one literal pool. The sources are unchanged: the host harness still compiles them
// one by one; only the entry point is marked externally_visible (ui_plugin.c).
#include "../../battle_load.c"
#include "../../scene_title.c"
#include "../../scene_lang.c"
#include "../../scene_name.c"
#include "../../scene_kb.c"
#include "../../names.c"
#include "../../save_io.c"
#include "../../settings_io.c"
#include "../../plugin_fs.c"
#include "../../scene_options.c"
#include "../../scene_mon.c"
#include "../../scene_save.c"
#include "../../scene_evo.c"
#include "../../ui_scene.c"
#include "../../ui_plugin.c"
