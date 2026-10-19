#pragma once
// Gameplay world: current map, hero step machine, NPC runtime state, global flags.
// No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "map.h"
#include "render.h"
#include "script.h"
#include "font.h" // tchar (the zone banner's name)

#define WORLD_MAX_NPCS \
    MAP_MAX_NPCS // 16 per map (tools/mapc.py enforces it): the runtime arrays below are 41 B each, in the World block
#define WORLD_FLAGS \
    1024 // global story flags 1..1023 (contract 40.1; flag 0 means "none" and is never stored); ids are u16 in the C API, 10 bits in the map format (map.h npc_show_flag & co)
#define WORLD_FLAG_BYTES (WORLD_FLAGS / 8)
// Contract 55 (roaming wild creatures, pilot): at most ROAM_MAX per map (tools/mapc.py ROAM_MAX); a creature that was fought / scared off is replaced ROAM_WAIT_STEPS hero steps later, the first ones come with the
// first step on the map, always out of view (the PC plugin places them: scene_card.c world_feat). A roamer's NpcDef carries the show flag FLAG_ROAM_OFF (never set): it is hidden by the flags alone, the plugin shows it with npc_force 2.
#define ROAM_MAX         3
#define ROAM_WAIT_STEPS  30
#define FLAG_ROAM_OFF \
    1021 // the world-system flag reserved by contract 42 (1016..1019, 1021): never set; tools/mapc.py ROAM_OFF_FLAG
#define AREA_TXT_CAP \
    20 // codes of the zone banner's name (World.area_txt), with END (contract 72: "Senda de Campeones" is 18 + END; langs.toml limits_codes keeps it)

// Set 1 to start with the NUCLEO_DEBUG overlay (dev build only) already visible.
#ifndef WORLD_DEBUG_DEFAULT
#define WORLD_DEBUG_DEFAULT 0
#endif

struct SceneCtx;

// ---- M3-B: wild encounters, shaking grass, ledges, blackout (contract section 13). Tunables live here (config.h is not ours).
#define ENC_GRACE_STEPS 1 // arrivals skipped after a map load / a battle before the next roll
// Contract 71 (the rustling grass, which replaced the timed shaking tile of 13.4): a roll on every step, a tall-grass tile near the hero rustles for 12 .. 16 steps; stepping onto it = a special wild battle.
#define RUSTLE_ROLL \
    4 // a step on a map with an encounter table rolls rand8 < RUSTLE_ROLL for a new rustling tile (4 / 256 = 1 in 64)
#define RUSTLE_STEPS \
    12 // a rustling tile lasts RUSTLE_STEPS .. RUSTLE_STEPS + RUSTLE_STEPS_SPREAD - 1 hero steps
#define RUSTLE_STEPS_SPREAD 5
#define RUSTLE_FRAME_MS     140 // animation phase length
#define RUSTLE_LV_MIN \
    3 // its creature is this much above the table's highest level (+3..+5, at most 100)
#define RUSTLE_LV_SPREAD 3
#define FLAG_DEV_RUSTLE \
    880 // contract 71, DEV / host builds only (NUCLEO_DEBUG or the host; a release build never reads it): while this story flag is set every step with no rustling tile asks for one (the roll always succeeds); 880 .. 889 are C2's range
#define HOP_MS      520 // ledge hop over 2 tiles (about 2 normal steps)
#define NPC_HOP_ARC 3 // peak of a hopping NPC's bob (SPRF_HOP sprites), px
#define HOP_ARC     9 // peak height of the hop arc, px

