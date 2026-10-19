#pragma once
// Loadable plugins (contract section 10.7 / 11): big screens that are needed a few minutes per hour live in a `.fal`
// loaded only while they are on, because the whole FAP sits in RAM. ONE plugin at a time:
//   PLUGIN_BATTLE  ez_battle.fal  the battle scene + effects + turn engine (battle_host.h, ~26 KB)
//   PLUGIN_MENU    ez_menu.fal    the party, bag and shop screens (menu_host.h, ~10 KB)
//   PLUGIN_UI      ez_ui.fal      title, language picker, name picker, options, Elementi popup, save screen (ui_host.h)
//   PLUGIN_PC      ez_pc.fal      the PC box, the trainer card and the post-battle store (pc_host.h; M5 memory recovery, contract 25.6)
//   PLUGIN_CASINO ez_casino.fal the casino: slot machine, prize counter, coin purchase (casino_host.h; C4-C, contract 37)
//   PLUGIN_LINK    ez_link.fal    the Club de Enlace: trade and exhibition battle over the infrared port (link_host.h; contract 45)
//   PLUGIN_ENDING  ez_ending.fal  the Hall of Legacy, its PC browser, the credits and FIN (ending_host.h; contract 50)
// Device: plugin_loader.c (firmware plugin manager + the app's symbol table app_api_table.cpp). Host harness:
// plugin_link.c (the plugin's functions are linked in; its RAM is emulated by one block so allocation trackers see the same
// coexistence). Every plugin interface struct starts with `bool (*active)(void)` (PluginBase), which is how the loader
// knows when the plugin's scene is gone and its code can be freed.
#include <stdint.h>
#include <stdbool.h>

typedef enum { PLUGIN_NONE = 0, PLUGIN_BATTLE, PLUGIN_MENU, PLUGIN_UI, PLUGIN_PC, PLUGIN_CASINO, PLUGIN_LINK, PLUGIN_ENDING } PluginKind;

// First member of every plugin interface (BattlePlugin, MenuPlugin, UiPlugin, PcPlugin, CasinoPlugin, LinkPlugin, EndingPlugin): true while the plugin has a scene state that exists
// (its ops are, or are about to be, on the scene stack).
typedef struct PluginBase {
    bool (*active)(void);
} PluginBase;

// Loads the plugin of `kind` (a no-op returning the same interface when it is already loaded) and returns its interface
// (cast to BattlePlugin / MenuPlugin / UiPlugin / PcPlugin / CasinoPlugin / LinkPlugin / EndingPlugin), or NULL: not enough heap, file missing, bad plugin, or ANOTHER kind is
// loaded and still active. A loaded plugin whose scenes are all gone (active() false, not yet unloaded by plugin_poll())
// is unloaded first, so a screen that closed in this very loop iteration never blocks the next one.
const void* plugin_load(PluginKind kind);
// Unloads right away (the scene was not opened, or the app exits). Never call from inside the plugin's own code.
void plugin_unload(void);
// What is loaded now (PLUGIN_NONE if nothing).
PluginKind plugin_kind(void);
// Unloads the plugin once it is no longer active (its scene popped itself from plugin code, so the code could not be
// freed there). Call from the game loop only, at the top of an iteration (battle_poll() does).
void plugin_poll(void);
// Serial `log` line (tag NucleoPlugin): "heap <what>: free N, largest block N, minimum since boot N". Host: nothing.
void plugin_log_heap(const char* what);
