// Device side of the plugins (contract sections 10.7 and 11): loads ez_battle.fal / ez_menu.fal / ez_ui.fal / ez_pc.fal / ez_casino.fal / ez_link.fal / ez_ending.fal with the firmware's
// plugin manager when their screen opens and unloads them when the scene is gone. ONE plugin at a time (they are the heap's
// biggest transient blocks and must never coexist). Pattern of the firmware example applications/examples/example_plugins_advanced:
// a composite API resolver (firmware API + the app's own symbol table, app_api_table.cpp) resolves the plugin's imports; the
// .fal files are embedded in the FAP (fal_embedded) and extracted with the app assets, so they are read from APP_ASSETS_PATH.
#include "plugin_host.h"
#include "battle_host.h"
#include "menu_host.h"
#include "ui_host.h"
#include "pc_host.h"
#include "casino_host.h"
#include "link_host.h"
#include "ending_host.h"
#include <furi.h>
#include <storage/storage.h>
#ifdef __arm__ // short_calls.h (forced include) made our own calls short; these SDK headers declare FIRMWARE functions, which must stay long calls
_Pragma("long_calls")
#endif
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <flipper_application/plugins/composite_resolver.h>
#include <loader/firmware_api/firmware_api.h>
#ifdef __arm__
_Pragma("no_long_calls")
#endif
#include "assets.h"
#include "debug.h"

#include "config.h"
#define TAG "NucleoPlugin"

extern const ElfApiInterface* const application_api_interface; // app_api_table.cpp

typedef struct {
    const char* path;    // absolute path of the .fal (the plugin manager needs it; read_at takes an absolute path too, so there is no separate relative copy: -0.14 KB resident)
    const char* appid;   // descriptor id the plugin manager filters on
    uint32_t api;        // descriptor ep_api_version
    uint32_t margin;     // heap the plugin's own scene needs on top of its code (peak of its scene state + pictures)
} PluginInfo;

static const PluginInfo INFO[] = {
    [PLUGIN_BATTLE] = {APP_ASSETS_PATH(BATTLE_PLUGIN_FILE), BATTLE_PLUGIN_APPID, BATTLE_PLUGIN_API_VERSION, 8704}, // scene peak 7.4 KB + slack (the trainer intro's hero facing 1.6 KB is freed when the intro is done, contract 22.5)
    [PLUGIN_MENU] = {APP_ASSETS_PATH(MENU_PLUGIN_FILE), MENU_PLUGIN_APPID, MENU_PLUGIN_API_VERSION, 3 * 1024},
    [PLUGIN_UI] = {APP_ASSETS_PATH(UI_PLUGIN_FILE), UI_PLUGIN_APPID, UI_PLUGIN_API_VERSION, 3 * 1024},
    [PLUGIN_PC] = {APP_ASSETS_PATH(PC_PLUGIN_FILE), PC_PLUGIN_APPID, PC_PLUGIN_API_VERSION, 3 * 1024}, // the PC visit's state + dialogs ~1.8 KB (+ the 1 KB copy buffer inside box_begin, freed at once)
    [PLUGIN_CASINO] = {APP_ASSETS_PATH(CASINO_PLUGIN_FILE), CASINO_PLUGIN_APPID, CASINO_PLUGIN_API_VERSION, 2 * 1024}, // the slot / prize state ~0.1 KB + the dialogs (C4-C, contract 37)
    [PLUGIN_LINK] = {APP_ASSETS_PATH(LINK_PLUGIN_FILE), LINK_PLUGIN_APPID, LINK_PLUGIN_API_VERSION, 4 * 1024}, // the scene (session + game ~1.8 KB), the IR driver ~0.4 KB, the result dialog (contract 45)
    [PLUGIN_ENDING] = {APP_ASSETS_PATH(ENDING_PLUGIN_FILE), ENDING_PLUGIN_APPID, ENDING_PLUGIN_API_VERSION, 4 * 1024}, // the scene (state 1.6 KB, creature pictures ~1 KB each, the 1.5 KB hof.bin image of a record, the dialogs): measured peak 3,594 B (build/c9sys c9sys_check heap), contract 50
};

// ---- what the loader needs, read from the .fal's section headers (contract 10.7, "Plugin load transient")
// The loader (flipper_application / elf_file) holds at the same time: every ALLOC section (.text as ONE block, .rodata, .data, .bss),
// the relocation data of those sections (the .fast.rel.* sections, read whole into RAM until the relocation is done; .fapmeta and
// .gnu_debuglink are counted with them, 109 B) and its own bookkeeping (ELFFile, the section map, the cache of resolved symbols,
// the File handle: ~2.5 KB estimated). .symtab / .strtab / .rel.* stay on the card and are read in small pieces, so the file size
// (8 KB of symbols in the 14 KB UI plugin) is a poor measure. Numbers for the plugins (MEM3): battle ram 31,732 + reloc 1,413
// (it was 33,118 + 3,187 before the unity build), menu 10,968 + 2,190, UI 5,596 + 1,945; `python3 tools/check_plugin_api.py` prints today's.
#define LOADER_BOOKKEEPING 2560u
static __attribute__((always_inline)) inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static __attribute__((always_inline)) inline bool elf_need(const char* file, uint32_t* block, uint32_t* ram, uint32_t* reloc) {
    uint8_t h[52];
    if(!read_at(file, 0, h, sizeof(h)) || memcmp(h, "\x7f" "ELF", 4) != 0) return false;
    uint32_t shoff = le32(h + 32);
    unsigned shentsize = (unsigned)(h[46] | (h[47] << 8)), shnum = (unsigned)(h[48] | (h[49] << 8));
    if(shentsize != 40 || shnum == 0 || shnum > 24) return false;
    uint8_t sh[24 * 40];
    if(!read_at(file, shoff, sh, shnum * 40u)) return false;
    *block = *ram = *reloc = 0;
    for(unsigned i = 0; i < shnum; i++) {
        const uint8_t* s = sh + i * 40u; // Elf32_Shdr: sh_type @4, sh_flags @8, sh_size @20
        uint32_t type = le32(s + 4), flags = le32(s + 8), size = le32(s + 20);
        if(flags & 2u) { // SHF_ALLOC
            *ram += (size + 3u) & ~3u;
            if(size > *block) *block = size;
        } else if(type == 1u) { // non-alloc PROGBITS: .fast.rel.*, .fapmeta, .gnu_debuglink
            *reloc += size;
        }
    }
    return *block != 0;
}