// Wild battle results (the values battle_open_wild writes; battle_host.h BATTLE_*, checked in natives.c)
enum {
    WILD_LOST = 0,
    WILD_WON = 1,
    WILD_RAN = 2,
    WILD_CAUGHT = 3,
    WILD_NOTOPEN = 4
}; // NOTOPEN (§72): only a script's `wild_go` VAR gets it, when no battle took place
// Post-battle chain phases (World.pb, contract 24): nickname of the captured creature -> its trip to the box -> the evolutions
enum {
    PB_NONE = 0,
    PB_NICK,
    PB_BOX,
    PB_EVO
};
#define PB_SLOTS 6 // party slots the chain looks at (= BC_MAX_PARTY, checked in natives.c)
// What the chain asks of the outside world (natives.c installs it; ops): PBH_MASK -> mask of the party slots that leveled in the last battle (0 when
// none or when the whole party is fainted: the hook also clears the mask then); PBH_CLEAR forgets the mask; PBH_NICK -> 1 when the nickname prompt of the
// captured creature was opened (a scene); PBH_BOX -> 1 when a creature waits in the pending slot and its trip to the box was opened (a scene); PBH_EVO k -> 1
// when slot k leveled, evolves now and its evolution scene was opened. (PBH_NICK / PBH_BOX = PB_NICK / PB_BOX + 1: the chain passes phase + 1.)
enum {
    PBH_MASK = 0,
    PBH_CLEAR,
    PBH_NICK,
    PBH_BOX,
    PBH_EVO
};
// Blackout phases (World.bo)
enum {
    BO_NONE = 0,
    BO_SAY1,
    BO_SAY2,
    BO_WARP
};

// Scripted NPC walk (timing state; the pose is in NpcState). Same step durations as the hero.
typedef struct {
    uint32_t t0; // wclock at the start of the current step
    uint32_t walk_px; // pixels walked before the current step (walk-cycle phase)
    uint16_t dur; // ms of the current step
    uint8_t step_px;
    uint8_t dir; // walking facing
    uint8_t steps; // steps left including the current one (0 = not walking)
} NpcWalk;

typedef struct {
    uint8_t ti, tj; // last ARRIVED tile
    uint8_t facing; // 0..7
    bool stepping;
    bool hop; // the current step is a ledge hop (lands 2 tiles away, arc)
    int8_t lift; // hop arc height in px (render)
    uint8_t to_i, to_j; // destination while stepping
    uint32_t t0; // wclock at the start of the current step
    uint16_t dur; // ms
    uint8_t step_px; // screen pixels covered by the current step
    uint8_t prog; // progress of the current step, 0..255 (set by every update; depth tile choice)
    uint32_t walk_px; // pixels walked before the current step (walk-cycle phase)
    int32_t fi, fj; // Q8 position (tile centre = i*256), valid every update
    uint8_t frame; // 0 idle, 1..4 walk
    // idle input machine (settle -> turn -> walk)
    int8_t cand; // candidate facing from the held keys, -1 none
    bool turned; // facing already turned toward cand
    uint32_t cand_t, turn_t;
    // C4-A (contract 35.3): an arrow-floor run in progress: the tile it began on and the pushes so far (0 = no run)
    uint8_t sl_i, sl_j, sl_n;
} Hero;

