// Entry point of the ending plugin (ez_ending.fal, contract section 50). Device only: the host links hof.c / scene_ending.c directly (plugin_link.c).
#include "ending_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include <flipper_application/flipper_application.h>

static const EndingPlugin ending_plugin = {
    .active = ending_active,
    .run = ending_run,
};

static const FlipperAppPluginDescriptor ending_plugin_descriptor = {
    .appid = ENDING_PLUGIN_APPID,
    .ep_api_version = ENDING_PLUGIN_API_VERSION,
    .entry_point = &ending_plugin,
};

// externally_visible: the plugin is one -fwhole-program unit (lib/ending/ending_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_ending_ep(void) {
    return &ending_plugin_descriptor;
}
