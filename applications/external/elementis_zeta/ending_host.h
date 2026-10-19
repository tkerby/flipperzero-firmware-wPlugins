#pragma once
// The ENDING plugin (contract section 50): the Hall of Legacy scene, the Hall of Legacy browser, the credits and the "FIN" screen, built as `ez_ending.fal`, loaded by the script native `pc` with the operation
// EN_OP (natives_shop.c, like the casino and link plugins) and unloaded when its scene has closed. A seventh plugin: nothing of it is resident except the routing in native 22 and the loader's row.
// Same mechanism and symbol table as the other plugins (plugin_host.h, app_api_table_i.h); this header is the interface between the app and the plugin. Host harness: the same files are linked in and
// plugin_link.c forwards to them.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"
#include "hof.h"

#define ENDING_PLUGIN_APPID       "elementis_zeta"
#define ENDING_PLUGIN_API_VERSION 1
#define ENDING_PLUGIN_FILE        "plugins/ez_ending.fal" // under the app assets folder (fal_embedded)

// Native 22 (`pc`), operation EN_OP = 14 (`a` = EN_OP << 4 | VAR, `b` = the sub-operation EN_*; the op numbers of native 22 are ONE space shared with the casino 1..8 and the link plugin 9..12; 13 and 15 stay spare).
//  EN_RUN      `ending run VAR`      the champion's ending: the Hall of Legacy screens (each creature of the party), the record in hof.bin, the flags (league_champion set, the run flags cleared), the game saved with the
//                                    hero at home (the new-game place), the credits roll, FIN. VAR = 1 the scenes ran (the game is saved), 0 the plugin could not run (nothing happened). Yields. The script then says
//                                    `ending title` (EN_TITLE) to go back to the title.
//  EN_BROWSE   `ending browse VAR`   the Hall of Legacy browser (newest run first); with no record it says so in a dialog. VAR = 1 shown, 0 could not run. Yields.
//  EN_CREDITS  `ending credits VAR`  the credits roll and FIN alone (a dev preview; nothing is recorded or saved). VAR as above. Yields.
//  EN_TITLE    `ending title`        (resident, not the plugin) fade out to the title screen: the plugin has been unloaded by then, the UI plugin loads for it.
enum {
    EN_OP = 14
};
enum {
    EN_RUN = 0,
    EN_BROWSE,
    EN_CREDITS,
    EN_TITLE
};

// Music for the three moments: the composed tracks of assets/music (salon_fama.txt: a fanfare, then the anthem loops through the creature screens and the summary; creditos.txt: the credits medley, loops;
// fin_jingle.txt: `once`, at FIN), indexed in assets/music/index.txt (the generated gen/music_gen.h names; the music notes).
#define END_SND_HOF     SND_SALON_FAMA
#define END_SND_CREDITS SND_CREDITOS
#define END_SND_FIN     SND_FIN_JINGLE

typedef struct EndingPlugin {
    bool (*active)(void); // PluginBase: true while the ending scene's state exists
    // Runs a sub-operation (the shape of CasinoPlugin.run: native 22 passes op = EN_OP and arg = EN_RUN / EN_BROWSE / EN_CREDITS): `out` is the script variable VAR (written before it returns). True = a scene was pushed (the script yields until it pops).
    bool (*run)(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
} EndingPlugin;

// The plugin's functions (scene_ending.c), behind the interface above; plugin_link.c has weak stand-ins for a host harness that does not link them.
bool ending_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
bool ending_active(void);