typedef struct World {
    // contract 77: fields ordered by how often the code touches them (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access, above it a 32-bit one): hot small fields first, big blocks last
    uint8_t fade_hold; // 0, or FADE_STEPS while a script holds the screen white (FADE_OUT)
    uint8_t bo; // blackout phase (BO_*)
    uint8_t
        psn_n; // contract 81: the hero's step counter for the poison (every 4th step a poisoned creature loses 1 HP: the counter & 3); free-running, not saved
    uint8_t
        rustle_n; // hero steps the rustle has left (0 = none; set by the casino plugin's rustle_spawn, counted down per step, dropped by a map load / a placement / a battle)
    MapErr err; // result of the last map load
    int8_t cam_npc; // camera follows this NPC (-1 = the hero)
    uint8_t area_on; // the banner runs
    // M5-B (contract 24): the post-battle chain (nickname, box, evolutions)
    uint8_t pb; // PB_* phase (0 none)
    // C5 (contract 42): act = a world feature waiting for the next update (WACT_*: the PC plugin's world_feat() continues it), push_on = Empuje is on for this map visit (cleared by every map load)
    uint8_t act, push_on;
    uint8_t
        balloon; // M4-C: NPC index + 1 that shows the "!" balloon (0 none) until wclock reaches balloon_until
    uint8_t
        rustle_ph; // the animation phase 1..RUSTLE_PHASES of the last update (read only while rustle_n != 0)
    bool ok_buf; // OK pressed mid-step, handled at arrival
    bool dbg_on; // debug overlay visible (NUCLEO_DEBUG)
    bool enc_pending; // an encounter was decided at an arrival; the battle opens at the end of the update
    uint8_t rustle_i, rustle_j; // contract 71: the rustling tile (valid while rustle_n != 0)
    int8_t
        cam_dy; // M4-C: while the camera follows an NPC, the view is shifted this many px down (room for the "!" balloon above a trainer)
    uint8_t enc_grace; // arrivals still skipped before the next encounter roll
    uint8_t enc_sp, enc_lv;
    bool wild_pending; // a wild battle is on: its result (*wild_out) is handled by the first update after it
    uint8_t wild_sp, wild_lv; // native wild_set: the species / level of the next wild_go
    uint8_t pb_k; // next party slot the PB_EVO phase looks at
    // Contract 69 (the follower): the first creature of the party able to fight walks one tile behind the hero. It is NOT an NPC of the map: its whole persistent state is the tile it stands on, fol_i / fol_j = the tile the hero
    // stood on one step ago (the trail: updated at every arrival, set behind him by world_place). Everything else (species, facing, position while he steps, hop, shown or not) is derived from the party and the hero
    // when it is drawn: world_fol_pose(). (Right after `sc`, so that the code reaching them uses the short load / store encodings: 2 B instead of 4 B each, ten places.)
    uint8_t fol_i, fol_j;
    bool enter_pending; // a map was loaded: look for an on_enter script at the next update
    bool just_loaded; // set by world_load_map, cleared by the first update
    uint8_t
        area_last; // area_id whose banner was shown last (0 none); the world scene's enter() clears it (a Continue shows it again)
    uint8_t area_off; // the slide offset of the last update (a change marks the overlay dirty)
    // wild encounters / shaking grass / blackout
    bool rng_seeded;
    // Contract 55 (roaming creatures): roam_sp / roam_lv[r] = what roamer r (the r-th NPCB_ROAM NPC of the map) is, valid while it is visible (roam_cnt is above, next to npc_force)
    uint8_t roam_n, roam_sp[ROAM_MAX],
        roam_lv[ROAM_MAX]; // (the roamers are the map's LAST roam_n NPCs: roamer r is NPC n_npcs - roam_n + r)
    struct SceneCtx* sc; // owner scene context (for the B2 hooks to push scenes)
    uint32_t wclock; // ms, advances by min(dt, WCLOCK_MAX_DT) only while the world updates
    // M4 (contract 22): animated tiles and the zone-name banner
    uint32_t aclk; // ms of the world's updates, not capped (the animation clock)
    uint32_t rng;
    uint8_t* wild_out; // where the battle writes the result (wild_res, or a script variable)
    uint32_t balloon_until;
    uint32_t anim_step; // aclk / ANIM_MS of the last update on a MAPF_ANIM map (render_anim_step)
    uint32_t area_t0; // aclk when the banner started
    Map map;
    Hero hero;
    uint8_t flags[WORLD_FLAG_BYTES];
    NpcState
        npc[WORLD_MAX_NPCS +
            1]; // runtime position / facing / frame / visibility (map.h); +1: the slot the world scene fills with the follower while it draws (contract 69: world_fol_pose into npc[n_npcs], drawn as one more NPC)
    NpcWalk npcw[WORLD_MAX_NPCS]; // scripted walk timing
    uint8_t npc_force
        [WORLD_MAX_NPCS]; // script override of the flag visibility: 0 none, 1 hidden, 2 shown
    uint8_t roam_cnt
        [WORLD_MAX_NPCS]; // contract 55 (right after npc_force: world_load_map clears both with one memset): roam_cnt[k] = of a hidden roamer NPC k that was fought (or could not be placed): hero steps until a creature is placed on it again (0 = not waiting: placed now / at the first update after the map load)
    uint32_t npc_next[WORLD_MAX_NPCS]; // wclock of the next periodic turn
    uint8_t vars[SCRIPT_VARS]; // script variables (var 0 = ASK result)
    ScriptState scr; // running script (script.c)
    uint8_t wild_res;
    // C3-A (contract 32): the in-game trade's arguments (native 23 mode 2): species wanted, species given, the nickname's text id (u16 LE).
    // C4-A (contract 35.5): the elevator's labels: tr[2k], tr[2k + 1] = the text id of row k (u16 LE), tr[10] = the number of rows (2..5); tr[11..15] unused
    uint8_t
        tr[16]; // (16: the 4-bit operand of the trade-argument store indexes it without a range check)
    tchar area_txt
        [AREA_TXT_CAP]; // gl.area.<key>, END terminated (LAST in the struct since contract 72: raising AREA_TXT_CAP 16 -> 20 must not move any other field's offset, which the code encodes)
} World;
#ifndef NUCLEO_DEVICE
extern int
    g_fol_on; // host tests only: the follower is OFF in every harness unless it turns this on (follow_check.c): the old ones keep their pictures
