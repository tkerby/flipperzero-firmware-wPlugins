// Entry point of the UI plugin (ez_ui.fal, contract section 11.5). Device only: the host links the scene files directly
// (plugin_link.c).
#include "ui_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include <flipper_application/flipper_application.h>

static const FlipperAppPluginDescriptor ui_plugin_descriptor = {
    .appid = UI_PLUGIN_APPID,
    .ep_api_version = UI_PLUGIN_API_VERSION,
    .entry_point = &ui_plugin_iface,
};

// externally_visible: the plugin is built as one -fwhole-program unit (lib/ui/ui_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_ui_ep(void) {
    return &ui_plugin_descriptor;
}
