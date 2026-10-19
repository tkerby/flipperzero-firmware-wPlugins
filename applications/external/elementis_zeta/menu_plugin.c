// Entry point of the menu plugin (ez_menu.fal, contract section 11). Device only: the host links scene_party.c directly
// (plugin_link.c).
#include "menu_host.h" // before the firmware headers: <elf.h> defines the macro EV_NONE (battle_core.h has an enum)
#include "scene_party.h"
#include "scene_bag.h"
#include "scene_shop.h"
#include <flipper_application/flipper_application.h>

static bool menu_active(void) {
    return scene_party_active() || scene_bag_active() || scene_shop_active();
}

static const MenuPlugin menu_plugin = {
    .active = menu_active,
    .open_party = scene_party_open,
    .open_bag = scene_bag_open,
    .open_shop = scene_shop_open_id,
    .open_pick = scene_party_open_pick,
};

static const FlipperAppPluginDescriptor menu_plugin_descriptor = {
    .appid = MENU_PLUGIN_APPID,
    .ep_api_version = MENU_PLUGIN_API_VERSION,
    .entry_point = &menu_plugin,
};

// externally_visible: the plugin is built as one -fwhole-program unit (lib/menu/menu_unity.c); this is its only exported symbol
__attribute__((externally_visible)) const FlipperAppPluginDescriptor* nucleo_menu_ep(void) {
    return &menu_plugin_descriptor;
}