#endif
// Contract 69: fills `s` with the follower's render state now (visible = 0 when it is not shown: nobody able to fight, the bicycle, surfing, a map with MAP_BG_NOFOL, its tile solid / water). The frame field is its SPECIES, spr is FOL_SPR.
// The world scene calls it on npc[n_npcs] and lets render_world draw one NPC more.
void world_fol_pose(const World* w, NpcState* s);

World* world_alloc(void);
void world_free(World* w);

// Called by world_new_game() before the start map loads (the party module resets its state there). NULL = none.
extern void (*world_new_game_hook)(void);
// Clears flags and loads START_MAP at START_SPAWN. Returns the MapErr.
MapErr world_new_game(World* w);
// Loads map `id` and puts the hero on (ti,tj) facing `facing` (flags are kept).
MapErr world_load_map(World* w, uint8_t id, uint8_t ti, uint8_t tj, uint8_t facing);
// Same, at the map's spawn `spawn_id`.
MapErr world_load_map_spawn(World* w, uint8_t id, uint8_t spawn_id);
// Teleports the hero inside the current map (cancels any step).
void world_place(World* w, uint8_t ti, uint8_t tj, uint8_t facing);

// RAM relief while a scene that does not draw the world (the battle, in its plugin) is on top: the map's sprite bank (7 - 13 KB)
// AND its data block (header, ground, flags, objects, NPC / warp / interaction defs, scripts + bytecode: 0.3 - 2.3 KB, map.h
// map_data_free) are freed and read back when the battle ends. Everything the game state is made of (hero, NPC state, flags,
// vars, the running script's id / pc / stack) is untouched: the script's pc is an offset, so the bytecode may come back at
// another address. Release is idempotent; restore returns false when a block cannot be read back (the map is then dropped:
// map.w == 0, w->err says why, the world shows the "map load" plate instead of crashing). Nothing may draw or update the world
// between the two (render_world draws a blank while map.bank == NULL; the map's section pointers dangle).
void world_art_release(World* w);
bool world_art_restore(World* w);

// ---- C3-A (contract 32): bracelet modules are story flags 88..92, no save field. The bag lists the owned ones; the scripts test the flags.
enum {
    MOD_FLAG_FIRST = 88,
    MOD_CORTADOR = 88,
    MOD_LINTERNA,
    MOD_HIDRO,
    MOD_EMPUJE,
    MOD_PLANEADOR,
    MOD_COUNT = 5
};
uint32_t world_rand(World* w); // the world's xorshift RNG (native 25 `calc`, the fuse puzzle)

