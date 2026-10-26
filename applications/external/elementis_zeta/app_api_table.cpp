// Symbol table of the app's functions that the plugins (battle, menu) import (contract sections 10.7 and 11). The firmware example
// applications/examples/example_plugins_advanced does the same: a compile-time sorted table of GNU hashes, wrapped
// in a HashtableApiInterface; plugin_loader.c puts it in a composite resolver next to the firmware's own API.
// Our headers FIRST: the firmware's <elf.h> defines EV_NONE (a macro), which would break battle_core.h's enum.
#define _Static_assert static_assert // our C headers use the C11 spelling
extern "C" {
#include "battle_core.h"
#include "bag.h"
#include "battle_host.h"
#include "blit.h"
#include "font.h"
#include "frame.h"
#include "monpic.h"
#include "party.h"
#include "world.h"
#include "render.h"
#include "scene.h"
#include "text.h"
#include "assets.h"
#include "save.h"
#include "settings.h"
#include "sound.h"
#include "ui_host.h"
#include "link_host.h"
}
#include <flipper_application/api_hashtable/api_hashtable.h>
#include <flipper_application/api_hashtable/compilesort.hpp>

#include "app_api_table_i.h"

static_assert(!has_hash_collisions(app_api_table), "Detected API method hash collision!");

constexpr HashtableApiInterface nucleo_hashtable_api_interface{
    {
        .api_version_major = 0,
        .api_version_minor = 0,
        .resolver_callback = &elf_resolve_from_hashtable,
    },
    app_api_table.cbegin(),
    app_api_table.cend(),
};

extern "C" const ElfApiInterface* const application_api_interface =
    &nucleo_hashtable_api_interface;
