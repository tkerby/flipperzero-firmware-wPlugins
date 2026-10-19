// Entry point of the link plugin (ez_link.fal, contract section 45). Device only: the host links scene_link.c / link_game.c / link_proto.c / link_phy.c directly (plugin_link.c).
#include "link_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include <flipper_application/flipper_application.h>

static const LinkPlugin link_plugin = {
    .active = link_active,
    .run = link_run,
};

static const FlipperAppPluginDescriptor link_plugin_descriptor = {
    .appid = LINK_PLUGIN_APPID,
    .ep_api_version = LINK_PLUGIN_API_VERSION,
    .entry_point = &link_plugin,
};

// externally_visible: the plugin is one -fwhole-program unit (lib/link/link_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_link_ep(void) {
    return &link_plugin_descriptor;
}
