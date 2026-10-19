// Opening and closing a battle (contract section 10.7): the memory choreography around the plugin, shared by the device
// and the host harness so both run the same steps.
//
// Heap order (the plugin is ~26 KB; loading it needs ~29 KB at once, so it is the low point of the whole run):
//   open:  draw the world once -> free the map's sprite bank -> load the plugin -> push the scene
//   close: the scene pops itself (plugin code on the stack) -> battle_scene_closed: screen white -> battle_poll() at the next
//          loop iteration: unload the plugin -> restore the bank -> fade back in.
// The plugin and the bank are never in the heap together.
#include "battle_host.h"
#include "world.h"
#include <stdlib.h>
#include <string.h>
#include "assets.h"
#include "text.h"
#include "render.h"
#include "party.h"
#include "sound.h"

uint8_t g_battle_leveled; // contract 23

// A battle just ended and the app has to finish the job once the plugin is gone (battle_poll()).
static struct {
    SceneCtx* c;
    bool pending;
} G_end;

// The world's last frame stays in `lvl` for the transition: the scene flashes / wipes it in place (no copy, nothing to
// allocate while the plugin loads). `lvl` is only a cache (a popup may have drawn into it), so the world draws it once
// more; nothing writes `lvl` between here and the scene's first draw.
static void freeze_world(SceneCtx* c) {
    if(c->world) scene_world_ops.draw_world(c, NULL);
}

static bool open_battle(SceneCtx* c, bool wild, uint8_t a, uint8_t b, uint8_t* result_out) {
    World* w = c->world;
    bool released = w && w->map.w && w->map.bank;
    plugin_log_heap("battle open"); // the world's level: the baseline of everything below
    g_battle_leveled = 0;
    sound_play(
        wild && (b & 128) ?
            SND_LEGENDARIO :
            SND_PICK + SND_TAB_TRAINER +
                (wild ?
                     0 :
                     a)); // battle music from the flash on (the timer plays it while the plugin loads); slot 0 = wild (contract 28); a wild level with bit 7 = a LEGENDARY encounter (contract 52)
    freeze_world(c);
    if(released) world_art_release(w); // before the plugin load: the heap's low point
    // More room for the loader: the font (1.4 KB Latin, 3 KB kana) and the display set that is neither shown nor pending (3 KB)
    // are not needed until the plugin is in (nothing draws, measures text or publishes in between; the flip loop keeps showing
    // the front set).
    text_font_release();
    render_cache_release(); // the hero's cached facing (1.4 KB) and the draw list + the pin layer (1.1 KB, contract 70); the battle's back view reads the facing again
    frame_set_pins(
        c->frame,
        NULL); // the frame's pin mask pointed into that block (the world's last frame stays in `lvl` for the flash: its hero columns take the world's dither phase)
    game_wait_ready(c->frame);
    bool idle_freed = frame_idle_release(c->frame);
    plugin_log_heap("before plugin load");
    const BattlePlugin* p = plugin_load(PLUGIN_BATTLE);
    if(p &&
       !party_chart(
           true)) { // the type chart (only the battle reads it, contract 27); the card failed to give it: no battle
        plugin_unload();
        p = NULL;
    }
    if(idle_freed &&
       !frame_idle_restore(
           c->frame)) { // cannot happen (the loader has freed its relocation data); if it does, make room
        plugin_unload();
        p = NULL;
        if(!frame_idle_restore(c->frame)) {
            c->game->quit =
                true; // no heap for the display at all: leave the game (scene_clear below ends the loop)
            scene_clear(c);
        }
    }
    text_font_restore();
    if(p) plugin_log_heap("after plugin load");
    bool ok = p && (wild ? p->open_wild(c, a, b, result_out) : p->open_trainer(c, a, result_out));
    if(!ok) {
        plugin_unload(); // the plugin first, then the bank: never both at once
        party_chart(false);
        if(released) world_art_restore(w);
        plugin_log_heap("battle not opened");
        if(w)
            sound_play(world_music_id(
                w)); // (the map's music again, or the bicycle's while riding: contract 28 / 35.4)
    } else {
        plugin_log_heap("battle opened"); // scene state, pictures and platforms are in
    }
    return ok;
}

bool battle_open_trainer(SceneCtx* c, uint8_t trainer, uint8_t* result_out) {
    return open_battle(c, false, trainer, 0, result_out);
}

bool battle_open_wild(SceneCtx* c, uint8_t species, uint8_t level, uint8_t* result_out) {
    return open_battle(c, true, species, level, result_out);
}

// Bank back, then the fade-in from white (the screen has been held white since the battle's exit fade).
static void world_back(SceneCtx* c) {
    party_chart(false); // the plugin is gone: the type chart goes with it
    render_cache_release(); // the battle drew the back view (facing NE): the world reads its own facing at its first draw
    if(c->world)
        world_art_restore(
            c->world); // failure: the world drops the map and shows the "map load" plate
    if(c->world)
        sound_play(world_music_id(
            c->world)); // the map's music again, the bicycle's while riding (contract 28 / 35.4)
    plugin_log_heap("after bank restore");
    if(!scene_fade_in(c)) { // no room on the scene stack (cannot happen): show the world at once
        c->fade = 0;
        c->hide_overlay = false;
        scene_dirty(c, D_WORLD);
    }
    scene_wake(c, 1);
}

void battle_scene_closed(SceneCtx* c) {
    plugin_log_heap("battle end, before unload");
    c->fade =
        FADE_STEPS; // the battle scene's last frame is white; the world below has no bank yet: keep it hidden
    c->hide_overlay = true;
    if(!battle_loaded()) { // host: no plugin to unload
        world_back(c);
        return;
    }
    G_end.c = c;
    G_end.pending = true;
    scene_wake(c, 1);
}

void battle_poll(void) {
    plugin_poll();
    if(G_end.pending && !battle_loaded()) {
        G_end.pending = false;
        world_back(G_end.c);
    }
}

bool battle_loaded(void) {
    return plugin_kind() == PLUGIN_BATTLE;
}

bool battle_closing(void) {
    return G_end.pending;
}

// App exit (after scene_clear(): the battle scene's leave() ran plugin code): unload, nothing is restored.
void battle_shutdown(void) {
    G_end.pending = false;
    plugin_unload();
    party_chart(false);
}