static struct {
    CompositeApiResolver* resolver;
    PluginManager* manager;
    const PluginBase* api;
    PluginKind kind;
} L;

void plugin_unload(void) {
    L.api = NULL;
    L.kind = PLUGIN_NONE;
    if(L.manager) plugin_manager_free(L.manager);
    if(L.resolver) composite_api_resolver_free(L.resolver);
    L.manager = NULL;
    L.resolver = NULL;
}

const void* plugin_load(PluginKind kind) {
    if(kind < PLUGIN_BATTLE || kind > PLUGIN_ENDING) return NULL;
    if(L.api) {
        if(L.kind == kind) return L.api;
        if(L.api->active()) {
            FURI_LOG_E(TAG, "plugin %d requested while %d is loaded", (int)kind, (int)L.kind);
            return NULL;
        }
        plugin_unload(); // its screens are gone (closed in this loop iteration, plugin_poll() has not run yet)
    }
    const PluginInfo* pi = &INFO[kind];
    HeapStats h0, h1;
    heap_stats_now(&h0);
    // The loader itself refuses a section that does not fit (no crash), but the scene also needs some heap on top, and the
    // scene's own allocations are guarded by heap_can_alloc, so check up front: the biggest section (.text) must fit as ONE block,
    // and free heap >= sections + relocation data + the loader's bookkeeping + the scene's margin (elf_need above).
    uint32_t block = 0, ram = 0, reloc = 0;
    bool known = elf_need(pi->path, &block, &ram, &reloc);
    uint32_t total = ram + reloc + LOADER_BOOKKEEPING + pi->margin;
    if(!known || !heap_can_alloc(block) || h0.free < total) {
        FURI_LOG_E(
            TAG, "no room for plugin %d: needs %u (block %u), free %u, largest block %u", (int)kind, (unsigned)total,
            (unsigned)block, (unsigned)h0.free, (unsigned)h0.max_blk);
        return NULL;
    }
    uint32_t t0 = furi_get_tick();
    L.resolver = composite_api_resolver_alloc();
    composite_api_resolver_add(L.resolver, firmware_api_interface);
    composite_api_resolver_add(L.resolver, application_api_interface);
    L.manager = plugin_manager_alloc(pi->appid, pi->api, composite_api_resolver_get(L.resolver));
    PluginManagerError e = plugin_manager_load_single(L.manager, pi->path);
    if(e != PluginManagerErrorNone || plugin_manager_get_count(L.manager) != 1) {
        FURI_LOG_E(TAG, "plugin %d load failed (error %d), free %u", (int)kind, (int)e, (unsigned)memmgr_get_free_heap());
        plugin_unload();
        return NULL;
    }
    L.api = plugin_manager_get_ep(L.manager, 0);
    L.kind = kind;
    heap_stats_now(&h1);
    // transient = free before - minimum since boot (when this load made the minimum); the model said sections + relocation data +
    // bookkeeping = `need`: compare them to calibrate LOADER_BOOKKEEPING
    FURI_LOG_I(
        TAG, "plugin %d loaded in %u ms: %u B of heap (free %u -> %u, largest block %u, minimum since boot %u; model: sections %u + reloc %u + bookkeeping %u)",
        (int)kind, (unsigned)(furi_get_tick() - t0), (unsigned)(h0.free - h1.free), (unsigned)h0.free, (unsigned)h1.free,
        (unsigned)h1.max_blk, (unsigned)h1.min_free, (unsigned)ram, (unsigned)reloc, (unsigned)LOADER_BOOKKEEPING);
    debug_log_stack("game thread after the plugin load", GAME_STACK); // the loader runs deep: the worst point
    return L.api;
}

PluginKind plugin_kind(void) {
    return L.api ? L.kind : PLUGIN_NONE;
}

void plugin_poll(void) {
    if(L.api && !L.api->active()) {
        plugin_unload();
        plugin_log_heap("after plugin unload");
    }
}

void plugin_log_heap(const char* what) {
    HeapStats h;
    heap_stats_now(&h);
    FURI_LOG_I(
        TAG, "heap %s: free %u, largest block %u, minimum since boot %u", what, (unsigned)h.free, (unsigned)h.max_blk,
        (unsigned)h.min_free);
}
