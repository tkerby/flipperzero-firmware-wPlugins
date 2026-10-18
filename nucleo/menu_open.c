// Opening the party and bag screens from the pause menu (contract sections 11 and 15), the shop screen from a script (section 17) and the PC
// plugin's screens (trainer card, PC box, post-battle store: sections 25 and 25.6). Shared by the device and the host harness.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-inline-functions-called-once") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "menu_host.h"
#include "pc_host.h"
#include "party.h"
#include "bag.h"
#include "gen/maps_gen.h"
#include "world.h"
#include "natives_shop.h"
#include "casino_host.h"

enum { SC_PARTY = 0, SC_BAG, SC_PICK, SC_SHOP };

// For SC_PICK: `shop` = the result variable and `shop_n` the picker mode; for SC_SHOP `shop_n` = the shop id (one shared opener, C3-A).
static bool open_screen(SceneCtx* c, uint8_t which, uint8_t* shop, uint8_t shop_n) {
    const MenuPlugin* p = plugin_load(PLUGIN_MENU);
    plugin_log_heap("menu screen: after plugin load");
    if(!p) return false;
    if(!(which == SC_PICK  ? p->open_pick(c, shop_n, shop)
         : which == SC_SHOP ? p->open_shop(c, shop_n)
         : which == SC_BAG ? p->open_bag(c)
                           : p->open_party(c))) {
        plugin_unload();
        return false;
    }
    return true;
}

static bool open_pc(SceneCtx* c, uint8_t what) {
    const PcPlugin* p = plugin_load(PLUGIN_PC);
    plugin_log_heap("pc screen: after plugin load");
    if(!p) return false;
    if(!p->open(c, what)) {
        plugin_unload();
        return false;
    }
    return true;
}

uint32_t menu_open_daycare(uint8_t op, uint8_t arg) { // (C3 day care, contract 33)
    if(op >= DC_FEAT + WACT_ROAM) // contract 55 / 69: the roamers' placement and touch and the follower's reaction are the CASINO plugin's (its `bond` operation, casino_host.h BOND_ACT: b = (act - WACT_ROAM + 1) << 4 | arg): the PC plugin gave them the room
        return nat_pc(g_w, CAS_BOND << 4, (uint8_t)((op - DC_FEAT - WACT_ROAM + 1) << 4 | arg)) == NATIVE_YIELD;
    const PcPlugin* p = plugin_load(PLUGIN_PC);
    const uint32_t r = p ? p->daycare(op, arg) : 0;
    if(p && !p->active()) plugin_unload();
    return r;
}

bool menu_open_team(SceneCtx* c) {
    if(party_player()->count == 0) return scene_dialog_open(c, TXT_GL_PARTY_NONE, -1); // nothing to show: no plugin
    return open_screen(c, SC_PARTY, NULL, 0);
}

bool menu_open_bag(SceneCtx* c) {
    if(bag_kinds() == 0) return scene_dialog_open(c, TXT_GL_BAG_EMPTY, -1);
    return open_screen(c, SC_BAG, NULL, 0);
}

bool menu_open_shop(SceneCtx* c, uint8_t id) {
    return id && open_screen(c, SC_SHOP, NULL, id);
}

bool menu_open_pick(SceneCtx* c, uint8_t mode, uint8_t* out) {
    return open_screen(c, SC_PICK, out, mode);
}

bool menu_open_bond(SceneCtx* c, uint8_t what) { // the pause menu's trainer card (MB_CARD: the PC plugin, PC_OPEN_CARD) and Registro (BOND_REG: the CASINO plugin, native 22 `bond reg`: nat_pc loads it, stops the world and says whether a scene was pushed)
    if(what == MB_CARD) return open_pc(c, PC_OPEN_CARD);
    return c->world && nat_pc(c->world, CAS_BOND << 4, what) == NATIVE_YIELD;
}

bool menu_open_pc(SceneCtx* c) {
    return open_pc(c, PC_OPEN_BOX);
}

bool menu_open_store(SceneCtx* c) {
    return party_pending_get(NULL) && open_pc(c, PC_OPEN_STORE);
}

bool menu_open_list(SceneCtx* c, uint8_t var) {
    return open_pc(c, (uint8_t)(PC_OPEN_LIST + var));
}
