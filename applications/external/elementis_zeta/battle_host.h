#pragma once
// The BATTLE scene as a loadable plugin (contract section 10.7).
//
// Device: scene_battle.c, battle_fx.c, battle_gfx.c, blit_fx.c ... are built into a separate plugin (`ez_battle.fal`,
// application.fam); plugin_loader.c (plugin_host.h) loads it with the firmware's plugin manager when a battle starts and
// unloads it once the battle scene is gone, so its ~26 KB of code are only in RAM while a battle is on. Host harness: the
// same files are linked into the executable and plugin_link.c forwards to them (no plugin).
//
// This header is the interface between the two sides. The plugin's entry point returns a BattlePlugin; the plugin
// calls back into the app through the symbol table in app_api_table.cpp (every app function it uses must be listed
// there; `tools/check_plugin_api.py` checks it).
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"
#include "battle_core.h"
#include "config.h"

// Result written to the script variable when a battle ends.
enum {
    BATTLE_LOST = 0,
    BATTLE_WON = 1,
    BATTLE_RAN = 2,
    BATTLE_CAUGHT = 3
}; // CAUGHT: a wild creature was captured (14.2)

#define BATTLE_PLUGIN_APPID \
    "elementis_zeta" // descriptor id the plugin manager filters on (the parent app's id)
#define BATTLE_PLUGIN_API_VERSION 3 // 3: `active` became the first member (PluginBase)
#define BATTLE_PLUGIN_FILE        "plugins/ez_battle.fal" // under the app assets folder (fal_embedded)

// What the plugin hands to the app (a const struct in the plugin's rodata; valid while the plugin is loaded).
typedef struct BattlePlugin {
    bool (*active)(
        void); // PluginBase: true while a battle scene state exists (its ops are, or are about to be, on the stack)
    // scene_battle_trainer() / scene_battle_wild(): push the battle scene. The transition flashes / wipes the world's last
    // frame, which battle_open.c left in `c->frame->lvl` (no copy: the level buffer is not touched until the scene draws).
    bool (*open_trainer)(SceneCtx* c, uint8_t trainer, uint8_t* result_out);
    bool (*open_wild)(SceneCtx* c, uint8_t species, uint8_t level, uint8_t* result_out);
} BattlePlugin;

// ---- app side
// battle_open.c (shared by the device and the host harness): open a battle. Order (contract 10.7, the heap's low point is
// the plugin load: the loader needs the plugin's ~26 KB plus ~3 KB of relocation data at once): draw the world once
// (its last frame stays in `lvl` for the transition: no copy), free the map's sprite bank (7 - 13 KB), THEN get the
// plugin, then push the scene. False (nothing pushed, plugin unloaded, THEN the bank restored) when the plugin cannot be
// loaded or the scene cannot be opened (natives.c then answers "won").
bool battle_open_trainer(SceneCtx* c, uint8_t trainer, uint8_t* result_out);
bool battle_open_wild(SceneCtx* c, uint8_t species, uint8_t level, uint8_t* result_out);
// M5-A (contract 23): bit k = party slot k gained at least one level in the battle that just ended (the plugin sets it while it plays the
// level-up; cleared when a battle opens; the party order does not change in a battle, so the slot is the final one). The evolution check reads it.
extern uint8_t g_battle_leveled;
// M5-A: a creature captured with a full party goes to the pending slot (party_pending_set, party.h; the plugin calls it inline) instead of being released.
// The plugin calls this when its scene has popped itself after a battle (finish()): the app takes over. The plugin
// is still loaded here (its code is on the stack), so the map's bank must NOT come back yet: the screen is held white,
// and battle_poll() (the game loop, top of every iteration) unloads the plugin, THEN restores the bank (failure: the
// world shows the "map load" plate), logs the heap and starts the fade-in to the world. Host (no plugin): all at once.
void battle_scene_closed(SceneCtx* c);
// Call from the game loop, never from inside a scene callback: plugin_poll() (any plugin whose scene is gone is unloaded)
// and then, when a battle just ended (battle_scene_closed), the bank restore + the fade-in back to the world.
void battle_poll(void);
// App exit: unloads the plugin (any) if it is still loaded (after scene_clear()).
void battle_shutdown(void);
// True while the BATTLE plugin is loaded.
bool battle_loaded(void);
// True between the end of a battle scene and the fade-in start (plugin still to unload, bank still to restore).
bool battle_closing(void);
