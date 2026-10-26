#pragma once
// The CASINO plugin (contract section 37): the casino's slot machine, its prize counter and the coin purchase, built as `ez_casino.fal`, loaded by the script
// native `pc` with a casino operation (natives_shop.c) and unloaded when its scenes have closed. A fifth plugin (the PC plugin had 2.5 KB of heap headroom in the DEV build,
// the casino code is ~5 KB). Same mechanism and symbol table as the other plugins (plugin_host.h, app_api_table_i.h); this header is the interface between the app and the
// plugin. Host harness: the same files are linked in and plugin_link.c forwards to them.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "plugin_host.h"

#define CASINO_PLUGIN_APPID       "elementis_zeta"
#define CASINO_PLUGIN_API_VERSION 1
#define CASINO_PLUGIN_FILE        "plugins/ez_casino.fal" // under the app assets folder (fal_embedded)

// The operations of the casino native (native 22, `a` = op << 4 | VAR, `b` = an operand; natives_shop.c). Their results go into the script variable VAR.
//  CAS_BUY    `coins_buy`   the clerk sells 50 coins for $1000 (the script asked YES / NO already): VAR = CB_*.  b unused.
//  CAS_SLOTS  `slots`       the slot machine (yields until Back): VAR = CS_*.   b unused.
//  CAS_PRIZES `prizes ID`   the prize counter of shops.toml shop ID (yields until Back): VAR = CS_*.   b = the shop id.
//  CAS_GET    `coins_get`   VAR = the coins (capped at 255), the text variable {v2} = the exact number.   b unused.
//  C5-B (contract 43): the Reserva Salvaje session. The same native, ops 5 .. 8; the state is PartyState.rv_* (saved with the game), the crystals are the bag item `cristal_reserva`.
//  CAS_RV_START  `reserva_start VAR`  pays RV_PRICE, puts RV_BALLS Reserve Crystals in the bag, remembers the hero's tile as the way home and sets RV_STEPS steps: VAR = RS_*.   b unused.
//  CAS_RV_END    `reserva_end VAR`    ends the session (the crystals left are taken back): VAR = 1 a session was on, 0 none.   b unused.
//  CAS_RV_LEFT   `reserva_left VAR`   VAR = 1 inside / 0 not, the text variables {v2} = the steps left, {v3} = the crystals left.   b unused.
//  CAS_RV_TIMEUP (not for scripts)    the step hook's call (natives_shop.c reserva_on_step): "time's up" / "out of crystals", then the fade home. Pushes a scene.
//  C65 (contract 65): the friendship (amistad) operations. The same native, op 15 (CAS_BOND), `b` = the sub-operation; the casino plugin is the smallest plugin and the only one a script can reach without a new native, so
//  the resident app pays nothing for them (the op nibble already routes anything that is not link / ending to this plugin).
//  BOND_TIER `bond tier VAR` VAR = the lead creature's friendship tier 0..4 (bc_friend_tier: < 50, < 100, < 150, < 220, 220+), 255 when the party is empty; the text variable {v0} = the maintainer's name (nickname or species).
//  BOND_HEAL  `bond heal VAR`  run BEFORE `heal_party` in a Centro: every creature that really needs the healing (HP below the maximum, or a status; a fainted one counts) gains 1 friendship; VAR = how many did.
//  C69 (contract 69, not for scripts; menu_open.c menu_open_daycare sends them here, they write VAR 0 (0)): b >= BOND_ACT = the world acts of world.h from WACT_ROAM on, b = (act - WACT_ROAM + 1) << 4 | arg: WACT_ROAM = the roaming creatures'
//  placement, WACT_ROAM_HIT = the touch of roamer `arg` (scene_roam.c: contract 55's code, moved here from the PC plugin; no scene), WACT_FOLLOW = OK pressed facing the FOLLOWER (world.c talk_npc): its emote bubble and one line by its state /
//  friendship tier (scene_follow.c; pushes a scene).
//  C68 (contract 68): the Flipper's clock for the legend of the Origin Fragments. The same native, op 13 (CAS_LEGEND), `b` = the sub-operation (legend.h CLK_*): the casino plugin is the only place that calls the firmware's
//  RTC (furi_hal_rtc_get_datetime), so the resident app pays nothing. `clock hour|night|cave VAR`: VAR = the hour 0..23 / 1 when it is night (20:00 .. 05:59) / 1 when the Gruta del Eclipse's windows are open
//  (05:00 .. 06:59 and 23:00 .. 00:59). DEV builds and the host harness can override the clock with the flags clk_on / clk_b0 / clk_b1 (legend.h); a release build never reads them.
enum {
    CAS_BUY = 1,
    CAS_SLOTS,
    CAS_PRIZES,
    CAS_GET,
    CAS_RV_START,
    CAS_RV_END,
    CAS_RV_LEFT,
    CAS_RV_TIMEUP,
    CAS_LEGEND = 13,
    CAS_BOND = 15
};
enum {
    BOND_TIER = 0,
    BOND_HEAL = 1,
    BOND_OT = 2,
    BOND_DIPLOMA = 3,
    BOND_REG = 5,
    BOND_ACT = 0x10
}; // contract 74: BOND_OT = `bond ot SLOT` (the slot is read from the variable before the result variable, the result is written to SLOT + 1), BOND_DIPLOMA = `bond diploma` (a scene), contract 76: BOND_REG = the pause menu's Registro (menu_open_bond, not for scripts; scene_reg.c moved here from the PC plugin, which was 383 B under its DEV floor; BOND_CARD = 4 was the trainer card until the card went back to the PC plugin, PC_OPEN_CARD, to balance the two rows)
enum {
    RS_ERROR = 0,
    RS_OK,
    RS_NO_MONEY,
    RS_BAG_FULL,
    RS_INSIDE
}; // CAS_RV_START: 0 the plugin could not run, 1 started, 2 not enough money, 3 the bag cannot hold the crystals, 4 already inside
#define RV_PRICE 500u // the Reserva Salvaje entrance fee
#define RV_STEPS 500u //
#define RV_BALLS 30u // Reserve Balls
enum {
    CB_ERROR = 0,
    CB_OK,
    CB_NO_MONEY,
    CB_NO_CASE,
    CB_FULL
}; // CAS_BUY: 0 the plugin could not run, 1 bought, 2 not enough money, 3 no Coin Purse, 4 the case cannot hold 50 more
enum {
    CS_OK = 0,
    CS_NO_CASE,
    CS_NO_COINS,
    CS_ERROR
}; // CAS_SLOTS / CAS_PRIZES: 0 played / visited, 1 no Coin Purse, 2 no coins (slots), 3 could not open (stack / heap / file)
#define COINS_PACK  50 // 50 coins ...
#define COINS_PRICE 1000 // ... for $1000

