// Opening the UI plugin's screens from the app (contract section 11.5). Shared by the device and the host harness.
#include "ui_host.h"

static const UiPlugin* ui(void) {
    return plugin_load(PLUGIN_UI);
}

// A screen that did not open leaves the plugin loaded and idle: unload it now (never when another UI screen is on).
__attribute__((noinline)) static bool open_ui(SceneCtx* c, uint8_t what, uint8_t arg) {
    const UiPlugin* p = ui();
    const bool ok = p && p->open(c, what, arg);
    if(!ok && p && !p->active()) plugin_unload();
    return ok;
}

bool ui_open_title(SceneCtx* c) {
    return open_ui(c, UI_OPEN_TITLE, 0);
}

bool ui_open_lang(SceneCtx* c) {
    return open_ui(c, UI_OPEN_LANG, 0);
}

bool ui_open_options(SceneCtx* c) {
    return open_ui(c, UI_OPEN_OPTIONS, 0);
}

bool ui_open_name(SceneCtx* c, uint8_t who) {
    return open_ui(c, UI_OPEN_NAME, who);
}

bool ui_open_mon(SceneCtx* c, uint8_t species) {
    return open_ui(c, UI_OPEN_MON, species);
}

bool ui_open_save(SceneCtx* c) {
    return open_ui(c, UI_OPEN_SAVE, 0);
}

bool ui_open_nick(SceneCtx* c, uint8_t slot) {
    return open_ui(c, UI_OPEN_NICK, slot);
}

bool ui_open_evo(SceneCtx* c, uint8_t slot) {
    return open_ui(c, UI_OPEN_EVO, slot);
}

// A call that is not a screen: the plugin goes again at once unless a screen of it is on (a name is applied in the middle of the options
// screen, and the continue job runs under the title).
static __attribute__((always_inline)) inline void after_call(const UiPlugin* p) {
    if(p && !p->active()) plugin_unload();
}

void ui_apply_names(void) {
    const UiPlugin* p = ui();
    if(p) p->apply_names();
    after_call(p);
}

bool ui_save_load(World* w, SaveInfo* in) {
    const UiPlugin* p = ui();
    bool ok = p && p->save_load(w, in);
    after_call(p);
    return ok;
}

BcErr ui_bc_load(BcData* d, const BcIo* io) {
#ifndef NUCLEO_DEVICE
    return bc_load(d, io); // host: battle_load.c is part of battle_core.c and no plugin block is involved (a harness may call party_init() while another plugin's scene is still on: plugin_load would refuse)
#else
    const UiPlugin* p = ui();
    BcErr e = p ? p->bc_load(d, io) : BC_E_OPEN;
    after_call(p);
    return e;
#endif
}

const SceneOps* ui_title_ops(void) {
    const UiPlugin* p = ui();
    return p ? p->title_ops : (const SceneOps*)0;
}
