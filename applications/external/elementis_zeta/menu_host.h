#pragma once
// The MENU plugin (contract sections 11 and 15): the party screen (list, option box, summary pages, Cambiar) and the bag screen
// (item list, Usar / Tirar, the party list as the target of a Salve) and the shop screen (section 17) built as `ez_menu.fal`, loaded when the player opens
// "Equipo" or "Mochila" in the pause menu and unloaded when its screens have closed. Same mechanism as
// the battle plugin (plugin_host.h, app_api_table_i.h): this header is the interface between the app and the plugin.
// Host harness: the same files are linked in and plugin_link.c forwards to them.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"

#define MENU_PLUGIN_APPID "elementis_zeta"
#define MENU_PLUGIN_API_VERSION \
    7 // 7 (C4 memory recovery): open_shop takes the shop id (the plugin reads shops.bin), 6 (C3-A): + open_pick, 2: + open_bag, 3: + open_shop, 4 (M5-C): + open_card, open_pc, open_store, 5 (M5 memory recovery): those three moved to the PC plugin (pc_host.h)
#define MENU_PLUGIN_FILE "plugins/ez_menu.fal" // under the app assets folder (fal_embedded)

typedef struct MenuPlugin {
    bool (*active)(
        void); // PluginBase: true while the party or the bag screen's state exists (its ops are on the scene stack)
    // Pushes the party screen (list) on top of the scene stack. False (nothing pushed) when the stack is full or the
    // state cannot be allocated.
    bool (*open_party)(SceneCtx* c);
    // Same for the bag screen (the party list it opens for a Salve is pushed by the plugin itself).
    bool (*open_bag)(SceneCtx* c);
    // Same for the shop screen (section 17) of shop `id` (shops.bin: the plugin reads the stock, scene_shop.c shop_load). It opens with the greeting.
    bool (*open_shop)(SceneCtx* c, uint8_t id);
    // C3-A (contract 32): the party list as a PICKER for a script (native 23). mode 0: any creature, *out = its slot; mode 1: the in-game trade (World.tr: wanted species,
    // given species, nickname text id): *out = 1 traded / 0 wrong species; Back = 255 in both. The screen closes after the choice. False (nothing pushed) when it cannot open.
    bool (*open_pick)(SceneCtx* c, uint8_t mode, uint8_t* out);
} MenuPlugin;

// ---- app side (menu_open.c, shared by the device and the host harness)
// "Equipo" of the pause menu: an empty party shows the dialog gl.party.none (no plugin is loaded); otherwise loads the
// plugin and pushes the party screen. The plugin stays loaded until the screen has closed: plugin_poll() (game loop)
// unloads it. False (nothing pushed, plugin unloaded) when it cannot be opened (the menu then falls back to "Pronto...").
bool menu_open_team(SceneCtx* c);
// "Mochila" of the pause menu: an empty bag shows the dialog gl.bag.empty (no plugin is loaded); otherwise like menu_open_team().
bool menu_open_bag(SceneCtx* c);
// The script native `shop ID` (natives_shop.c): loads the plugin and pushes the shop screen of shop `id`. False (nothing
// pushed, plugin unloaded) when it cannot be opened (no such shop / no heap / file / stack); the native then lets the script go on.
bool menu_open_shop(SceneCtx* c, uint8_t id);
// C3-A: the script native `pick` (natives.c): loads the plugin and pushes the party picker (menu_host.h MenuPlugin.open_pick). False = not opened.
bool menu_open_pick(SceneCtx* c, uint8_t mode, uint8_t* out);
// M5-C (contract 25), implemented in the PC plugin since the M5 memory recovery (pc_host.h). Each loads the PC plugin, opens the screen and unloads the plugin again when it cannot open (false: nothing pushed).
// menu_open_pc: the script native `pc` (native 22). menu_open_store: the post-battle store of the
// pending creature (party_pending_get() must be true, else false): "X was sent to Box N" + the game is saved; the scene closes by itself.
bool menu_open_pc(SceneCtx* c);
bool menu_open_store(SceneCtx* c);
// C4-A (contract 35.5): the elevator's floor list (PC plugin, PC_OPEN_LIST): native 23 op 13; the chosen row (255 = Back) is written to script variable `var`. False = nothing opened.
bool menu_open_list(SceneCtx* c, uint8_t var);
// The pause menu's trainer card (MB_CARD, contract 25.4: the PC plugin's screen, scene_tcard.c; it was the casino plugin's from 74 to 76) and Registro (BOND_REG, casino_host.h, contract 30 / 76: the CASINO plugin's screen, scene_reg.c): loads the plugin and
// pushes the screen above the menu. False (nothing pushed) when it cannot open. ONE opener for both: the resident app pays for one function, not two.
enum {
    MB_CARD = 0
}; // (= PC_OPEN_CARD; BOND_REG = 5 is casino_host.h's)
bool menu_open_bond(SceneCtx* c, uint8_t what);
