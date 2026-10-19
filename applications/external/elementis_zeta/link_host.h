#pragma once
// The LINK plugin (contract section 45): the IR link of the Club de Enlace (Club de Enlace over the Flipper's infrared port), built as `ez_link.fal`, loaded by the script
// native `pc` with a link operation (natives_shop.c nat_pc) and unloaded when its scene has closed. A sixth plugin: nothing of it is resident except the native and one pointer (g_link_foe).
// Same mechanism and symbol table as the other plugins (plugin_host.h, app_api_table_i.h); this header is the interface between the app and the plugin.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"
#include "party.h"
#include "link_game.h"

#define LINK_PLUGIN_APPID       "elementis_zeta"
#define LINK_PLUGIN_API_VERSION 1
#define LINK_PLUGIN_FILE        "plugins/ez_link.fal" // under the app assets folder (fal_embedded)

// The operations of native 22 (`pc`) beyond the casino's (`a` = op << 4 | VAR, `b` = an operand; natives_shop.c). VAR (<= 14) receives the result code LR_* (link_game.h); VAR + 1 the second result.
//  LK_OP_TRADE     `link trade VAR`  the trade scene. VAR = LR_*; VAR + 1 = the party slot that evolves because of the trade (255 = none). Yields.
//  LK_OP_BATTLE    `link battle VAR TRAINER`  the team exchange of the exhibition battle. VAR = LR_*. On LR_DONE g_link_foe holds the friend's team, bound to the trainers.toml
//                  row TRAINER (b): the script then runs `battle TRAINER VAR2` (native 4) and the battle plugin plays that trainer against the friend's team. Yields.
//  LK_OP_SELFTEST  `link selftest VAR`  (the dev self-test) VAR = 1 when the IR codec round trip passed, VAR + 1 = the measured payload bytes / second (cap 255). Yields (a dialog).
//  LK_OP_EVOLVE    `link_evolve`  (in the resident native: the evolution scene lives in the UI plugin and a plugin may not open another) VAR = the slot to evolve (or 255): opens
//                  the existing evolution scene (ui_open_evo, 0x80 | slot; the target is SceneCtx.stone_to, which the trade scene stored). VAR is set to 255. Yields.
enum {
    LK_OP_TRADE = 9,
    LK_OP_BATTLE,
    LK_OP_SELFTEST,
    LK_OP_EVOLVE
}; // native 22 `pc` with these operations (1..8 are the casino's: CAS_*, casino_host.h; the field is 4 bits, so 9..12 and 13..15 spare)

// The friend's team after a battle exchange (a heap block made by the link plugin, owned by the app: the link plugin frees a stale one at its next call, the battle plugin takes it over).
typedef struct LinkFoe {
    Party party; // healed copies of the friend's creatures (full HP / PP, no status)
    Party mine; // OUR team as a healed COPY: the exhibition is fought by it, the real party is never touched (the battle scene owns and frees the block)
    tchar name[PARTY_NAME_CAP]; // the friend's name as text codes (ASCII as is, anything else '?')
    uint8_t
        trainer; // the trainers.toml row the script battles: only a battle against this id uses the block
} LinkFoe;
extern LinkFoe* g_link_foe;

typedef struct LinkPlugin {
    bool (*active)(void); // PluginBase: true while the link scene's state exists
    // Runs operation `op` (LK_OP_TRADE / BATTLE / SELFTEST): `out` is the script variable VAR (out[0]) and VAR + 1 (out[1]). True = a scene was pushed (the script yields until it pops).
    bool (*run)(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
} LinkPlugin;

// The plugin's functions (scene_link.c), behind the interface above; plugin_link.c has weak stand-ins for a host harness that does not link them.
bool link_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
bool link_active(void);

// The transport. Device: link_ir.c (the infrared port through link_phy.c). Host: link_host_chan, set by the test (NULL = no transport: the scene does not open).
const LinkChan* link_chan_open(void);
void link_chan_close(void);
#ifndef NUCLEO_DEVICE
extern const LinkChan* link_host_chan;
// Host tests: the link scene's state
typedef struct LinkProbe {
    LinkGame* game;
    LinkSess* sess;
    uint8_t phase, cur, sel;
    bool open;
} LinkProbe;
bool scene_link_probe(LinkProbe* p);
#endif
