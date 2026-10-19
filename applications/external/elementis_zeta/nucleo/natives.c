// Script natives (CALL_NATIVE, contract 8.4): party, naming, Elementi popup. See natives.h.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-tree-forwprop,no-tree-dce,no-tree-loop-optimize,no-tree-ter") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "natives.h"
#include "natives_shop.h"
#include "script.h"
#include "world.h"
#include "scene.h"
#include "party.h"
#include "bag.h"
#include "battle_host.h"
#include "menu_host.h"
#include "ui_host.h"
#include "text.h"
#include "sound.h"
#include "daycare.h"
#include "gen/reg_gen.h"
#include <stdio.h>

#define nlog SCRIPT_LOG

static __attribute__((noinline)) uint8_t var_get(const World* w, uint8_t v) {
    return v < SCRIPT_VARS ? w->vars[v] : 0;
}

// 1 choose_starter VAR: VAR = the species of the player's starter (0 until one was given).
static int nat_choose_starter(World* w, uint8_t a, uint8_t b) {
    (void)b;
    if(a < SCRIPT_VARS) w->vars[a] = party_starter();
    return NATIVE_DONE;
}

// 2 rival_pick VAR_MINE VAR_OUT: VAR_OUT = the species that beats the one in VAR_MINE (the rival takes the counter;
// trainers.bin rival_lab counter table). Also stored as the rival's starter.
static int nat_rival_pick(World* w, uint8_t a, uint8_t b) {
    uint8_t out = party_rival_pick(var_get(w, a));
    if(b < SCRIPT_VARS) w->vars[b] = out;
    if(!out) nlog("rival_pick: no counter for that species");
    return NATIVE_DONE;
}

// 3 give SPECIES LEVEL: adds a creature to the player's party; var 0 = 1 given, 0 refused. C6 (contract 46): LEVEL | 128 (mapc `give_box`) sends it to the PC box when the party is full (var 0 = 2): the
// PC plugin makes the creature at the next world update (WACT_GIVE: it parks it in the pending slot and arms the post-battle chain's box trip, PB_BOX), the species / level ride in World.enc_sp / enc_lv.
static int nat_give(World* w, uint8_t a, uint8_t b) {
    w->vars[0] = party_give(a, b & 127) >= 0;
    if(!w->vars[0] && (b & 128) && a && party_player()->count >= BC_MAX_PARTY && !g_pending.on) {
        w->enc_sp = a;
        w->enc_lv = b & 127;
        w->act = WACT_GIVE;
        w->vars[0] = 2;
    }
    if(!w->vars[0]) nlog("give: party full / bad species / data missing");
    return NATIVE_DONE;
}

// 6 name_select WHO: opens the name picker (0 player, 1 rival; contract 66: 2 = the Solar / Lunar affinity picker, the same scene) and yields until a name / the affinity was chosen.
static int nat_name_select(World* w, uint8_t a, uint8_t b) {
    (void)b;
    if(!w->sc || !ui_open_name(w->sc, a)) {
        nlog("name_select: scene not opened");
        return NATIVE_DONE;
    }
    world_stop(w);
    return NATIVE_YIELD;
}

// 7 name_var N VAR: text variable {vN} (0..3) = the species name of the species in VAR (gl.sp.* text).
static int nat_name_var(World* w, uint8_t a, uint8_t b) {
    uint8_t sp = var_get(w, b);
    uint16_t id = reg_sp_txt(sp);
    if(id == 0xFFFF || !text_set_var_id(a, id)) text_set_var(a, NULL);
    return NATIVE_DONE;
}

// 8 mon_show VAR: popup with the picture of the species in VAR; it stays up during the NEXT say / ask and closes
// with that dialog (scene_mon.c). Nothing is shown when the picture is missing.
static int nat_mon_show(World* w, uint8_t a, uint8_t b) {
    (void)b;
    if(w->sc && !ui_open_mon(w->sc, var_get(w, a))) nlog("mon_show: no picture");
    return NATIVE_DONE; // the script goes on and opens the dialog; the popup below it is not updated before
}

// 4 battle TRAINER VAR: starts the trainer battle (scene_battle.c, a plugin on the device: battle_host.h) and yields until it ends; VAR receives 1 won / 0 lost.
// When the scene cannot be opened (no data / heap) the script goes on and the player "won" (never blocks the story).
static int nat_battle(World* w, uint8_t a, uint8_t b) {
    uint8_t* out = b < SCRIPT_VARS ? &w->vars[b] : NULL;
    if(!w->sc || !battle_open_trainer(w->sc, a, out)) {
        nlog("battle: scene not opened (answering won)");
        if(out) *out = BATTLE_WON;
        return NATIVE_DONE;
    }
    world_stop(w);
    return NATIVE_YIELD;
}

