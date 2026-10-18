#pragma once
// The PC plugin (contract sections 25 and 25.6): the PC box screen, the trainer card and the "pending capture goes to the box" scene, built as
// `ez_pc.fal`, loaded when one of them opens and unloaded when they have closed. It used to be part of the menu plugin; M5's memory recovery
// split it off (the menu plugin is back to the party / bag / shop screens, 10 KB). Same mechanism and symbol table as the other plugins
// (plugin_host.h, app_api_table_i.h): this header is the interface between the app and the plugin. Host harness: the same files are linked in and
// plugin_link.c forwards to them.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"

#define PC_PLUGIN_APPID "elementis_zeta"
#define PC_PLUGIN_API_VERSION 1
#define PC_PLUGIN_FILE "plugins/ez_pc.fal" // under the app assets folder (fal_embedded)

// (The Registro, contract 30, was PC_OPEN_REG = 3 here; since contract 76 it is the casino plugin's `bond reg`, casino_host.h BOND_REG: the pause menu's first entry, shown once the player has the Registro, world flag FLAG_REGISTRO.)
enum { PC_OPEN_CARD = 0, PC_OPEN_BOX, PC_OPEN_STORE };
// PC_OPEN_LIST + v (v = a script variable 0..15, C4-A, contract 35.5): the elevator's floor list (scene_card.c scene_list_open; the rows are World.tr); the chosen row goes to variable v.
#define PC_OPEN_LIST 16
// The world flag that gives the player the Registro: `parcel_delivered` (Lin received the parcel, maps/villa_lab.map).
#define FLAG_REGISTRO 41

// What menu_open_daycare() asks the plugin (PcPlugin.daycare, C3, contract 33): the day care's work that needs the species data (daycare_pc.c).
enum { DC_DEPOSIT = 0, DC_WITHDRAW, DC_GAINED, DC_PRICE };
// C5 (contract 42): PcPlugin.daycare(DC_FEAT + act, arg) runs the world feature `act` (world.h WACT_*; scene_card.c world_feat) in the plugin: the surf / Empuje prompts, the fishing cast. 1 = it pushed a scene or started a step.
#define DC_FEAT 16

typedef struct PcPlugin {
    bool (*active)(void); // PluginBase: true while the PC screen's, the card's or the store scene's state exists
    // Pushes one of the plugin's scenes on the stack: PC_OPEN_CARD = the trainer card (pause menu, between Mochila and Guardar), PC_OPEN_BOX = the PC box screen
    // (script native `pc`), PC_OPEN_STORE = the "pending capture goes to the box" scene (opened by the world after a battle that parked a creature,
    // party.h party_pending_*). False = nothing pushed (stack full / a screen of it is already open / no heap or card).
    bool (*open)(SceneCtx* c, uint8_t what);
    // C3 (contract 33): the day care's deposit / withdraw / levels gained / price (daycare_pc.c); op is DC_*, arg the party slot of DC_DEPOSIT. The result is the bool of
    // deposit / withdraw, the number of levels, or the price. Not a screen: the plugin goes again at once unless a screen of it is on.
    uint32_t (*daycare)(uint8_t op, uint8_t arg);
} PcPlugin;

// App side (menu_open.c): PcPlugin.daycare through the plugin (loaded for the call, unloaded again when none of its screens is on); 0 when the plugin cannot be loaded.
uint32_t menu_open_daycare(uint8_t op, uint8_t arg);

// The plugin's scenes (scene_box.c: the PC screen and the store scene; scene_card.c: the card), behind the two entry points the interface uses. Linked into the
// plugin on the device and into the executables on the host (plugin_link.c has weak stand-ins for a harness that does not link scene_box.c).
bool scene_pc_open(SceneCtx* c, uint8_t what);
bool scene_pc_active(void);
uint32_t daycare_pc_op(uint8_t op, uint8_t arg); // daycare_pc.c
uint32_t world_feat(uint8_t act, uint8_t arg);   // scene_card.c (C5, contract 42): the world features of DC_FEAT; acts on THE world (g_w)