// ---- C4-A (contract 35): dark caves, arrow floors, the bicycle, the elevator list.
#define FLAG_BIKE \
    93 // story flag: the hero is on the bicycle (mapc builtin name `on_bike`); saved with the flags, no save-format change
#define DARK_R \
    22 // a dark map without the Linterna module is black outside a diamond around the hero: half-height DARK_R px, half-width 2 x DARK_R (a 2-tile radius), render.c darken()
// The functions below act on THE world (the one world_alloc made): the bag screen (menu plugin) and the battle code have no World pointer.
extern World*
    g_w; // THE world (world_alloc sets it): for the helpers below, which the code without a World pointer uses
static inline bool world_on_bike(void) {
    return (g_w->flags[FLAG_BIKE >> 3] >> (FLAG_BIKE & 7)) & 1;
}
// Gets on / off the bicycle (the bag's "Usar" on the key item `bici`). Getting on is refused (false, nothing changes) on a map that is an interior, a cave or dark. Either way
// world_bike_msg() is the global text id to show afterwards (gl.c4a.bike.on / off / no; a function of the state, NOT in the plugin API table: the bag derives it from the flag and the result, and the
// linker drops it from the device image). On the bike a step takes half the time; the music is the `bici` track when music.bin has one.
bool world_bike_toggle(void);
uint16_t world_bike_msg(void);
// ---- C5 world systems (contract 42): Hidrodeslizador (surf), Empuje (boulders), fishing, bike slopes
#define FLAG_SURF \
    1020 // story flag: the hero surfs (mapc builtin name `on_surf`); saved with the flags, no save-format change. Surfing and the bicycle exclude each other; a map load / placement on land clears it
#define FLAG_BADGE_TOXINA 84 // Surf needs the 5th badge, Empuje flag 83 (badge_raiz)
static inline bool world_on_surf(void) {
    return (g_w->flags[FLAG_SURF >> 3] >> (FLAG_SURF & 7)) & 1;
}
// What World.act holds: a world feature the NEXT world_update continues in the PC plugin (scene_card.c world_feat: the prompts' answers, the fishing cast, the bite). WWACT_WATER / WACT_BOULDER are
// the OK press on the water / on a boulder (called at once, not stored); WACT_LAND: a boulder arrived on a tile of a map with pits (the plugin looks for the pit under it).
enum {
    WACT_NONE = 0,
    WACT_WATER,
    WACT_BOULDER,
    WACT_SURF,
    WACT_PUSH,
    WACT_ROD0,
    WACT_ROD1,
    WACT_ROD2,
    WACT_BITE,
    WACT_LAND,
    WACT_FLY,
    WACT_FLY_GO,
    WACT_GIVE,
    WACT_ROAM,
    WACT_ROAM_HIT,
    WACT_FOLLOW,
    WACT_RUSTLE,
    WACT_DETECT,
    WACT_REGMAP,
    WACT_POISON
};
// Contract 55: WACT_ROAM = a roamer is due (the PC plugin places it out of view), WACT_ROAM_HIT (arg = its NPC index) = the hero touched it / it stepped into him (the plugin hides it, arms its respawn and queues the wild battle).
// Neither stops the hero (world.c feat()): the acts from WACT_ROAM on are not stopping ones. Contract 69: the acts from WACT_ROAM on are the CASINO plugin's (menu_open.c menu_open_daycare routes them to the casino native; WACT_FOLLOW =
// OK pressed facing the follower: its emote bubble and one line). Contract 71: WACT_RUSTLE (arg 0 = a new rustling tile is due, 1 = the hero stepped onto it) is the casino plugin's too (scene_roam.c).
// Contract 74 (F2): WACT_DETECT = the bag's Usar on the Buscatesoros (the hidden-item detector: one line of text by the nearest hidden item), WACT_REGMAP = the bag's Usar on the Atlas (the region map screen); both are the CASINO plugin's
// (scene_f2.c, through the same route as WACT_FOLLOW) and are set by the menu plugin's bag (scene_bag.c), which then closes its screens like a rod.
// Fishing (the bag's Usar on a rod: rod 0 old, 1 good, 2 super): false = the hero does not face water and nothing happens; true = the cast runs at the next update (the bag closes its screens): a text, then
// "something bites" and a wild battle from the map's fish table (mapc `@encounters fish_old|fish_good|fish_super`), or "nothing bites". An INLINE (like world_module_has / world_on_bike): the menu plugin
// compiles it in and reads `g_w` through the API table, so it costs the resident app nothing; the cast itself is the PC plugin's (scene_card.c world_feat).
// The tile the hero faces (the diagonal tile for a screen-cardinal facing): the facings' tile steps are world.c's DI / DJ, written as a formula here so the plugins need no table.
static inline void world_front(const World* w, int* i, int* j) {
    const uint8_t f = w->hero.facing & 7;
    *i = w->hero.ti + (f < 3) - (f > 3 && f < 7);
    *j = w->hero.tj + (f == 0 || f > 5) - (f > 1 && f < 5);
}
static inline bool world_fish(uint8_t rod) {
    World* w = g_w;
    int i, j;
    world_front(w, &i, &j);
    if(!(cflags_at(&w->map, i, j) & C_WATER)) return false;
    w->act = (uint8_t)(WACT_ROD0 + rod);
    return true;
}
// True when the loaded map has the header flag `f` (MAPF_*): the battle side reads MAPF_GHOSTS.
static inline bool world_map_flag(uint8_t f) {
    return (g_w->map.flags & f) != 0;
}
// The music id of the loaded map: the `bici` track while on the bike (when music.bin has one), else the map's own track (battle_open.c / natives `music_map` replay it).
uint16_t world_music_id(const World* w);

