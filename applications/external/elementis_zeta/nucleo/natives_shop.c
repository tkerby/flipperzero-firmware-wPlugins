// `shop ID` (native 12, contract section 17) and `pc` (native 22, section 25): the mart. The stock comes from shops.bin (tools/shopc.py), the screen is the
// SHOP scene of the menu plugin (scene_shop.c), which also reads the stock (shop_load moved there in the C4 memory recovery, contract 38.3). Resident part: the natives themselves.
#include <stdlib.h>
#include "natives_shop.h"
#include "script.h"
#include "world.h"
#include "scene.h"
#include "menu_host.h"
#include "casino_host.h"
#include "link_host.h"
#include "ending_host.h"
#include "ui_host.h"
#include "party.h"
#include "bag.h"
#include "gen/reg_gen.h"

// 12 shop ID: shop screen for the shop `a` (b unused); yields until it closes. When it cannot open (file / heap / plugin) the
// script goes on (logged).
int nat_shop(World* w, uint8_t a, uint8_t b) {
    (void)b;
    if(!w->sc || !menu_open_shop(w->sc, a)) { // (the menu plugin reads the stock itself: contract 38.3)
        SCRIPT_LOG("shop: no such shop / screen not opened");
        return NATIVE_DONE;
    }
    world_stop(w);
    return NATIVE_YIELD;
}

// 22 pc (a = 0): the PC box screen (scene_box.c, contract 25); yields until it closes. When it cannot open the script goes on (logged).
// 22 with a != 0 (C4-C, contract 37): the CASINO operations (and, IR LINK contract 45, the link plugin's: ops 9..12, link_host.h), a = op << 4 | VAR, b = an operand (casino_host.h): everything (the Coin Purse / coin / money checks, the slot machine,
// the prize counter) is in the casino plugin, which is loaded for the call; the result goes into the script variable VAR; a scene it opened makes the script yield.
int nat_pc(World* w, uint8_t a, uint8_t b) {
    if(a) {
        uint8_t* v = &w->vars[a & 15];
        const uint8_t op = a >> 4;
        if(op == LK_OP_EVOLVE) { // IR LINK (contract 45): the second half of `link trade`: VAR holds the slot that evolves (255 = none); the evolution scene is in the UI plugin, which a plugin cannot open
            const uint8_t k = *v;
            *v = 255;
            if(k >= BC_MAX_PARTY || !w->sc || !ui_open_evo(w->sc, (uint8_t)(0x80 | k))) return NATIVE_DONE; // (SceneCtx.stone_to = the target, stored by the trade scene)
            world_stop(w);
            return NATIVE_YIELD;
        }
        if(op == EN_OP && b == EN_TITLE) return w->sc && scene_fade_to_title(w->sc) ? NATIVE_YIELD : NATIVE_DONE; // C9 (contract 50.3): the ending is over: fade out to the title (the ending plugin is gone by then, the fade loads the UI plugin)
        const bool lk = (uint8_t)(op - LK_OP_TRADE) < 3; // ops 9..11 = the link plugin (same entry shape as the casino's: active + run), 1..8 = the casino, 14 (EN_OP) = the ending plugin (contract 50)
        const CasinoPlugin* p = w->sc ? plugin_load(op == EN_OP ? PLUGIN_ENDING : lk ? PLUGIN_LINK : PLUGIN_CASINO) : NULL;
        *v = 0; // (CB_ERROR / CS_OK / LR_OPEN_FAIL: what stays when the plugin cannot load)
        const bool scene = p && p->run(w->sc, op, b, v);
        if(p && !p->active()) plugin_unload();
        if(!scene) return NATIVE_DONE;
        world_stop(w);
        return NATIVE_YIELD;
    }
    if(!w->sc || !menu_open_pc(w->sc)) {
        SCRIPT_LOG("pc: screen not opened");
        return NATIVE_DONE;
    }
    world_stop(w);
    return NATIVE_YIELD;
}

// C5-B (contract 43): the Reserva Salvaje's step hook, called once per completed hero step from world.c's on_step_arrival (before the encounter roll). Inside the Reserva
// (PartyState.rv_steps != 0) a step counts down; at 0 steps or with no Reserve Crystal left the session ends: the casino plugin's end scene shows "time's up" / "out of
// crystals" and fades the hero to the gate (casino.c). True = a scene took over (the caller stops the step).
bool reserva_on_step(World* w) {
    PartyState* p = party_state();
    if(!p->rv_steps) return false;
    if(--p->rv_steps && bag_count(IT_CRISTAL_RESERVA)) return false;
    return nat_pc(w, CAS_RV_TIMEUP << 4, 0) == NATIVE_YIELD;
}

LinkFoe* g_link_foe; // IR LINK (contract 45): the friend's team of a battle exchange, for the battle plugin (made by the link plugin, which also frees a stale one at its next call)

#ifndef NUCLEO_DEVICE
// Host harness: the tests that do not call natives_init() install the two natives with this (the device's table is the const g_natives of natives.c).
void natives_shop_init(void) {
    script_register_native(NAT_SHOP, nat_shop);
    script_register_native(NAT_PC, nat_pc);
}
#endif
