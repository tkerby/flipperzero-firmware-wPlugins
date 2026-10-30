// Unity build of the menu plugin (ez_menu.fal): ONE translation unit compiled with -fno-function-sections and -fwhole-program (application.fam), like the battle and UI
// plugins (lib/battle/battle_unity.c, lib/ui/ui_unity.c; the reasons are in lib/ui/ui_unity.c and contract 44): fewer relocation entries in the load-time data, what the
// plugin never calls is dropped. The sources are unchanged: the host harness compiles them one by one; the entry point is marked externally_visible.
#include "../../scene_party.c"
#include "../../scene_bag.c"
#include "../../scene_shop.c"
#include "../../hud_gfx.c"
#include "../../menu_plugin.c"
