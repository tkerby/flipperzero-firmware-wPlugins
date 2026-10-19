#pragma once
// The UI plugin (contract section 11.5): the screens that are never on while the player explores or fights live in
// `ez_ui.fal`, loaded only while one of them is on, because the whole FAP sits in RAM:
//   title (+ Continuar list), first-run language picker, name picker + naming keyboard (+ the Solar / Lunar affinity picker, contract 66), nickname prompt, options, the Elementi picture
//   popup, the "Guardar" screen.
// Same mechanism as the battle and menu plugins (plugin_host.h, app_api_table_i.h): this header is the interface between the app
// and the plugin. Host harness: the same files are linked in and plugin_link.c forwards to them.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"
#include "save.h"
#include "battle_core.h"

#define UI_PLUGIN_APPID       "elementis_zeta"
#define UI_PLUGIN_API_VERSION 2 // 2 (C4 memory recovery): + bc_load
#define UI_PLUGIN_FILE        "plugins/ez_ui.fal" // under the app assets folder (fal_embedded)

// What ui_open_*() asks the plugin to open (UiPlugin.open, M5 memory recovery): ONE function pointer instead of eight, so the app's side is eight 8-byte wrappers
// around a shared opener (ui_open.c) and the dispatch is in the plugin (ui_scene.c).
enum {
    UI_OPEN_TITLE = 0,
    UI_OPEN_LANG,
    UI_OPEN_OPTIONS,
    UI_OPEN_NAME,
    UI_OPEN_MON,
    UI_OPEN_SAVE,
    UI_OPEN_NICK,
    UI_OPEN_EVO
};

typedef struct UiPlugin {
    bool (*active)(void); // PluginBase: true while any of the plugin's scenes is on the stack
    const SceneOps*
        title_ops; // the title scene (the FADE scene swaps it in below itself, scene_fade.c)
    // Opens one screen: `what` is UI_OPEN_*, `arg` the second argument of the screen that takes one (name: who, mon: species, nick / evo: the party slot;
    // M5-B evo: scene_evo.c; NICK_PENDING for the creature in the pending slot). False = nothing opened.
    bool (*open)(SceneCtx* c, uint8_t what, uint8_t arg);
    // task K: derives the codes of the active language from PartyState's Unicode names (names.c names_apply)
    void (*apply_names)(void);
    // task K: save_load() (save_io.c) for the resident save_continue()
    bool (*save_load)(World* w, SaveInfo* in);
    // C4 memory recovery (contract 38.1): battle_load.c's bc_load, run once at boot by party_init (ui_bc_load): the header checks and the item table, 1.1 KB of code that no longer sits in the app
    BcErr (*bc_load)(BcData* d, const BcIo* io);
} UiPlugin;

// The plugin's single instance (ui_scene.c): linked into the plugin on the device, into the executable on the host.
extern const UiPlugin ui_plugin_iface;
// M5-B: the evolution scene (UI plugin side; the app opens it through ui_open_evo)
bool scene_evo_open(
    SceneCtx* c,
    uint8_t
        arg); // arg = the party slot, or (C4-B, contract 36) 0x80 | slot for a stone used from the bag (target = c->stone_to; not cancellable)
extern const SceneOps scene_evo_ops;
#ifndef NUCLEO_DEVICE
int scene_evo_opened(void); // host tests: how many EVO scenes were opened so far
bool scene_evo_probe(const SceneCtx* c, int* ph, int* show, uint32_t* t, bool* list); // host tests
#endif
// The slot number that the nickname prompt (scene_nick_open / scene_kb_open kind 2) takes for the creature in the PENDING slot (a capture
// with a full party, party.h party_pending_*): it is named before it goes to the box.
#define NICK_PENDING 0xFE

// Scene bookkeeping: every UI scene calls ui_live_enter() in its enter() and ui_live_leave() in its leave(); active() is
// "count > 0", which is how plugin_poll() knows the code can go.
void ui_live_enter(void);
void ui_live_leave(void);
#ifndef NUCLEO_DEVICE
void ui_live_reset(
    void); // host harness: a scenario that starts over (new SceneCtx) forgets the scenes of the previous one
#endif

// ---- app side (ui_open.c, shared by the device and the host harness). Each loads the plugin when it is not loaded and
// opens the screen; false = nothing opened (plugin file / heap / scene stack) and the plugin is unloaded again unless
// another of its screens is on.
bool ui_open_title(SceneCtx* c);
bool ui_open_lang(SceneCtx* c);
bool ui_open_options(SceneCtx* c);
bool ui_open_name(SceneCtx* c, uint8_t who);
bool ui_open_mon(SceneCtx* c, uint8_t species);
bool ui_open_save(SceneCtx* c);
bool ui_open_nick(SceneCtx* c, uint8_t slot);
bool ui_open_evo(
    SceneCtx* c,
    uint8_t
        slot); // M5-B: world.c's post-battle chain (contract 24); C4-B: scene_menu.c's update passes 0x80 | slot (a stone used from the bag, contract 36)
// task K: names_apply() / save_load() through the plugin (loads it when it is not loaded and unloads it again when no UI screen is on). False:
// the plugin is not available (apply_names: nothing is done, the codes stay what they were).
void ui_apply_names(void);
bool ui_save_load(World* w, SaveInfo* in);
// bc_load(d, io) through the plugin (party_init, once at boot): loads the plugin, runs the loader and unloads it again (the title opens it next). BC_E_OPEN when the plugin cannot be loaded.
BcErr ui_bc_load(BcData* d, const BcIo* io);
// The title scene's ops (loads the plugin if needed), NULL when it cannot be loaded.
const SceneOps* ui_title_ops(void);