// 5 heal_party: full HP / PP / status for the whole party (the lab does it after the rival fight).
static int nat_heal_party(World* w, uint8_t a, uint8_t b) {
    (void)w;
    (void)a;
    (void)b;
    party_heal_all();
    sound_play(SND_FX_HEAL); // the healing jingle (contract 28)
    return NATIVE_DONE;
}

// ---- M3-B: wild battles
// The battle scene's result values and the world's WILD_* ones are the same numbers (battle_host.h BATTLE_*)
_Static_assert(
    (int)WILD_LOST == (int)BATTLE_LOST && (int)WILD_WON == (int)BATTLE_WON && (int)WILD_RAN == (int)BATTLE_RAN,
    "wild result values");

// 9 wild_set SPECIES LEVEL: remembers the creature of the next wild_go (two operands only: CALL_NATIVE has a and b)
static int nat_wild_set(World* w, uint8_t a, uint8_t b) {
    w->wild_sp = a;
    w->wild_lv = b;
    return NATIVE_DONE;
}

// 10 wild_go VAR: starts the wild battle of wild_set (battle_open_wild, a plugin on the device) and yields until it ends;
// VAR = 0 lost / 1 won / 2 ran / 3 caught. Losing it is a blackout (world.c: the script is cut). When the battle cannot
// open (no data / heap / plugin) VAR = 4 (WILD_NOTOPEN, §72: no fight took place, distinct from a flight) and the script goes on:
// the one-chance legends (mapc `legend=`, Genesa) keep their chance on it.
static int nat_wild_go(World* w, uint8_t a, uint8_t b) {
    (void)b;
    if(!w->wild_sp || w->wild_sp >= SP_COUNT || !w->wild_lv || !world_start_wild(w, w->wild_sp, w->wild_lv, a < SCRIPT_VARS ? a : -1)) {
        nlog("wild: battle not opened (answering 4)");
        if(a < SCRIPT_VARS) w->vars[a] = WILD_NOTOPEN;
        return NATIVE_DONE;
    }
    return NATIVE_YIELD;
}

// 11 give_item ITEM N (M3-E): bag_add; var 0 = how many were really added (0 = bag full / unknown item).
static int nat_give_item(World* w, uint8_t a, uint8_t b) {
    w->vars[0] = (a && a < IT_COUNT) ? bag_add(a, b) : 0;
    if(!w->vars[0]) nlog("give_item: nothing added (bag full / bad item)");
    else sound_play(SND_FX_ITEM); // item received jingle (contract 28)
    return NATIVE_DONE;
}

// ---- M4-C: bag / money helpers, trainers, blackout (contract 8.4, 18)
// 13 take_item ITEM VAR: removes one of ITEM; VAR = 1 when the bag had it.
static int nat_take_item(World* w, uint8_t a, uint8_t b) {
    uint8_t ok = (a && a < IT_COUNT) ? bag_take(a) : 0;
    if(b < SCRIPT_VARS) w->vars[b] = ok;
    return NATIVE_DONE;
}

// 14 has_item ITEM VAR: VAR = 1 when the bag holds one or more of ITEM.
static int nat_has_item(World* w, uint8_t a, uint8_t b) {
    if(b < SCRIPT_VARS) w->vars[b] = (a && a < IT_COUNT && bag_count(a)) ? 1 : 0;
    return NATIVE_DONE;
}

// 15 money_add N: +N*10 yen (capped at MONEY_MAX).
static int nat_money_add(World* w, uint8_t a, uint8_t b) {
    (void)w;
    (void)b;
    money_add((uint32_t)a * 10u);
    return NATIVE_DONE;
}

// 16 money_take N VAR: -N*10 yen; VAR = 1 when paid, 0 when there was not enough (nothing is taken then).
static int nat_money_take(World* w, uint8_t a, uint8_t b) {
    uint8_t ok = money_take((uint32_t)a * 10u);
    if(b < SCRIPT_VARS) w->vars[b] = ok;
    return NATIVE_DONE;
}

// 17 trainer_spot NPC: var 0 = distance to the hero (1 = next to him), the "!" balloon above the NPC when farther.
static int nat_trainer_spot(World* w, uint8_t a, uint8_t b) {
    (void)b;
    w->vars[0] = world_trainer_spot(w, a);
    return NATIVE_DONE;
}

// 18 trainer_come NPC: starts the NPC's walk to the tile next to the hero (the script waits with wait_idle).
static int nat_trainer_come(World* w, uint8_t a, uint8_t b) {
    (void)b;
    world_trainer_come(w, a);
    return NATIVE_DONE;
}

// 19 blackout: a lost trainer battle. The script is cut and the blackout runs (13.7).
static int nat_blackout(World* w, uint8_t a, uint8_t b) {
    (void)a;
    (void)b;
    world_blackout(w);
    return NATIVE_DONE;
}

