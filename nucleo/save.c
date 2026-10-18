// Save game: play time, log hook and the load of the saved game (contract section 12). The file itself (format, validation, peek / write /
// load) is save_io.c in the UI plugin: it is only touched from the title, the "Guardar" screen and the continue job. See save.h.
#include "save.h"
#include "ui_host.h"
#include "party.h"
#include "text.h"
#include "frame.h"
#include "gen/maps_gen.h"
#include <stdio.h>

void (*save_log_fn)(const char* msg) = NULL;

void save_log_why(int why) { // "save: rejected (reason N)", N = SAVE_WHY_* (save.h)
    if(!save_log_fn) return;
    char m[32];
    snprintf(m, sizeof(m), "save: rejected (reason %d)", why);
    save_log_fn(m);
}

// ---------------------------------------------------------------- play time
static uint32_t g_secs, g_ms;

void save_time_set(uint32_t secs) {
    g_secs = secs > SAVE_MAX_SECS ? SAVE_MAX_SECS : secs;
    g_ms = 0;
}

uint32_t save_time_secs(void) {
    return g_secs;
}

void save_time_tick(const SceneCtx* c, uint32_t dt_ms) {
    if(!c->n || c->st[0].ops != &scene_world_ops || g_secs >= SAVE_MAX_SECS) return; // not in a game / capped
    g_ms += dt_ms;
    if(g_ms >= 1000) {
        g_secs += g_ms / 1000;
        g_ms %= 1000;
        if(g_secs > SAVE_MAX_SECS) g_secs = SAVE_MAX_SECS;
    }
}

bool save_continue(World* w) {
    SaveInfo in;
    if(!ui_save_load(w, &in)) return false; // the UI plugin reads and applies the file (nothing changed when it says no)
    save_time_set(in.secs);
    party_pending_clear(); // (a capture parked in RAM belongs to the game that is being left)
    w->fade_hold = 0;
    w->wclock = 0;
    if(world_load_map(w, in.map, in.ti, in.tj, in.facing) != MAP_OK) {
        save_log_why(SAVE_WHY_MAP);
        return false;
    }
    if(in.ti >= w->map.w || in.tj >= w->map.h) { // a tile outside the map: the map's first spawn instead
        save_log_why(SAVE_WHY_RANGE);
        world_load_map_spawn(w, in.map, 0);
    }
    return true;
}
