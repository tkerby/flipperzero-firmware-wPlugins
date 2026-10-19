// Game thread: scene stack driver (poll input -> update -> render when dirty -> publish).
#include <furi.h>
#include <string.h>
#include "game.h"
#include "input.h"
#include "scene.h"
#include "world.h"
#include "settings.h"
#include "sound.h"
#include "text.h"
#include "assets.h"
#include "party.h"
#include "save.h"
#include "natives.h"
#include "battle_host.h"
#include "ui_host.h"
#include "debug.h"

// Minimum ms between renders: a world re-render is ~22 ms + compose ~12 ms on the device, so
// this caps motion at ~30 fps without starving the flip loop (same priority) when it is cheap.
#define RENDER_MIN_MS_WORLD 33
#define RENDER_MIN_MS_COMPOSE 16

static inline uint32_t tick(void) {
    return furi_get_tick();
}

static void save_log(const char* msg) { // why a save.bin was ignored / not written (serial CLI `log`)
    FURI_LOG_W("nucleo", "%s", msg);
}

// The UI plugin did not load (file missing / no heap): "Continuar" if there is a save, else a new game, straight to the world.
static void boot_without_ui(SceneCtx* c, World* w) {
    FURI_LOG_E("nucleo", "UI plugin not loaded: starting without the title");
    if(!save_continue(w)) {
        save_time_set(0);
        world_new_game(w);
    }
    scene_push(c, &scene_world_ops, NULL);
}

int32_t game_thread(void* arg) {
    Game* g = arg;
    World* w = world_alloc();
    SceneCtx* c = malloc(sizeof(SceneCtx));
    if(!w || !c) {
        if(w) world_free(w);
        if(c) free(c);
        g->quit = true;
        return 0;
    }
    g->world = w;
    scene_ctx_init(c, g);
    w->sc = c;
    // settings: the language must be active before the first text is loaded; with no valid
    // settings file (first run) the language picker runs first and its OK goes to the title
    save_log_fn = save_log;
    party_init(tick());  // battle data (about 1 KB) + party state + creature RNG seed
    natives_init();      // script natives: party / naming / Elementi popup (contract 8.4, 9)
    bool have = settings_boot();
    // the title / picker live in the UI plugin (contract 11.5); if it cannot load, start the game without them
    if(!(have ? ui_open_title(c) : ui_open_lang(c))) boot_without_ui(c, w);

    plugin_log_heap("title open"); // serial `log` (tag NucleoPlugin): the baseline of the session
    uint32_t last_update = tick();
    uint32_t last_render = last_update - RENDER_MIN_MS_WORLD;
    uint32_t deadline = last_update; // when the next update is due (wake)

    while(!g->quit && c->n) {
        battle_poll(); // unloads the battle plugin once its scene is gone (never from inside a scene callback)
        uint32_t now = tick();

        // ---- how long may we sleep?
        uint32_t timeout;
        if(c->dirty) {
            uint32_t cap = (c->dirty & D_WORLD) ? RENDER_MIN_MS_WORLD : RENDER_MIN_MS_COMPOSE;
            uint32_t since = now - last_render;
            timeout = since >= cap ? 0 : cap - since;
        } else if(c->wake_ms != SCENE_NO_WAKE) {
            timeout = deadline > now ? deadline - now : 0;
        } else {
            timeout = FuriWaitForever;
        }

        // ---- input (block until an event or the timeout), then drain
        InputEv ev;
        bool got = input_poll(&ev, timeout);
        while(got && !g->quit && c->n) {
            scene_input(c, &ev);
            got = input_poll(&ev, 0);
        }
        if(g->quit || !c->n) break;

        // ---- update
        now = tick();
        uint32_t dt = now - last_update;
        last_update = now;
        c->held = input_held_effective();
        scene_update(c, dt);
        if(!c->n) break;
        save_time_tick(c, dt); // play time (counted while a game runs)
        deadline = now + (c->wake_ms == SCENE_NO_WAKE ? 0 : c->wake_ms);

        // ---- render when dirty and the frame cap allows
        if(c->dirty) {
            uint32_t cap = (c->dirty & D_WORLD) ? RENDER_MIN_MS_WORLD : RENDER_MIN_MS_COMPOSE;
            uint32_t t0 = tick();
            if(t0 - last_render >= cap) {
                const bool world_frame = (c->dirty & D_WORLD) != 0;
                scene_render(c);
                // V1: a world frame's cap is measured from where it STARTED: render + compose (~34 ms) then fill the cap instead of
                // adding to it, and the frame rate locks to the flip loop's 39 ms cycle (one new image per cycle, 25.6 / s, instead
                // of 33 + 34 ms between publishes = 15 / s, each image shown for 1 or 2 cycles unevenly). Other frames keep the old cap.
                last_render = world_frame ? t0 : tick();
            }
        }
    }

    scene_clear(c);
    debug_log_stack("game thread", GAME_STACK); // headroom since the thread started (serial CLI `log`)
    battle_shutdown(); // after scene_clear(): the battle scene's leave() runs plugin code
    sound_shutdown(); // silence, release the speaker, free the timer (contract 28)
    // no storage handle / font cache may outlive the app (Storage never cleans up after a dead app)
    party_shutdown();
    text_shutdown();
    assets_shutdown();
    g->world = NULL;
    free(c);
    world_free(w);
    g->quit = true;
    return 0;
}