// 20 item_var N ITEM: text variable {vN} (0..3) = the item's name (gl.it.* text).
static int nat_item_var(World* w, uint8_t a, uint8_t b) {
    (void)w;
    uint16_t id = reg_it_txt(b);
    if(id == 0xFFFF || !text_set_var_id(a, id)) text_set_var(a, NULL);
    return NATIVE_DONE;
}

// 21 nick_prompt SLOT (task K): the nickname question + keyboard for party slot SLOT (255 = the last one); yields until it is done.
static int nat_nick_prompt(World* w, uint8_t a, uint8_t b) {
    (void)b;
    const Party* p = party_player();
    uint8_t slot = a == 255 ? (uint8_t)(p->count - 1) : a;
    if(p->count == 0 || slot >= p->count || !w->sc || !ui_open_nick(w->sc, slot)) {
        nlog("nick_prompt: nothing to name / scene not opened");
        return NATIVE_DONE;
    }
    world_stop(w);
    return NATIVE_YIELD;
}

// ---- C3-A (contract 32): ONE native for the world systems of chapter 3 (one function and one table entry cost less than seven). The high nibble of `a` is the operation, the
// low nibble the script variable it works on (see natives.h for the table); mapc's macros hide the encoding.
static int nat_world(World* w, uint8_t a, uint8_t b) {
    uint8_t op = a >> 4, i = a & 15, *v = &w->vars[i];
    switch(op) {
    case C3A_PICK:
    case C3A_PICK_TRADE: // the party list as a picker (the menu plugin writes the result into the variable; the trade is done there)
        *v = 255;        // cancelled until a creature is chosen (also what a screen that cannot open leaves)
        if(w->sc && menu_open_pick(w->sc, op, v)) {
            world_stop(w);
            return NATIVE_YIELD;
        }
        nlog("pick: screen not opened");
        break;
    case C3A_TRADE_ARG: w->tr[i] = b; break; // (i = 0..15; World.tr has 16 bytes)
    case C3A_DEPOSIT:
        if(*v < BC_MAX_PARTY) *v = daycare_deposit(*v); // (255 = the picker was cancelled: stays)
        break;
    case C3A_WITHDRAW: {
        uint32_t fee = daycare_price();
        *v = money_get() < fee ? 2 : daycare_withdraw() ? (money_take(fee), 1) : 0;
        break;
    }
    case C3A_QUERY:
        if(i > SCRIPT_VARS - 3) break;
        v[0] = daycare_occupied();
        v[1] = daycare_species();
        v[2] = daycare_levels_gained();
        text_set_var_num(2, v[2]); // (the levels: {v2} reserves 4 digits in the lint; the fee: {v1} reserves 6, like gl.bt.money)
        text_set_var_num(1, (int)daycare_price());
        break;
    case C3A_RAND: *v = (uint8_t)(((world_rand(w) >> 16) * b) >> 16); break; // (no division: 0 when b = 0)
    case C3A_COPY: *v = var_get(w, b); break;
    case C3A_ADD: *v = (uint8_t)(*v + var_get(w, b)); break;
    case C3A_SFX: sound_play((uint16_t)(SND_FX0 + b)); break;
    case C3A_MUSIC: sound_play(b); break;
    case C3A_MUSIC_MAP: sound_play(world_music_id(w)); break; // (the bicycle's track while riding, C4-A)
    case C4A_CAUGHT: { // (b = 0: the count; b = N: 1 when at least N species are caught); C4a: {v2} = the count (b = 0) or how many are still missing (b = N)
        uint8_t n = reg_caught_count();
        *v = b ? n >= b : n;
        text_set_var_num(2, b ? (n < b ? b - n : 0) : n);
        break;
    }
    case C4A_ELEVATOR: // the floor list (PC plugin); the result lands in the variable
        *v = 255;
        if(w->sc && menu_open_list(w->sc, i)) {
            world_stop(w);
            return NATIVE_YIELD;
        }
        nlog("elevator: list not opened");
        break;
    default: break;
    }
    return NATIVE_DONE;
}

// Blackout hook: the party is healed and the player pays half his money (silently).
static void blackout_pay(void) {
    party_heal_all();
    money_lose_half();
}

uint8_t g_wild_count_before; // the party's size when the last wild battle opened (a capture appends the new creature); host tests that replace the battle hook set it

static bool wild_battle(World* w, uint8_t species, uint8_t level, uint8_t* result_out) {
    g_wild_count_before = party_player()->count;
    return w->sc && battle_open_wild(w->sc, species, level, result_out);
}