bool world_flag_get(const World* w, uint16_t id);
static inline bool world_module_has(
    const World* w,
    uint8_t
        mod) { // mod = MOD_CORTADOR .. MOD_PLANEADOR (reads the flag bits itself: the menu plugin uses it and may not call world_flag_get)
    return (w->flags[mod >> 3] >> (mod & 7)) & 1;
}
void world_flag_set(World* w, uint16_t id, bool v); // also refreshes NPC visibility
void world_refresh_npcs(World* w);

// ---- C6 (contract 46): the Planeador. The towns he has visited are story flags FLY_FLAG0 + slot (984..994, 11 slots - contract 72: 995 gruta_pit and 996 genesa_try are D1's flags: world_load_map sets the one named by the map header's `fly` byte = 1 + slot, mapc `@fly`).
// The bag's Usar on the Planeador row calls world_fly(): FLY_OK arms the flight (the bag closes its screens and the next world update opens the list of towns in the PC plugin, scene_card.c world_feat); the other
// results are refusals the bag answers with a text. An INLINE like world_fish (0 B resident; the menu plugin reads g_w through the API table).
#define FLY_FLAG0 \
    984 // first of the 11 visited-town flags (984..994; tools/mapc.py FLY_SLOTS / BUILTIN_FLAGS keep the maps off 995 / 996)
#define FLY_SLOTS 11
#define FLAG_BADGE_VOLTIO \
    82 // Thunder Badge (the card's third slot, mapc builtin `badge_voltio`): the Planeador obeys from it on
enum {
    FLY_OK = 0,
    FLY_NO_BADGE,
    FLY_NOT_HERE
};
static inline uint8_t world_fly(void) {
    World* w = g_w;
    if(!world_module_has(w, FLAG_BADGE_VOLTIO) || !world_module_has(w, MOD_PLANEADOR))
        return FLY_NO_BADGE;
    if((w->map.flags & MAPF_NOBIKE) || world_on_surf())
        return FLY_NOT_HERE; // outdoors only (not an interior, a cave or a dark map), not while surfing
    w->act = WACT_FLY;
    return FLY_OK;
}