typedef struct CasinoPlugin {
    bool (*active)(void); // PluginBase: true while the slot or prize scene's state exists
    // Runs operation `op` (CAS_*): `out` is the script variable that receives the result (written before it returns; a scene that opened keeps it as it is).
    // True = a scene was pushed (the script yields until it pops). The Coin Purse / coins / money checks are made here, not in the app.
    bool (*run)(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
} CasinoPlugin;

// The plugin's functions (casino.c), behind the interface above; plugin_link.c has weak stand-ins for a host harness that does not link them.
bool casino_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out);
bool casino_active(void);
bool roam_feat(uint8_t act, uint8_t arg); // scene_roam.c (contract 55 / 69)
bool scene_follow_open(
    SceneCtx* c); // contract 69 (scene_follow.c): the follower's emote bubble + one line
bool scene_follow_active(void);
bool f2_feat(
    SceneCtx* c,
    uint8_t
        act); // contract 74 (scene_f2.c): WACT_DETECT (a text) / WACT_REGMAP (the region map scene); true = a scene was pushed
uint8_t
    f2_ot(const uint8_t* out); // `bond ot`: out[-1] = the party slot; 1 own / 0 traded / 255 none
bool f2_diploma(SceneCtx* c); // `bond diploma`: the diploma scene
bool scene_f2_active(void);
extern const SceneOps scene_follow_ops;
extern const SceneOps scene_poison_ops; // contract 81 (scene_poison.c): the overworld poison
bool poison_act(SceneCtx* c); // the act WACT_POISON: the damage, the flash scene; true = pushed
bool scene_poison_active(void);
