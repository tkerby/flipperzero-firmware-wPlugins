// Host harness stand-in for plugin_loader.c: the battle scene and the party screen are linked into the executable, so
// "loading a plugin" returns its functions directly. (The device builds plugin_loader.c instead; application.fam keeps this
// file out of the app.) The plugin's RAM is emulated by one block that lives exactly as long as the device's plugin would
// (plugin_load ... the poll after the scene is gone), so the harness' allocation tracker sees the same coexistence
// (battle_open.c's order: the bank is freed before the "load" and restored after the "unload") and ui_main can assert it.
// Like the device, ONE plugin at a time.
#include "battle_host.h"
#include "menu_host.h"
#include "ui_host.h"
#include "pc_host.h"
#include "casino_host.h"
#include "link_host.h"
#include "ending_host.h"
#include "scene_battle.h"
#include "scene_party.h"
#include "scene_bag.h"
#include "scene_shop.h"
#include "natives_shop.h"
#include <stdlib.h>
#include "render.h"
#include "party.h"

#define EMU_BATTLE_BYTES 25176 // ez_battle.fal: .text + .rodata + .bss (arm-none-eabi-size of the linked ELF: 25,170 with the effects as data (fx.bin); MEM3 unity build 31,732; refresh after changing the plugin)
#define EMU_BATTLE_TRANS 2900 // what the loader holds on top of the sections while it loads and (except ~1.5 KB of bookkeeping that stays) frees at the end: relocation data (1.4 KB) + bookkeeping (~1.5 KB)
#ifndef EMU_MENU_BYTES // ez_menu.fal: .text + .rodata + .bss (party + bag screens 7.7 KB; + the shop screen 3.2 KB = 10.9 KB, section 17.5; 9.9 KB after the PC split).
#define EMU_MENU_BYTES 7400 // The party / bag host tests' heap limits were set with 7.4 KB: tools/host_shop builds with -DEMU_MENU_BYTES=10900.
#endif
#ifndef EMU_PC_BYTES // ez_pc.fal: .text + .rodata + .bss (the PC box, the trainer card and the store scene, contract 25.6). tools/host_shop builds with the real size.
#define EMU_PC_BYTES 8389
#endif
#ifndef EMU_CASINO_BYTES // ez_casino.fal: .text + .rodata + .bss (the slot machine, the prize counter, the coin purchase, contract 37; arm-none-eabi-size of the linked ELF)
#define EMU_CASINO_BYTES 5308
#endif
#ifndef EMU_LINK_BYTES // ez_link.fal: .text + .rodata + .bss (the IR link: session, trade, team exchange, IR driver; contract 45; arm-none-eabi-size of the linked ELF)
#define EMU_LINK_BYTES 10700
#endif
#ifndef EMU_ENDING_BYTES // ez_ending.fal: .text + .rodata + .bss (the Hall of Legacy scenes + browser, the credits, hof.c; contract 50; arm-none-eabi-size of the linked ELF)
#define EMU_ENDING_BYTES 5332
#endif
#define EMU_UI_BYTES 12598 // ez_ui.fal: .text + .rodata + .bss (arm-none-eabi-size of the linked ELF: 12,598 since M5-B: + the evolution scene; 9,316 at task K, 5,100 before)

// Weak stand-ins: a host harness whose Makefile does not link scene_shop.c / natives_shop.c (the shop is optional there) still links;
// the real definitions (strong) replace them when the files are in the executable. natives_init() calls natives_shop_init().
__attribute__((weak)) bool scene_shop_active(void) { return false; }
__attribute__((weak)) bool scene_shop_open_id(SceneCtx* c, uint8_t id) { (void)c; (void)id; return false; }
__attribute__((weak)) void natives_shop_init(void) {}
// M5-C: the same for the PC plugin's scenes (scene_box.c / scene_card.c) in a harness that does not link them
__attribute__((weak)) bool scene_pc_active(void) { return false; }
__attribute__((weak)) bool scene_pc_open(SceneCtx* c, uint8_t what) { (void)c; (void)what; return false; }
// C4-C: the casino plugin's functions (casino.c / scene_slots.c / scene_prize.c) in a harness that does not link them
__attribute__((weak)) bool casino_active(void) { return false; }
__attribute__((weak)) bool casino_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) { (void)c; (void)op; (void)arg; *out = 0; return false; }
// IR LINK (contract 45): the friend's team for the exhibition battle (defined for the device in natives_shop.c; a harness that does not link that file gets this one)
// contract 76: the Registro (scene_reg.c) is a casino plugin screen now; a harness that links casino.c but not scene_reg.c (host_battle c5reserve) gets inert stand-ins
__attribute__((weak)) bool scene_reg_open(SceneCtx* c) { (void)c; return false; }
__attribute__((weak)) bool scene_reg_active(void) { return false; }
__attribute__((weak)) LinkFoe* g_link_foe;
// the link plugin's functions (scene_link.c ...) in a harness that does not link them
__attribute__((weak)) bool link_active(void) { return false; }
__attribute__((weak)) bool link_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) { (void)c; (void)op; (void)arg; out[0] = 0; return false; }
// the ending plugin's functions (scene_ending.c) in a harness that does not link them
__attribute__((weak)) bool ending_active(void) { return false; }
__attribute__((weak)) bool ending_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) { (void)c; (void)op; (void)arg; out[0] = 0; return false; }
__attribute__((weak)) uint32_t daycare_pc_op(uint8_t op, uint8_t arg) { (void)op; (void)arg; return 0; } // (C3 day care, contract 33)