// ---- C9 (contract 50): the League four run. Named story flags 960..983 are reserved: 960..967 = the RUN, one byte of the bitset (league_run 960 = the first room was entered, the doors behind
// are shut; league_e1..e4 961..964 = the four members beaten, league_champ 965 = the champion beaten, 966 / 967 spare for the content); a blackout (blackout_step) and the end of the Hall of Legacy clear that whole byte
// (world_league_reset), so the four members must be beaten again. 968..975 = the post-game byte, never cleared by the engine: league_champion 968 (the Hall of Legacy was entered; the PC shows "Salon del Legado"),
// league_hof_req 969 (the PC asked the script to browse the Hall of Legacy; transient). 976..983 were spare for the maintainer: D1 took them (contract 68 / 72: frag_7 976, frag_8 977, lit_5 .. lit_7 978 .. 980, the DEV clock clk_on / clk_b0 / clk_b1 981 .. 983). mapc builtin names: BUILTIN_FLAGS.
#define LEAGUE_FLAG0         960 // first flag of the run byte (a multiple of 8)
#define FLAG_LEAGUE_RUN      960
#define FLAG_LEAGUE_CHAMPION 968
#define FLAG_LEAGUE_HOF_REQ  969
static inline void world_league_reset(World* w) {
    w->flags[LEAGUE_FLAG0 >> 3] = 0;
}

// Hooks installed by natives_init() (NULL on the host harnesses that do not link the party / battle):
// start a wild battle (battle_open_wild); false = it could not open
extern bool (*world_wild_hook)(World* w, uint8_t species, uint8_t level, uint8_t* result_out);
// M5-B (contract 24): the post-battle chain's window to the party / UI code (see PBH_*); NULL = no chain. Since the M5 memory recovery it also carries
// task K's nickname prompt (PBH_NICK: called when a wild battle ended with WILD_CAUGHT, after the world is back; true = it pushed a scene).
extern uint8_t (*world_pb_hook)(World* w, uint8_t op, uint8_t k);
// The first creature able to fight (natives_init: party_ready): its LEVEL in the low byte, its SPECIES in the high byte (contract 69: the follower), 0 = none (no encounters, no shaking grass, no follower). C2-S1 (contract 29): the
// Ward Spray's step counter is the script variable VAR_REPEL (13, saved with the game): the world counts it down at every arrival and skips the wilds below this level. (Callers that want the level assign the result to a uint8_t.)
extern uint16_t (*world_party_ready_hook)(void);
// Blackout: heal the party and pay (natives_init: party_heal_all + money_lose_half)
extern void (*world_heal_hook)(void);
// Entropy for the world RNG, mixed in once at the first map load (natives_init: the creature RNG, seeded from the tick)
extern uint32_t (*world_seed_hook)(void);

// ---- M4 (contract 22): zone-name banner
#define BANNER_STEP_SHIFT \
    5 // one slide step = 1 << 5 = 32 ms (about one rendered frame), BANNER_STEP_PX rows
#define BANNER_STEP_PX 4
#define BANNER_STEPS \
    5 // steps of a slide: BANNER_STEP_PX * BANNER_STEPS = 20 rows (>= every language's plate height)
#define BANNER_TOTAL_MS \
    2048 // slide down, hold, slide up: the banner lives this long (the slides are the first / last BANNER_STEPS steps)
// Rows of the banner that are on the screen now (0 = hidden .. BANNER_STEP_PX * BANNER_STEPS); the plate is drawn with its bottom edge at this row (scene_world.c).
int world_banner_off(const World* w);

