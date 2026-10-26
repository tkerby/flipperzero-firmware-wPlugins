// Entry point of the battle plugin (ez_battle.fal, contract section 10.7). Device only: the host links
// scene_battle.c directly (plugin_link.c).
#include "battle_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include "scene_battle.h"
#include <flipper_application/flipper_application.h>

static bool plugin_active(void) {
    return scene_battle_core() != NULL;
}

static const BattlePlugin battle_plugin = {
    .open_trainer = scene_battle_trainer,
    .open_wild = scene_battle_wild,
    .active = plugin_active,
};

static const FlipperAppPluginDescriptor battle_plugin_descriptor = {
    .appid = BATTLE_PLUGIN_APPID,
    .ep_api_version = BATTLE_PLUGIN_API_VERSION,
    .entry_point = &battle_plugin,
};

// externally_visible: the plugin is built as one -fwhole-program unit (lib/battle/battle_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_battle_ep(void) {
    return &battle_plugin_descriptor;
}