// M5-B (contract 24): what the world's post-battle chain needs (world.h PBH_*).
_Static_assert(PB_SLOTS == BC_MAX_PARTY, "the chain walks the party slots");
static bool evo_ready(uint8_t k) { // slot k evolves now: level reached; a cancelled evolution is simply asked again at the next level-up
    const BcData* d = party_data();
    const Party* p = party_player();
    if(!d || k >= p->count || p->mon[k].species >= d->n_species) return false;
    const SpeciesDef* s = bc_species(d, p->mon[k].species);
    return s->evo_to && s->evo_level && p->mon[k].level >= (s->evo_level & 127); // (contract 65: bit 7 = a friendship evolution: the evolution scene, a plugin, asks for the friendship)
}

static uint8_t pb_hook(World* w, uint8_t op, uint8_t k) {
    const Party* p = party_player();
    if(op == PBH_NICK) { // a wild battle ended with a capture: offers a nickname for the new creature (task K); a full party parked it in the pending slot (M5-B)
        if(!w->sc) return 0;
        if(p->count > g_wild_count_before) return ui_open_nick(w->sc, (uint8_t)(p->count - 1));
        return party_pending_get(NULL) && ui_open_nick(w->sc, NICK_PENDING);
    }
    if(op == PBH_BOX) return w->sc && menu_open_store(w->sc); // (false when nothing is pending)
    if(op == PBH_EVO) {
        if(!(g_battle_leveled & (1u << k))) return 0;
        g_battle_leveled &= (uint8_t)~(1u << k);
        return evo_ready(k) && w->sc && ui_open_evo(w->sc, k);
    }
    if(op == PBH_MASK) // a lost battle left every creature fainted
        for(uint8_t i = 0; i < p->count && i < BC_MAX_PARTY; i++)
            if(p->mon[i].hp) return g_battle_leveled;
    g_battle_leveled = 0; // PBH_CLEAR, or PBH_MASK with nobody alive
    return 0;
}

// world_party_ready_hook: the first creature able to fight: level | species << 8, 0 = none (the encounters and the Ward Spray's level filter, contract 13.3 / 29; the follower's species, contract 69).
static uint16_t party_ready(void) {
    const Party* p = party_player();
    for(int k = 0; k < p->count && k < BC_MAX_PARTY; k++)
        if(p->mon[k].hp > 0) return (uint16_t)(p->mon[k].level | p->mon[k].species << 8);
    return 0;
}

static uint32_t world_seed(void) {
    return *party_rng();
}

// Every native by id. On the device this const table IS the VM's table (script.c reads g_natives: no register calls, no RAM copy: C3-A memory trim, -0.2 KB resident);
// the host harness copies it into the VM's table with script_register_native, which the host tests also use to replace single natives (the battle).
#ifdef NUCLEO_DEVICE
const ScriptNative g_natives[NAT_MAX] = {
#else
static const ScriptNative g_natives[NAT_MAX] = {
#endif
    [NAT_CHOOSE_STARTER] = nat_choose_starter, [NAT_RIVAL_PICK] = nat_rival_pick,       [NAT_GIVE] = nat_give,
    [NAT_BATTLE] = nat_battle,                 [NAT_HEAL_PARTY] = nat_heal_party,       [NAT_NAME_SELECT] = nat_name_select,
    [NAT_NAME_VAR] = nat_name_var,             [NAT_MON_SHOW] = nat_mon_show,           [NAT_WILD_SET] = nat_wild_set,
    [NAT_WILD_GO] = nat_wild_go,               [NAT_GIVE_ITEM] = nat_give_item,         [NAT_SHOP] = nat_shop,
    [NAT_TAKE_ITEM] = nat_take_item,           [NAT_HAS_ITEM] = nat_has_item,           [NAT_MONEY_ADD] = nat_money_add,
    [NAT_MONEY_TAKE] = nat_money_take,         [NAT_TRAINER_SPOT] = nat_trainer_spot,   [NAT_TRAINER_COME] = nat_trainer_come,
    [NAT_BLACKOUT] = nat_blackout,             [NAT_ITEM_VAR] = nat_item_var,           [NAT_NICK_PROMPT] = nat_nick_prompt,
    [NAT_PC] = nat_pc,                         [NAT_WORLD] = nat_world,
};

void natives_init(void) {
#ifndef NUCLEO_DEVICE
    for(uint8_t id = 1; id < NAT_MAX; id++)
        if(g_natives[id]) script_register_native(id, g_natives[id]);
#endif
    world_pb_hook = pb_hook;
    world_wild_hook = wild_battle;
    world_party_ready_hook = party_ready;
    world_heal_hook = blackout_pay;
    world_seed_hook = world_seed;
    scene_menu_team_hook = menu_open_team; // pause menu "Equipo": the party screen (menu plugin)
    world_new_game_hook = party_new_game;
    text_lang_hook = party_apply_names;
}