static uint8_t g_probe; // host test probe (plugin_load_probe): what was still in the heap when the BATTLE plugin "loaded"
uint8_t plugin_load_probe(void) {
    return g_probe;
}

static uint8_t* emu; // the "loaded plugin"
static PluginKind emu_kind;

static bool battle_active(void) {
    return scene_battle_core() != NULL;
}

static const BattlePlugin battle_linked = {
    .active = battle_active,
    .open_trainer = scene_battle_trainer,
    .open_wild = scene_battle_wild,
};

static bool menu_active(void) {
    return scene_party_active() || scene_bag_active() || scene_shop_active();
}

static const MenuPlugin menu_linked = {
    .active = menu_active,
    .open_party = scene_party_open,
    .open_bag = scene_bag_open,
    .open_shop = scene_shop_open_id,
    .open_pick = scene_party_open_pick,
};

static const PcPlugin pc_linked = {
    .active = scene_pc_active,
    .open = scene_pc_open,
    .daycare = daycare_pc_op,
};

static const CasinoPlugin casino_linked = {
    .active = casino_active,
    .run = casino_run,
};

static const LinkPlugin link_linked = {
    .active = link_active,
    .run = link_run,
};

static const EndingPlugin ending_linked = {
    .active = ending_active,
    .run = ending_run,
};

static const PluginBase* iface(PluginKind k) {
    if(k == PLUGIN_ENDING) return (const PluginBase*)&ending_linked;
    if(k == PLUGIN_LINK) return (const PluginBase*)&link_linked;
    if(k == PLUGIN_CASINO) return (const PluginBase*)&casino_linked;
    if(k == PLUGIN_UI) return (const PluginBase*)&ui_plugin_iface;
    if(k == PLUGIN_PC) return (const PluginBase*)&pc_linked;
    return k == PLUGIN_BATTLE ? (const PluginBase*)&battle_linked : (const PluginBase*)&menu_linked;
}

size_t g_plugin_live_before_load; // see plugin_load()
unsigned g_plugin_calls; // host tests (contract 72): how many times anybody asked for a plugin (loaded now or already there)
const void* plugin_load(PluginKind kind) {
    g_plugin_calls++;
    if(kind < PLUGIN_BATTLE || kind > PLUGIN_ENDING) return NULL;
    if(emu) {
        if(emu_kind == kind) return iface(kind);
        if(iface(emu_kind)->active()) return NULL;
        plugin_unload(); // as the device: a finished screen's plugin never blocks the next one
    }
    if(kind == PLUGIN_BATTLE) g_probe = (uint8_t)(render_cache_bytes() ? 1 : 0); // 1 renderer caches (the battle data tables stay since contract 27: they are 0.8 KB)
#ifdef TRK_HAVE // host probe for tools/mem_budget.py: the live heap right before the plugin's block (the world's art is already released here)
    { size_t lk, pk; trk_stats(&g_plugin_live_before_load, &lk, &pk); }
#endif
    emu = malloc(kind == PLUGIN_BATTLE ? EMU_BATTLE_BYTES : kind == PLUGIN_UI ? EMU_UI_BYTES : kind == PLUGIN_PC ? EMU_PC_BYTES : kind == PLUGIN_CASINO ? EMU_CASINO_BYTES : kind == PLUGIN_LINK ? EMU_LINK_BYTES : kind == PLUGIN_ENDING ? EMU_ENDING_BYTES : EMU_MENU_BYTES);
    if(!emu) return NULL;
    if(kind == PLUGIN_BATTLE) { // the load's transient: the harness' peak and census see the device's low point
        void* trans = malloc(EMU_BATTLE_TRANS);
        free(trans);
    }
    emu_kind = kind;
    return iface(kind);
}
void plugin_unload(void) {
    free(emu);
    emu = NULL;
    emu_kind = PLUGIN_NONE;
}
void plugin_poll(void) {
    if(emu && !iface(emu_kind)->active()) plugin_unload();
}
PluginKind plugin_kind(void) {
    return emu ? emu_kind : PLUGIN_NONE;
}
void plugin_log_heap(const char* what) {
#ifdef TRK_SITES // heap census (tools/host_battle/memtrack.h): every live block by allocation site, at each of the device's log points
    trk_dump(what);
#else
    (void)what;
#endif
}
