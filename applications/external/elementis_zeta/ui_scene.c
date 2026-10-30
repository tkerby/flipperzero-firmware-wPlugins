// The UI plugin's shared part (contract section 11.5): the interface instance and the live-scene count that tells plugin_poll()
// when every UI screen is gone. Built into ez_ui.fal on the device and into the executables of the host harness.
#include "ui_host.h"
#include "save.h"
#include "names.h"

static int g_live;

void ui_live_enter(void) {
    g_live++;
}

void ui_live_leave(void) {
    if(g_live > 0) g_live--;
}

#ifndef NUCLEO_DEVICE
void ui_live_reset(void) {
    g_live = 0;
}
#endif

static bool ui_active(void) {
    return g_live > 0;
}

static bool ui_open_what(SceneCtx* c, uint8_t what, uint8_t arg) {
    switch(what) {
    case UI_OPEN_TITLE:
        return scene_title_open(c);
    case UI_OPEN_LANG:
        return scene_lang_open(c);
    case UI_OPEN_OPTIONS:
        return scene_options_open(c);
    case UI_OPEN_NAME:
        return scene_name_open(c, arg);
    case UI_OPEN_MON:
        return scene_mon_open(c, arg);
    case UI_OPEN_SAVE:
        return scene_save_open(c);
    case UI_OPEN_NICK:
        return scene_nick_open(c, arg);
    case UI_OPEN_EVO:
        return scene_evo_open(c, arg);
    default:
        return false;
    }
}

const UiPlugin ui_plugin_iface = {
    .active = ui_active,
    .title_ops = &scene_title_ops,
    .open = ui_open_what,
    .apply_names = names_apply,
    .save_load = save_load,
    .bc_load = bc_load,
};
