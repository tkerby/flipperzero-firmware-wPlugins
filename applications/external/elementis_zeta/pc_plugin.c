// Entry point of the PC plugin (ez_pc.fal, contract sections 25 and 25.6). Device only: the host links scene_box.c / scene_card.c directly
// (plugin_link.c).
#include "pc_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include <flipper_application/flipper_application.h>

static const PcPlugin pc_plugin = {
    .active = scene_pc_active,
    .open = scene_pc_open,
    .daycare = daycare_pc_op,
};

static const FlipperAppPluginDescriptor pc_plugin_descriptor = {
    .appid = PC_PLUGIN_APPID,
    .ep_api_version = PC_PLUGIN_API_VERSION,
    .entry_point = &pc_plugin,
};

// externally_visible: the plugin is built as one -fwhole-program unit (lib/pc/pc_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_pc_ep(void) {
    return &pc_plugin_descriptor;
}