// ---- M4-C (contract 18): trainer sight, blackout
#define BALLOON_MS 600 // how long the "!" balloon stays above a trainer that saw the hero
#define BALLOON_CAM_DY \
    14 // the camera that follows the trainer puts him this many px lower: the balloon (15 px) fits above a 32 px sprite on the 64 px view
// Native 17: distance (tiles, Chebyshev) from NPC k to the hero when they are in a straight line and not adjacent (the "!" balloon then
// shows for BALLOON_MS), else 1.
uint8_t world_trainer_spot(World* w, uint8_t k);
// Native 18: NPC k starts walking towards the hero along the line, stopping next to him.
void world_trainer_come(World* w, uint8_t k);
// Native 19 / a lost trainer battle: cuts the running script and starts the blackout (13.7).
void world_blackout(World* w);
// Fade warp to the last Centro (respawn vars) or the new-game spawn: the blackout's last step and the Exit Wisp (contract 29).
void world_go_respawn(World* w);

// Starts a wild battle now (stops the hero). var >= 0: the result goes to script variable var, else to World.wild_res.
// The result is handled by the first world update after the battle (blackout when it was lost). False = not started.
bool world_start_wild(World* w, uint8_t species, uint8_t level, int var);
#ifndef NUCLEO_DEVICE
// host tests: encounter rolls made (a roll = an arrival on C_GRASS with a table and no grace) and the ones that hit
void world_enc_stats(uint32_t* rolls, uint32_t* hits);
#endif

// Collision: in bounds, not C_SOLID / C_LEDGE (a ledge is only ever crossed by its hop), no visible solid NPC (an NPC mid-step blocks both its tiles).
bool world_can_enter(const World* w, int i, int j);

// ---- script / cutscene helpers (used by script.c, usable by game code)
// Starts a walk of `steps` tiles of facing `dir` for NPC k (a step into the hero, a solid tile or a
// solid NPC stops it). Returns false if k is invalid / hidden.
bool world_npc_walk(World* w, uint8_t k, uint8_t dir, uint8_t steps);
bool world_npc_busy(const World* w, uint8_t k); // walking or mid-step
bool world_npcs_busy(const World* w); // any NPC walking
void world_npc_face(World* w, uint8_t k, uint8_t dir);
void world_npc_face_hero(World* w, uint8_t k); // quantised to the sprite's directions
void world_npc_place(World* w, uint8_t k, uint8_t i, uint8_t j, uint8_t facing);
void world_npc_show(World* w, uint8_t k, bool show); // script override until the next map load
// Hero faces NPC k.
void world_hero_face_npc(World* w, uint8_t k);
// Scripted hero walk: `steps` forced steps of facing `dir` (ignores collision, stays on the map).
bool world_hero_walk(World* w, uint8_t dir, uint8_t steps);
bool world_hero_busy(const World* w);

// Advance the world by dt ms with the held keys (KEY_* mask). Returns D_* dirty flags (scene.h
// values: 1 = D_WORLD) and fills *wake_ms with the ms until the next needed update (0 = none).
uint8_t world_update(World* w, uint32_t dt_ms, uint8_t held, uint32_t* wake_ms);

// Starts a step of `facing` now (for pushes / scripted moves). forced = ignore collision and
// slide assist. Returns true when the hero started moving.
bool world_start_step(World* w, uint8_t facing, bool forced);
// Stops walking and clears the held-key state machine (call after scene changes).
void world_stop(World* w);

bool world_is_stepping(const World* w);
void world_render_hero(const World* w, RenderHero* out);
void world_camera(const World* w, int* camx, int* camy);

// ---- hooks implemented in world.c, replaced/filled by task B2 --------------------------------
// Called when a step ARRIVES on (ti,tj): warps first, then step-on interactions. Return true if
// something took over (fade / dialog / scene push): the hero then stops instead of chaining.
bool on_step_arrival(World* w, int ti, int tj);
// OK pressed while standing (or buffered during a step, handled at arrival): interaction probe +
// dialog. Return true when it started something.
bool on_interact_pressed(World* w);
