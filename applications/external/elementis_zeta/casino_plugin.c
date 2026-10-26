// Entry point of the casino plugin (ez_casino.fal, contract section 37). Device only: the host links casino.c / scene_slots.c / scene_prize.c directly (plugin_link.c).
#include "casino_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include <flipper_application/flipper_application.h>

static const CasinoPlugin casino_plugin = {
    .active = casino_active,
    .run = casino_run,
};

static const FlipperAppPluginDescriptor casino_plugin_descriptor = {
    .appid = CASINO_PLUGIN_APPID,
    .ep_api_version = CASINO_PLUGIN_API_VERSION,
    .entry_point = &casino_plugin,
};

// externally_visible: the plugin is built as one -fwhole-program unit (lib/casino/casino_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_casino_ep(void) {
    return &casino_plugin_descriptor;
}
