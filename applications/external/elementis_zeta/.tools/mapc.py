#!/usr/bin/env python3
"""mapc.py - map compiler (parse, validate, preview, binary emit).

Commands (run from anywhere; paths are relative to the repo root):
    mapc.py check parse + validate every map in maps/maps.txt
    mapc.py preview <map|all> [out] iso overview PNG (+ <out>_grid.png top-down debug grid);
                                       default out = build/preview/map_<name>.png
    mapc.py build [--placeholder] nucleo/files/map_NN.bin, lang/<xx>/txt_NN.bin +
                                       lang/<xx>/txt_gl.bin for every enabled language of
                                       assets/lang/langs.toml, and nucleo/gen/maps_gen.h. Missing art is an error unless
                                       --placeholder (in-memory generated sprites, never written
                                       to assets/).
                                       Format v2 (task L): big actors (spritec.is_streamed) are written behind the resident bank, contract 3.1.
    mapc.py refmaps <dir> test fixture: every map_NN.bin with ALL actors resident (tools/host_world/stream_check.c)

Inputs: maps/maps.txt, maps/<name>.map, assets/catalog.toml, assets/art/**.lv.png (optional),
        assets/lang/langs.toml + the master text (assets/text/es.txt; keys are checked when it exists)

maps/maps.txt
    <id> <name> one per line (id = u8 used in map_NN.bin)
    start = <map>:<spawn> new-game position
    # comments

maps/<name>.map (column = i, row = j, tile (0,0) = top corner; '#' starts a comment line, so
'#' is not usable as a grid/legend char; grid rows contain no spaces)
    @map <name>
    @size <w> <h>
    @interior yes|no bit0 of header flags
    @cave yes|no bit3 of header flags (MAPF_CAVE, C2-S1): the Exit Wisp works here (warps to the last Centro)
    @dark yes|no bit4 of header flags (MAPF_DARK, C4-A, contract 35.1): without the Linterna module (flag 89) only a small diamond around the hero is drawn
    @fog yes|no bit6 of header flags (MAPF_FOG, C4b, contract 40.5): drifting mist over the scene (Pueblo Bruma, the Torre de los Recuerdos); the text box and the menus are not fogged
    @ghosts yes|no bit5 of header flags (MAPF_GHOSTS, C4-A, contract 35.2): the battle side reads it (world_map_flag(MAPF_GHOSTS))
    @bg <0..3> colour outside the map (header bg_level)
    @nofollow yes|no bit 2 of the header's background byte (contract 69): the follower (the first able creature walking behind the hero) stays hidden on this map: the ferry and the sea maps
    @area <key> zone-name banner (contract 22): villa_aliso ruta_1 ciudad_nodo ruta_2 bosque_umbrio pueblo_cantera -> header area_id 1..6
                                (text gl.area.<key>); interiors have none (0)
    @fill <tile> ground tile for cells whose legend has no tile=
    @spawn <name> <i> <j> <facing> facing in S SE E NE N NW W SW (contract order)
    @legend
      <char> attr=value ...
    @grid
      rows (h rows of w chars)
    @warp <char> -> <map> spawn=<name> [dirs=N,NE,..] [nofade] [oneway]
        every cell with that char becomes a warp (C_WARP); dirs = allowed arrival facings
    @pad <i> <j> -> [<map>] <i2> <j2> [face=DIR] C6 (contract 46.3): a teleport pad. The cell (i, j) must be a legend entry with `pad=yes`; stepping on it flies the
        hero to the pad at (i2, j2) of <map> (omitted = this map), arriving ON that pad facing DIR (default S, the way out) without firing it again until he steps off. A plain
        warp whose target is a pad cell; mapc checks both ends are `pad=yes` cells, the exit tile is walkable, and follows the pad as an edge in the reachability model.
    @fly <slot 0..10> <town_key> <i> <j> [face=DIR] C6 (contract 46.1 / 72: 11 slots, 984..994; 995 and 996 are D1's): this map is a town of the Planeador (Fly). slot = the town's place in the list and its visited flag (984 + slot);
        town_key = the area key whose text gl.area.<key> names the town in the list; (i, j) = the landing tile (in front of the Centro door), face = the arrival facing (default S)

    @encounters grass rate=<1..255> wild encounter table of the map; one line per slot, species = the id name of
    @end assets/data/species.toml, weight 1..255 (see contract section 13)

    @roam <n> [table=grass] [area=<legend chars>] [period=<ms>] contract 55 (PILOT: Ruta 1): up to n (1..3) wild creatures of the map's grass table wander the open ground and a touch starts that
        wild battle. area= = the legend chars they may stand on (default: every walkable cell that is not tall grass; never water, doors, triggers, ledges, solids, the map's border cells);
        period = ms between two of their steps (default 1600). Needs `@encounters grass` and the actors of tools/roam_ow.py (assets/catalog_roam.toml); takes the n LAST NPC slots (cap 16: an error when they do not fit)
    @open_rate <0..255> contract 63: wild encounters on OPEN ground (a walkable tile with no flag at all: not tall grass, water, counter, door / warp, step trigger, ledge or arrow floor / bike slope):
        style, encounter when rand8 < n on a step onto it, drawn from the map's grass table (Repel and the bicycle as in the tall grass; surfing keeps its water table). Needs `@encounters grass`; default 0
    @flag <NAME> <n> name a global flag (1..1023, MAX_FLAG; contract 40) for scripts; also written to maps_gen.h
    @var <NAME> <n> name a script variable (0..15; var 0 = ASK result)
    @script <name> [on_enter|interact|npc|step|called] [if [!]FLAG]
      <opcode> args one instruction per line, `label:` lines, `#` comments
    @end see "Scripts" below and contract section 8

Legend attributes (all names are LOGICAL names resolved through catalog [alias]/[variants]):
    tile=<name|group|none> ground tile ('none' = nothing drawn, 0xFF)
    object=<name> part of a multi-cell footprint: the rectangle of cells with the same
                             object name must exactly match the catalog fw x fh (first cell in
                             row-major order = min corner)
    skirt=<name> additional 1x1 object stacked on the same cell, drawn before the object
    door=<name> cell of that object's footprint that is a walkable door; must be one
                             of the sprite's door_tiles and must have a matching @warp
    ledge=SW|SE|NE|NW one-way ledge: tile ledge_<dir> (C_LEDGE) + the jump direction (a world axis: SW = +j, SE = +i,
                             NE = -j, NW = -i); the hero hops over it onto the tile beyond (contract section 13)
    interact=look|step InteractDef: look = OK while facing (mode 0), step = on step-onto (mode 1)
      kind=text|flag|push (default text) text = 0, flag = SET_FLAG+TEXT (2, needs flag=N),
                             push = TEXT then push hero back (3)
      text=<key> text key (default: catalog `text` of the object)
      flag=<n> dirs=<list> optional (dirs = allowed player facings, default any)
      script=<name> run the @script (kind 4, no text= needed; step triggers too)
    npc=<actor> face=<facing> text=<key> [talk_flag=N] [show_flag=N] [hide_flag=N]
        [behavior=static|turn|wander period=<ms> radius=<tiles>] [solid=yes|no] [gate=yes] [id=<name>] [script=<name>]
        script= replaces text= (talking runs the script); id= is a handle for scripts
        talk_flag: first talk shows <key>, later talks show the same key with a trailing
        ".a" replaced by ".b" (text_id+1) - both keys must exist and be emitted adjacently.
        npc= may be combined with object= (the NPC stands ON that furniture, e.g. a Crystal on the lab bench).
    keydoor=<item> [npc=<actor>] [hide_flag=<flag>] C6 (contract 46.2): a card-key door: a solid script NPC (default actor `keydoor`). Without the key item it says gl.c6.door.locked;
        with it: the `door` sound, its hide flag is set (it vanishes for good), gl.c6.door.open. The key stays in the bag. A gate for the reachability checks.
    gift=<species> level=<1..100> [npc=<actor>] [hide_flag=<flag>] C6 (contract 46.4): a gift creature to CHOOSE among several: give every one of the group the SAME hide_flag (an @flag name or
        a number): taking one (ask gl.c6.gift.ask, the picture shows) gives it (a full party sends it to the PC box), sets the flag and hides the whole group. Default actor `item_bag`.
    hidden=<item> [qty=N] [hide_flag=<flag>] F2 (contract 74): a HIDDEN item: a walkable floor tile with nothing drawn; OK while facing it gives the item once (give_item, gl.found, the `item` jingle), the
        pick-up flag is `hide_flag=` or automatic (item_<map>_<i>_<j>). It is a face-mode script interaction whose Interact.flag is that flag (the only INT_SCRIPT with a flag): the Buscatesoros key item finds exactly those
    pad=yes C6 (contract 46.3): a teleport pad cell (plain floor, no solid / object / NPC); every one needs a `@pad` line

Scripts: opcodes are lower-case mnemonics (OPS below); operands: text keys, flags (number or @flag
name), vars (number or @var name), npc (legend char of the NPC, its id=, its actor name, or #index),
dirs (S SE E NE N NW W SW or 0..7), labels (`name:`), script names. Example:
    @script orb_a npc
        say orb.a.look
        ask orb.a.take
        jv 0 0 no # var 0 = 1 yes / 0 no
        npc_hide orb_a
        setf 7
        say orb.a.took
        end
      no:
        say orb.a.left
    @end
C4-A macros (contract section 35): `caught_count VAR` (VAR = the species caught in the Registro), `caught_ge VAR N` (VAR = 1 when at least N are caught) and `elevator VAR KEY1 KEY2 [.. KEY5]` (a 2..5 row list of text keys, usually the
`gl.c4a.floor.*` ones; VAR = the chosen row 0..n-1, 255 = Back; follow it with `jv VAR n label` and a `warp` per floor). C4-C macros (contract 37.3 / 38.7, the casino native 22 `pc`): `coins_buy VAR` (the clerk sells 50 coins for $1000: VAR = 1 bought / 2 no money / 3 no Coin Purse / 4 full / 0 plugin), `slots VAR`
(the slot machine, waits until Back: VAR = 0 played / 1 no case / 2 no coins / 3 not opened), `prizes SHOP VAR` (the prize counter of a `coins = true` shop of shops.toml, by id name or number: VAR = 0 visited / 1 no case / 3 refused)
and `coins_get VAR` (VAR = the coins capped at 255, `{v2}` = the exact number). Each is one `call_native 22` with the same bytes as the raw `native pc A B`. Arrow floors are TILES with the catalog property `slide = "<facing>"`
(arrow_s .. arrow_sw in assets/catalog.toml): the legend says `tile=arrow_se`; the checker simulates the runs (loops are warned, the reachability BFS follows them).
C9 (contract section 50, the endgame): `ending run VAR` (the champion's ending: the Hall of Legacy screens, the record in hof.bin, league_champion set + the run flags cleared, the game saved with the hero at home,
the credits, FIN; VAR = 1 ran / 0 could not open) followed by `ending title` (fade out to the title), `ending browse VAR` (the Hall of Legacy browser) and `ending credits VAR` (the roll alone, a preview) are native 22 `pc`
with the operation 14 << 4 | VAR. `pc` (the PC terminal) is the box screen followed by the browser when the PC menu's "Salon del Legado" row asked for it. The League four's flags are builtin names (BUILTIN_FLAGS):
league_run 960 (the first room was entered: the doors behind are shut), league_e1 .. league_e4 961..964 (the members' beaten flags), league_champ 965, league_run1 / league_run2 966 / 967 (spare), all CLEARED by a
blackout and by the end of the Hall of Legacy; league_champion 968 (post-game, saved) and league_hof_req 969 (transient). The league rooms need no engine support: a door is a solid NPC with `show_flag=league_run`
(the door behind: it appears when the first room sets the flag) or `hide_flag=league_eN` (the door ahead: gone once the member's beaten flag is set) standing on the tile IN FRONT of a warp cell (`gate=yes` for the
reachability checks; mapc refuses an NPC ON a warp cell); a member is `trainer=<id> beaten_flag=league_eN` (or a hand-written script that does `setf league_eN` after a win and `blackout` after a loss); the first room's
on_enter script does `jf league_run in / sfx door / setf league_run / say <door shut text> / in:`; `@warp ... oneway` silences the "has no warp back" warning of the lobby -> first room warp.
Contract 65 (friendship): `bond tier VAR` (VAR = the lead creature's friendship tier 0 cold .. 4 devoted, 255 = no creature; the text variable `{v0}` = its name) and `bond heal VAR` (before `heal_party` in a Centro: +1 friendship for every
creature that needs the healing; VAR = how many) = native 22 `pc` with the operation 15 << 4 | VAR (the casino plugin's CAS_BOND), no resident code.
C7 (contract section 53): `setv_sp VAR SPECIES` = `setv VAR <species id>` by NAME (the ids move when chapters merge): `setv_sp sp sealily` then `name_var 0 sp` / `mon_show sp`; the builtin flag `badge_brasa` = 86 (the 7th badge);
`@area` keys ruta_19 .. ruta_21, islas_escarcha, isla_ceniza, mansion_calcinada, central_electrica.
Text ids: per map = index in the map's ordered key list (talk_flag NPC: ".a" immediately followed
by ".b" so b = a+1); global ("gl.") keys = 0x8000 | index in master (es.txt) order; the same ids in every language; 0xFFFF = none.
Spawn ids = source order (0..n-1). See text_plan, emit_v1, emit_header_h.

Internal API (for the emitter): load_world -> World with .maps (ordered by id), .start,
.catalog. Each Map has ground[], cflags[], objects[], npcs[], warps[], interacts[], spawns[]
and bank[] already in the binary's semantics (contract 3.1); `Map.text_keys` lists keys used.
"""
import os
import re
import sys

try:
    import tomllib
except ImportError:  # Python < 3.11 (e.g. the 3.9 .venv): needs `pip install tomli`
    import tomli as tomllib
import zlib
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---- contract constants -------------------------------------------------------------------
C_SOLID, C_GRASS, C_WATER, C_COUNTER, C_WARP, C_TRIGGER, C_LEDGE, C_SLIDE = (
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    128,
)  # C_SLIDE (C4-A, contract 35.3): an arrow floor; its facing is in the bank entry's flags bits 4..6
LEDGE_DIRS = {
    "SE": 1,
    "NE": 3,
    "NW": 5,
    "SW": 7,
}  # jump facing (world axes: +i, -j, -i, +j)
(
    NPCB_STATIC,
    NPCB_TURN,
    NPCB_WANDER,
    NPCB_TRAINER,
    NPCB_BOULDER,
    NPCB_HOLE,
    NPCB_ROAM,
) = (
    0,
    1,
    2,
    3,
    4,
    5,
    6,
)  # NpcDef.behavior low nibble; the high nibble = wander radius / trainer range
MAP_VERSION = 2  # map_NN.bin format version (2: bank_res at header offset 18, streamed actors, contract 3.1)
(
    MAPF_INTERIOR,
    MAPF_BALLOON,
    MAPF_ANIM,
    MAPF_CAVE,
    MAPF_DARK,
    MAPF_GHOSTS,
    MAPF_FOG,
    MAPF_PITS,
) = (
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    128,
)  # map header flags (balloon: the LAST bank sprite is the "!" balloon, contract 18; anim: a tile / object of the bank has nframes > 1, contract 22)
AREA_KEYS = (
    "villa_aliso",
    "ruta_1",
    "ciudad_nodo",
    "ruta_2",
    "bosque_umbrio",
    "pueblo_cantera",
    "ruta_3",
    "monte_eco",
    "ruta_4",
    "ciudad_marea",
    "ruta_24",
    "ruta_25",
    "ruta_5",
    "ruta_6",
    "puerto_dinamo",
    "ruta_11",
    "cueva_excavadora",
    "ruta_9",
    "ruta_10",
    "tunel_oscuro",
    "pueblo_bruma",
    "ruta_8",
    "ruta_7",
    "metropolis_neon",
    "sala_juegos",
    "escondite_vertice",
    "torre_recuerdos",
    "ruta_12",
    "ruta_13",
    "ruta_14",
    "ruta_15",
    "ciudad_ambar",
    "ruta_16",
    "autopista",
    "ruta_18",
    "reserva",
    "mar_ambar",
    "ciudad_cenit",
    "torre_vertice",
    "ruta_19",
    "ruta_20",
    "ruta_21",
    "islas_escarcha",
    "isla_ceniza",
    "mansion_calcinada",
    "central_electrica",
    "ruta_22",
    "ruta_23",
    "senda_campeones",
    "meseta_cumbre",
    "cueva_secreta",
    "ruinas_origen",
    "gruta_eclipse",
)  # @area keys (C4a: the seven before the last three; C4b: sala_juegos, escondite_vertice, torre_recuerdos; C5a: ruta_12 .. ruta_15, ciudad_ambar; C5b: ruta_16, autopista, ruta_18, reserva, mar_ambar; C6: ciudad_cenit, torre_vertice; C7 (contract 53): ruta_19 .. ruta_21, islas_escarcha, isla_ceniza, mansion_calcinada, central_electrica; C9 (contract 56): ruta_22, ruta_23, senda_campeones, meseta_cumbre, cueva_secreta; D1 (contract 68): ruinas_origen, gruta_eclipse); area_id = 1 + index, 0 = none (text gl.area.<key>, contract 22)
BALLOON_SPRITE, ITEM_SPRITE = (
    "emote_excl",
    "item_bag",
)  # catalog names (tools/gen_tiles.py)
CUT_SPRITE = "cut_bush"  # default actor of the legend `cut=yes` (a cuttable bush, C3-A, contract 32)
BOULDER_SPRITE, HOLE_SPRITE = (
    "boulder_rock",
    "hole_pit",
)  # default actors of the legend `boulder=yes` / `hole=FLAG` (C5, contract 42.2)
LEGEND_FX = "legend_cry"  # the cry of a legendary creature in the `legend=` script (an `fx` of assets/music/index.txt, C7 / C9, contract 52)
SLEEPER_FX = "heal"  # the jingle of the flute that wakes a sleeper (an `fx` of assets/music/index.txt, C5, contract 42.4)
TRAINER_RANGE = 4  # default `range=` of a trainer (tiles)
BUILTIN_VARS = {
    "respawn_map": 14,
    "respawn_spawn": 15,
    "repel": 13,
}  # script vars the engine reads (blackout respawn, contract 18.3; the Repel's step counter, C2-S1, contract 29)
# story flags the engine / the bag / the trainer card read, named for every map (C3-A, contract 32): the badges 80..87 (party.h FLAG_BADGE_FIRST; a map may still declare the
# same @flag), the five bracelet modules 88..92 (world.h MOD_*)
KEYDOOR_SPRITE = "keydoor"  # default actor of the legend `keydoor=<item>` (a card-key door, C6, contract 46.2)
KEYDOOR_FX = (
    "door"  # the sound of a key door opening (an `fx` of assets/music/index.txt)
)
GIFT_FX = "item"  # the jingle of a gift (`gift=`, C6 contract 46.4)
FLY_FLAG_BASE, FLY_SLOTS, FLY_LABEL_MAX = (
    984,
    11,
    15,
)  # the Planeador's visited flags 984..994 (nucleo/world.h FLY_FLAG0, contract 72: 995 gruta_pit and 996 genesa_try are D1's, so slots 11 and 12 would collide); the list label holds 15 codes (scene_card.c LIST_CAP 16)
BUILTIN_FLAGS = {
    "badge_cantera": 80,
    "badge_marea": 81,
    "badge_voltio": 82,
    "badge_raiz": 83,
    "mod_cortador": 88,
    "mod_linterna": 89,
    "mod_hidro": 90,
    "mod_empuje": 91,
    "mod_planeador": 92,
    "badge_toxina": 84,  # C5: the fifth badge (Surf needs it, contract 42.1)
    "badge_mente": 85,  # C6: the sixth badge (the psychic gym)
    "badge_brasa": 86,  # C7 (contract 53): the seventh badge, the Medalla Brasa of Profesor Brasa (the fire gym of Isla Ceniza)
    "badge_nucleo": 87,  # C8 (contract 51): the eighth badge, the Medalla Nucleo of Director Varo
    "on_bike": 93,
    "on_surf": 1020,  # 93 = the hero is on the bicycle (C4-A, contract 35.4; world.h FLAG_BIKE; saved with the flags)
    # C9 (contract 50.1): the League four run, named story flags 960..983 (world.h LEAGUE_FLAG0). 960..967 are CLEARED by a blackout and by the end of the Hall of Legacy; 968.. never
    "league_run": 960,
    "league_e1": 961,
    "league_e2": 962,
    "league_e3": 963,
    "league_e4": 964,
    "league_champ": 965,
    "league_run1": 966,
    "league_run2": 967,
    "league_champion": 968,
    "league_hof_req": 969,
    # D1 (contract 68): the legend of the eight Origin Fragments, the Ruinas del Origen and the Gruta del Eclipse: D1's block 950 .. 999 (nucleo/legend.h has the same numbers)
    "legend_rumor": 951,
    "ruins_open": 952,
    "ruins_solved": 953,
    "genesa_gone": 954,
    "cave_open": 955,
    "gruta_pit": 995,
    "genesa_try": 996,  # 995 = the Gruta's pit (the engine sets it when the boulder fills it); the Planeador has 11 slots (984..994): FLY_SLOTS in mapc and world.h, and the @fly check below, keep it off 995 / 996
    "lit_1": 956,
    "lit_2": 957,
    "lit_3": 958,
    "lit_4": 959,
    "lit_5": 978,
    "lit_6": 979,
    "lit_7": 980,  # the pedestals lit so far, in the mural's order (the 8th is ruins_solved)
    "frag_1": 970,
    "frag_2": 971,
    "frag_3": 972,
    "frag_4": 973,
    "frag_5": 974,
    "frag_6": 975,
    "frag_7": 976,
    "frag_8": 977,
    "clk_on": 981,
    "clk_b0": 982,
    "clk_b1": 983,
}  # DEV / host clock override (casino plugin, `clock`)
# flags handed out to trainers / ground items (maps/auto_flags.txt keeps them stable). C4a (contract 39) widened the pool: 100..199 were all taken by chapters 1 - 4a, so it goes on with the numbers no map, no engine
# constant and no test fixture uses (2..9, 94..99 stay free: the world systems' reserve): 58..79, 207..219, 221..229, 17..29, 32..39
LEGACY_AUTO_POOL = tuple(
    list(range(100, 200))
    + list(range(58, 80))
    + list(range(207, 220))
    + list(range(221, 230))
    + list(range(17, 30))
    + list(range(32, 40))
)  # chapters 1 - 4a handed out these (the lines of maps/auto_flags.txt are still valid)
# C4b / save format 5 (contract 40): flag ids are 10 bits (1..MAX_FLAG). The automatic pool is now ONE fresh contiguous range above everything used so far (the legacy numbers above stay valid for the lines already in
# maps/auto_flags.txt, nothing new is given from them); the hand-named story flags of the next chapters (@flag) take STORY_FLAG_RANGE; ids 2..9 / 94..99 and the unused rest of the legacy pool stay free.
MAX_FLAG = 1023
FLAG_OPS = (
    "jf",
    "jnf",
    "setf",
    "clrf",
)  # the opcodes whose flag id has its bits 8..9 in bits 6..7 of the opcode byte (contract 40.3)
AUTO_FLAG_POOL = tuple(range(256, 768))  # 512 numbers for the trainers / ground items
STORY_FLAG_RANGE = range(
    768, 1024
)  # 256 numbers for named story flags (documentation only: mapc accepts any free number)
AUTO_FLAG_VALID = frozenset(LEGACY_AUTO_POOL) | frozenset(AUTO_FLAG_POOL)
TEST_MAP_BASE = 200  # map ids >= 200 are engine test fixtures (maps/test_*.map); real maps use 0..199 (97 .. 99 are also the NAMES of the v1 host fixtures of make_test_map.py in build/m1_test; since contract 49 real maps go past 99 and nothing in nucleo/files is called map_99.bin)
FIXTURE_FLAG_POOL = range(
    230, 256
)  # the same for the test maps (ids >= TEST_MAP_BASE), not remembered
AUTO_FLAGS_FILE = "auto_flags.txt"  # in maps/: `name id` per line, never reassigned
MAX_ENC = 16
ROAM_OFF_FLAG = 1021  # contract 55: the show flag of a roamer's NpcDef: never set, so the roamer is hidden until the PC plugin shows it (nucleo/world.h FLAG_ROAM_OFF)
ROAM_NONE = 0xFF  # contract 66: the roam block's actor index of a slot that never roams (an affinity Solar form; nucleo/map.h MAP_ROAM_NONE)
ROAM_MAX, ROAM_PERIOD = (
    3,
    1600,
)  # contract 55: roamers per map (nucleo/world.h ROAM_MAX) / the default ms between two of their steps (`@roam ... period=`)
WILD_TABLES = (
    "water",
    "fish_old",
    "fish_good",
    "fish_super",
)  # C5 (contract 42.1): the extras tail's tables, in this order (map.h TBL_*)
FISH_RATE = {
    "fish_old": 255,
    "fish_good": 128,
    "fish_super": 255,
}  # the default chance of a bite (255 = always: the reed rod always bites, the fiber rod about half the time)
CFLAG_NAMES = {
    "solid": C_SOLID,
    "grass": C_GRASS,
    "water": C_WATER,
    "counter": C_COUNTER,
    "warp": C_WARP,
    "trigger": C_TRIGGER,
    "ledge": C_LEDGE,
}
FACINGS = ["S", "SE", "E", "NE", "N", "NW", "W", "SW"]
DI = [1, 1, 1, 0, -1, -1, -1, 0]
DJ = [1, 0, -1, -1, -1, 0, 1, 1]
KIND_TILE, KIND_OBJECT, KIND_ACTOR = 0, 1, 2
KIND_IDS = {"tile": KIND_TILE, "object": KIND_OBJECT, "actor": KIND_ACTOR}
IK_TEXT, IK_FLAG, IK_PUSH, IK_SCRIPT = 0, 2, 3, 4
SCRT_NAMES = {"called": 0, "on_enter": 1, "interact": 2, "npc": 3, "step": 4}
NPCF_SOLID, NPCF_SCRIPT = 1, 2
NO_TILE = 0xFF
ORANGE = (255, 139, 41)
INK = (24, 14, 6)


# ---- reporting ------------------------------------------------------------------------------
class Report:
    def __init__(self):
        self.errors, self.warnings, self.notes = [], [], []

    def err(self, where, msg):
        self.errors.append(f"{where}: {msg}")

    def warn(self, where, msg):
        self.warnings.append(f"{where}: {msg}")

    def note(self, msg):
        self.notes.append(msg)


# ---- catalog --------------------------------------------------------------------------------
def test_catalog_path(catalog_path):
    """maps/test_catalog.toml next to the maps of this catalog's repo (extra sprites for the test maps), or None."""
    p = Path(catalog_path).resolve().parent.parent / "maps" / "test_catalog.toml"
    return p if p.is_file() else None


class Catalog:
    def __init__(self, path, rep):
        self.path = path
        d = tomllib.load(open(path, "rb"))
        self.extra = test_catalog_path(
            path
        )  # maps/test_catalog.toml: sprites of the test maps (ids >= TEST_MAP_BASE) only
        roam = (
            Path(path).resolve().parent / "catalog_roam.toml"
        )  # contract 55: GENERATED by tools/roam_ow.py: the `roam_<species>` actors of the @roam maps
        self.extras = [p for p in (self.extra, roam if roam.is_file() else None) if p]
        for xp in self.extras:
            e = tomllib.load(open(xp, "rb"))
            d["sprite"] = list(d.get("sprite", [])) + list(e.get("sprite", []))
            d["alias"] = {**d.get("alias", {}), **e.get("alias", {})}
            d["variants"] = {**d.get("variants", {}), **e.get("variants", {})}
        self.sprites = {}
        self.alias = d.get("alias", {})
        self.variants = d.get("variants", {})
        self._art = {}
        for s in d.get("sprite", []):
            n = s.get("name")
            if not n or n in self.sprites:
                rep.err("catalog", f"missing/duplicate sprite name {n!r}")
                continue
            if s.get("kind") not in KIND_IDS:
                rep.err("catalog", f"{n}: bad kind {s.get('kind')!r}")
                continue
            s.setdefault("fw", 1)
            s.setdefault("fh", 1)
            s.setdefault("cflags", [])
            s.setdefault("door_tiles", [])
            s.setdefault("ndirs", 8 if s["kind"] == "actor" else 1)
            s.setdefault("nframes", 1)
            if (
                "frames" in s
            ):  # an animated tile / object (contract 22): `frames = [".lv.png", ...]` instead of `src`
                fr = s["frames"]
                if (
                    s["kind"] == "actor"
                    or "src" in s
                    or not isinstance(fr, list)
                    or not 2 <= len(fr) <= 8
                    or not all(isinstance(x, str) for x in fr)
                ):
                    rep.err(
                        "catalog",
                        f"{n}: frames = [2..8 paths] is for tiles / objects and replaces src",
                    )
                    continue
                s["nframes"] = len(fr)
            s["_cf"] = 0
            for c in s["cflags"]:
                if c not in CFLAG_NAMES:
                    rep.err("catalog", f"{n}: unknown cflag {c!r}")
                else:
                    s["_cf"] |= CFLAG_NAMES[c]
            s["_slide"] = (
                None  # C4-A (contract 35.3): `slide = "<facing>"` on a tile = an arrow floor (C_SLIDE + the push facing)
            )
            s["_bike"] = (
                False  # C5 (contract 42.6): `slide_bike = "<facing>"` = a bike slope: the same push, but only while riding the bicycle (directory flags bit 7)
            )
            for key in ("slide", "slide_bike"):
                if key in s:
                    if (
                        s["kind"] != "tile"
                        or s[key] not in FACINGS
                        or ("slide" in s and "slide_bike" in s)
                    ):
                        rep.err(
                            "catalog",
                            f"{n}: {key} = one of {' '.join(FACINGS)} (tiles only, not both), got {s[key]!r}",
                        )
                    else:
                        s["_slide"] = FACINGS.index(s[key])
                        s["_bike"] = key == "slide_bike"
                        s["_cf"] |= C_SLIDE
            if s["kind"] == "tile" and (s["fw"], s["fh"]) != (1, 1):
                rep.err("catalog", f"{n}: tiles must be 1x1")
            if s["ndirs"] not in (1, 4, 8):
                rep.err("catalog", f"{n}: ndirs must be 1, 4 or 8")
            for dt in s["door_tiles"]:
                if not (0 <= dt[0] < s["fw"] and 0 <= dt[1] < s["fh"]):
                    rep.err(
                        "catalog",
                        f"{n}: door tile {dt} outside {s['fw']}x{s['fh']} footprint",
                    )
            self.sprites[n] = s
        # alias lint
        for a, t in self.alias.items():
            targets = t if isinstance(t, list) else [t]
            for x in targets:
                if x == a:
                    continue
                if x not in self.sprites and x not in self.variants:
                    rep.err("catalog", f"alias {a} -> unknown sprite {x!r}")
            if a in self.sprites and not isinstance(t, list) and t in self.sprites:
                A, B = self.sprites[a], self.sprites[t]
                if (A["fw"], A["fh"]) != (B["fw"], B["fh"]) or A["kind"] != B["kind"]:
                    rep.err(
                        "catalog",
                        f"alias {a} -> {t}: footprint/kind mismatch "
                        f"({A['fw']}x{A['fh']} vs {B['fw']}x{B['fh']})",
                    )
            if a in self.sprites and isinstance(t, list):
                A = self.sprites[a]
                if len(t) != A["fw"] * A["fh"]:
                    rep.err(
                        "catalog",
                        f"alias {a}: list has {len(t)} entries, footprint is "
                        f"{A['fw']}x{A['fh']}",
                    )
                for x in t:
                    if x in self.sprites and (
                        self.sprites[x]["fw"],
                        self.sprites[x]["fh"],
                    ) != (1, 1):
                        rep.err("catalog", f"alias {a}: list entry {x} is not 1x1")
        for g, lst in self.variants.items():
            for x in lst:
                if x not in self.sprites:
                    rep.err("catalog", f"variant group {g}: unknown tile {x!r}")

    # logical name -> ('one', spritename) | ('tiles', [spritenames]) | None
    def resolve(self, name, i=0, j=0):
        for _ in range(8):
            t = self.alias.get(name, name)
            if isinstance(t, list):
                return ("tiles", list(t))
            if t == name:
                break
            name = t
        if name in self.variants:
            lst = self.variants[name]
            h = (i * 73856093 ^ j * 19349663) & 0xFFFFFFFF
            h ^= h >> 13
            h = (h * 0x5BD1E995) & 0xFFFFFFFF
            h ^= h >> 15
            return ("one", lst[h % len(lst)])
        if name in self.sprites:
            return ("one", name)
        return None

    def dims(self, logical, i=0, j=0):
        """(fw, fh) of a logical object: the catalog entry of that name, else of its target."""
        if logical in self.sprites:
            s = self.sprites[logical]
            return s["fw"], s["fh"]
        r = self.resolve(logical, i, j)
        if r and r[0] == "one":
            s = self.sprites[r[1]]
            return s["fw"], s["fh"]
        return None

    def art(self, name):
        """Loaded .lv.png art for a tile/object sprite or None: dict(w,h,ax,ay,img RGBA)."""
        if name in self._art:
            return self._art[name]
        res = None
        s = self.sprites.get(name)
        if s and s["kind"] in ("tile", "object"):
            p = ROOT / (s["src"] if "src" in s else s["frames"][0])
            if p.is_file():
                try:
                    from PIL import Image
                    import json

                    im = Image.open(p).convert("RGBA")
                    w, h = im.size
                    ax, ay = (
                        (w // 2, h // 2) if s["kind"] == "tile" else (w // 2, h - 1)
                    )
                    jp = (
                        p.with_name(p.name[: -len(".lv.png")] + ".json")
                        if p.name.endswith(".lv.png")
                        else None
                    )
                    if jp and jp.is_file():
                        jd = json.load(open(jp))
                        ax, ay = jd.get("ax", ax), jd.get("ay", ay)
                    res = dict(w=w, h=h, ax=ax, ay=ay, img=im)
                except Exception as e:  # noqa
                    print(f"warning: cannot load {p}: {e}", file=sys.stderr)
        self._art[name] = res
        return res


# ---- map data -------------------------------------------------------------------------------
@dataclass
class Obj:
    spr: int  # bank index
    name: str  # sprite name
    i0: int
    j0: int
    fw: int
    fh: int
    flags: int = 0


@dataclass
class Npc:
    spr: int
    name: str
    i: int
    j: int
    facing: int
    behavior: int = 0
    flags: int = 1
    text_key: str = ""
    text_key2: str = ""  # shown on later talks when talk_flag != 0
    show_flag: int = 0
    hide_flag: int = 0
    param: int = 0
    talk_flag: int = 0
    script: str = ""  # NPC talk runs this @script (flags bit1, text_id = script id)
    ch: str = ""  # legend char that placed it (script reference)
    ident: str = ""  # legend id=<name> (script reference)
    gen: str = (
        ""  # "trainer" | "item": the NPC (and its script) come from `trainer=` / `item=` (contract 18)
    )
    flag_tok: str = (
        ""  # trainer: its beaten flag / item: its hide flag, as a script token (a name or a number)
    )
    flag_auto: str = (
        ""  # name of the auto-allocated flag (empty: the flag was given explicitly)
    )
    gate: bool = (
        False  # legend gate=yes (tool only, C3): a solid NPC that goes away by a script (an electric barrier): a GATE, not a wall, for the reachability checks
    )


@dataclass
class Warp:
    i: int
    j: int
    target_map: str
    target_spawn: str
    flags: int = 0
    req_dirs: int = 0xFF
    pad: tuple = (
        None  # C6: a teleport pad: (i2, j2, facing); target_spawn is "" and link resolves the target to that pad cell
    )
    oneway: bool = (
        False  # C9: `oneway` on the @warp line: the target has no way back by design (the League four rooms)
    )
    # filled by World.link()
    target_map_id: int = -1
    target_i: int = 0
    target_j: int = 0
    target_facing: int = 0


@dataclass
class Interact:
    i: int
    j: int
    dirmask: int
    mode: int  # 0 look, 1 step
    kind: int  # 0 text, 2 flag, 3 push
    text_key: str
    flag: int = 0
    script: str = ""  # kind 4: text_id carries this script's id


@dataclass
class HiddenItem:
    """F2 (contract 74): a hidden item = a face-mode INT_SCRIPT interaction whose `flag` is its pick-up flag (the Buscatesoros reads exactly this: kind 4, mode look, flag != 0, flag clear)."""

    i: int
    j: int
    item: str
    qty: int
    interact: object
    flag_auto: str = (
        ""  # automatic flag name (numbered by alloc_auto_flags), "" = explicit
    )
    flag_tok: str = ""


@dataclass
class Ins:
    op: str  # canonical mnemonic (OPS key)
    args: list  # raw operand tokens
    kinds: list  # operand kinds (see OPS)
    loc: str = ""


@dataclass
class Script:
    name: str
    trigger: int = 0  # SCRT_*
    cond: str = ""  # flag token ("" = always)
    cond_value: int = 1
    ins: list = field(default_factory=list)
    labels: dict = field(default_factory=dict)  # label -> instruction index
    loc: str = ""
    map: str = ""  # name of the map that holds it (the `respawn` macro's default map)


@dataclass
class Spawn:
    id: int
    name: str
    i: int
    j: int
    facing: int


@dataclass
class Map:
    name: str
    path: str = ""
    id: int = -1
    w: int = 0
    h: int = 0
    interior: bool = False
    cave: bool = False  # @cave: header flag MAPF_CAVE (the Exit Wisp works, C2-S1)
    dark: bool = (
        False  # @dark: header flag MAPF_DARK (a dark cave: only a diamond around the hero is drawn without the Linterna module, C4-A)
    )
    ghosts: bool = False  # @ghosts: header flag MAPF_GHOSTS (C4-A)
    fog: bool = False  # @fog: header flag MAPF_FOG (drifting mist, C4b, contract 40.5)
    surf: bool = (
        False  # @surf yes (tool only, C5 contract 42.1): the reachability checks treat the water as walkable
    )
    nofollow: bool = (
        False  # @nofollow yes (contract 69): the follower stays hidden on this map (header bg byte bit 2; the ferry and the sea maps)
    )
    fly: tuple = (
        None  # @fly (C6, contract 46.1): (slot, town key, i, j, facing, loc); the header's `fly` byte = 1 + slot
    )
    pads_src: list = field(
        default_factory=list
    )  # @pad lines: (i, j, target map name or "", i2, j2, facing, loc)
    pad_cells: set = field(
        default_factory=set
    )  # the cells whose legend entry says pad=yes
    extra_seeds: set = field(
        default_factory=set
    )  # reachability: cells the hero can be put on by a teleport pad (arrival of a reachable pad of this or another map)
    slides: dict = field(
        default_factory=dict
    )  # (i, j) -> facing the arrow floor pushes to (C4-A, contract 35.3)
    bike_slides: dict = field(
        default_factory=dict
    )  # (i, j) -> facing of a bike slope (C5, contract 42.6): not in `slides`
    bg: int = 0
    area: int = 0  # @area: header area_id (1 + index in AREA_KEYS, 0 none)
    fill: str = ""
    ground: list = field(default_factory=list)  # w*h bank idx or 0xFF
    cflags: list = field(default_factory=list)  # w*h baked
    objects: list = field(default_factory=list)  # sorted by front-tile sum
    npcs: list = field(default_factory=list)
    warps: list = field(default_factory=list)
    interacts: list = field(default_factory=list)
    hidden: list = field(
        default_factory=list
    )  # F2 (contract 74): HiddenItem records (legend `hidden=<item>`)
    spawns: list = field(default_factory=list)
    bank: list = field(default_factory=list)  # sprite names; index = bank index
    chars: list = field(
        default_factory=list
    )  # h rows of w source chars (debug/preview)
    doors: list = field(default_factory=list)  # (i, j) walkable door cells
    text_keys: list = field(default_factory=list)  # ordered, unique
    scripts: list = field(default_factory=list)  # Script, id = index
    flag_defs: dict = field(default_factory=dict)  # @flag NAME -> id
    var_defs: dict = field(default_factory=dict)  # @var NAME -> index
    enc_rate: int = 0  # @encounters grass rate=N (0 = none)
    open_rate: int = (
        0  # contract 63: @open_rate N: the encounter rate of open ground (byte 4 of the extras header; 0 = none)
    )
    enc: list = field(
        default_factory=list
    )  # (species id, lv_min, lv_max, weight, species name)
    tables: dict = field(
        default_factory=dict
    )  # C5 (contract 42.1): WILD_TABLES kind -> (rate, [slots like enc]): the water and the three fishing tables
    ledges: list = field(default_factory=list)  # (i, j, jump facing)
    roam: tuple = (
        None  # contract 55: `@roam <n> [table=grass] [area=<chars>] [period=<ms>]`: (n, table, area chars or "", period ms, loc)
    )
    roam_mask: bytes = (
        b""  # the roam area, 1 bit per cell (index j * w + i, LSB first): walkable ground the roamers may stand on
    )
    roam_spr: list = field(
        default_factory=list
    )  # per grass slot: the bank index of its species' `roam_<species>` actor
    balloon: bool = (
        False  # has a trainer: the "!" balloon is the last bank sprite (header flag 2)
    )

    def spawn(self, name):
        for s in self.spawns:
            if s.name == name:
                return s
        return None

    def solid_at(self, i, j):
        if not (0 <= i < self.w and 0 <= j < self.h):
            return True
        if self.cflags[j * self.w + i] & C_SOLID:
            return True
        # a solid NPC that goes away by a script is a gate, not a wall, for the reachability checks: a cuttable bush (`cut=yes`) or an NPC with the legend attribute `gate=yes` (the electric barrier of the C3 gym)
        return any(
            n.i == i
            and n.j == j
            and n.flags & 1
            and not (
                n.gate
                or n.gen in ("cut", "boulder", "sleeper", "legend", "keydoor", "roam")
            )
            for n in self.npcs
        )

    def blocked_at(self, i, j):
        """Cells the hero can never ARRIVE on by walking: solid, or a ledge (crossed only by its hop)."""
        return self.solid_at(i, j) or bool(self.cflags[j * self.w + i] & C_LEDGE)


@dataclass
class World:
    catalog: Catalog
    maps: list  # ordered by id
    start: tuple  # (map name, spawn name)
    report: Report

    def by_name(self, n):
        for m in self.maps:
            if m.name == n:
                return m
        return None


# ---- parsing --------------------------------------------------------------------------------
LEGEND_KEYS = {
    "tile",
    "object",
    "skirt",
    "door",
    "interact",
    "kind",
    "text",
    "flag",
    "dirs",
    "npc",
    "face",
    "talk_flag",
    "show_flag",
    "hide_flag",
    "behavior",
    "period",
    "solid",
    "script",
    "id",
    "ledge",
    "radius",
    "trainer",
    "range",
    "after",
    "beaten_flag",
    "item",
    "qty",
    "cut",
    "gate",
    "boulder",
    "hole",
    "sleeper",
    "legend",
    "level",
    "keydoor",
    "gift",
    "pad",
    "hidden",
}


_SPECIES = None


def species_ids():
    """{species name: id} from assets/data/species.toml (id = 1-based position in the file, like regc.py)."""
    global _SPECIES
    if _SPECIES is None:
        d = tomllib.load(open(ROOT / "assets" / "data" / "species.toml", "rb"))
        _SPECIES = {n: k + 1 for k, n in enumerate(d.keys())}
    return _SPECIES


def parse_species(tok):
    sp = species_ids()
    if tok in sp:
        return sp[tok]
    v = parse_int(tok)
    if v is None or not 1 <= v <= len(sp):
        raise ValueError(
            f"unknown species {tok!r} (an id name of assets/data/species.toml: {' '.join(sp)})"
        )
    return v


_AFF = None


def affinity_sets():
    """(solar ids, lunar ids) of the WILD pairs of assets/data/affinity.toml (contract 66; ids = positions of species.toml): the wild tables list the Solar form, a Lunar player meets the
    counterpart in the same slot (nucleo/affinity.h). A Solar form is never a visible roamer (the roamer would show one species and fight another); a Lunar form in a table is an error.
    """
    global _AFF
    if _AFF is None:
        sp = species_ids()
        p = ROOT / "assets" / "data" / "affinity.toml"
        pairs = tomllib.load(open(p, "rb")).get("pair", []) if p.is_file() else []
        _AFF = (
            {sp[x["solar"]] for x in pairs if x.get("wild", True)},
            {sp[x["lunar"]] for x in pairs if x.get("wild", True)},
        )
    return _AFF


_ITEMS = None


def parse_item(tok):
    """An id name of assets/data/items.toml (id = 1-based position, like regc.py) or its number."""
    global _ITEMS
    if _ITEMS is None:
        d = tomllib.load(open(ROOT / "assets" / "data" / "items.toml", "rb"))
        _ITEMS = {n: k + 1 for k, n in enumerate(d.keys())}
    if tok in _ITEMS:
        return _ITEMS[tok]
    v = parse_int(tok)
    if v is None or not 1 <= v <= len(_ITEMS):
        raise ValueError(
            f"unknown item {tok!r} (an id name of assets/data/items.toml: {' '.join(_ITEMS)})"
        )
    return v


_TRAINERS = None
_SHOPS = None


def _toml_ids(name):
    """{table name: 1-based position} of assets/data/<name>.toml (the id convention of species / items / trainers / shops)."""
    p = ROOT / "assets" / "data" / name
    if not p.is_file():
        return {}
    return {n: k + 1 for k, n in enumerate(tomllib.load(open(p, "rb")).keys())}


def parse_trainer(tok):
    """An id name of assets/data/trainers.toml (id = 1-based position, like regc.py) or its number."""
    global _TRAINERS
    if _TRAINERS is None:
        _TRAINERS = _toml_ids("trainers.toml")
    if tok in _TRAINERS:
        return _TRAINERS[tok]
    v = parse_int(tok)
    if v is None or not 1 <= v <= max(len(_TRAINERS), 1):
        raise ValueError(
            f"unknown trainer {tok!r} (an id name of assets/data/trainers.toml: {' '.join(_TRAINERS)})"
        )
    return v


def parse_shop(tok):
    """An id name of assets/data/shops.toml (id = 1-based position, like shopc.py) or its number (1..255)."""
    global _SHOPS
    if _SHOPS is None:
        _SHOPS = _toml_ids("shops.toml")
    if tok in _SHOPS:
        return _SHOPS[tok]
    v = parse_int(tok)
    if v is None or not 1 <= v <= 255:
        raise ValueError(
            f"unknown shop {tok!r} (an id name of assets/data/shops.toml: {' '.join(_SHOPS)})"
        )
    return v


_SFX = None


def parse_sfx(tok, what="fx"):
    """A sound effect (`fx NAME` lines of assets/music/index.txt; number 0 = the first effect, SND_FX0 + number is the sound id) or, with what="music", a music track
    (`music NAME` lines: the position is the track's SND_* id, 0 = title) by name or by number.
    """
    global _SFX
    if _SFX is None:
        _SFX = {"fx": [], "music": []}
        for ln in (
            (ROOT / "assets" / "music" / "index.txt")
            .read_text(encoding="utf-8")
            .splitlines()
        ):
            t = ln.split("#")[0].split()
            if len(t) == 2 and t[0] in _SFX:
                _SFX[t[0]].append(t[1])
    names = _SFX[what]
    if tok in names:
        return names.index(tok)
    v = parse_int(tok)
    if v is None or not 0 <= v < len(names):
        raise ValueError(
            f"unknown {'sound effect' if what == 'fx' else 'music track'} {tok!r} (a `{what}` name of assets/music/index.txt: {' '.join(names)})"
        )
    return v


def parse_facing(s):
    s = s.upper()
    return FACINGS.index(s) if s in FACINGS else None


def parse_dirs(s):
    m = 0
    for p in s.split(","):
        f = parse_facing(p.strip())
        if f is None:
            return None
        m |= 1 << f
    return m


def parse_int(s):
    try:
        return int(s, 0)
    except ValueError:
        return None


def strip_comment(line):
    k = line.find("#")
    return (line[:k] if k >= 0 else line).strip()


def parse_map(path, cat, rep, name_hint=None):
    where = Path(path).name
    m = Map(name=name_hint or Path(path).stem, path=str(path))
    legend, warps_src, spawn_src = {}, [], []
    grid = []
    section = None
    cur = None  # Script being read
    enc_kind = None  # the @encounters table being read (grass or a WILD_TABLES kind)
    seen_w = False
    for ln, raw in enumerate(open(path, encoding="utf-8"), 1):
        line = strip_comment(raw)
        if not line:
            continue
        loc = f"{where}:{ln}"
        tok = line.split()
        if tok[0].startswith("@"):
            d = tok[0]
            prev_section, section = section, None
            if d == "@map":
                m.name = tok[1] if len(tok) > 1 else m.name
            elif d == "@size":
                if (
                    len(tok) != 3
                    or parse_int(tok[1]) is None
                    or parse_int(tok[2]) is None
                ):
                    rep.err(loc, "@size <w> <h>")
                else:
                    m.w, m.h = int(tok[1]), int(tok[2])
                    seen_w = True
                    if not (1 <= m.w <= 64 and 1 <= m.h <= 64):
                        rep.err(loc, "size must be 1..64")
            elif d == "@interior":
                m.interior = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@cave":
                m.cave = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@dark":
                m.dark = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@ghosts":
                m.ghosts = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@fog":
                m.fog = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@surf":
                m.surf = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@nofollow":
                m.nofollow = len(tok) > 1 and tok[1].lower() in ("yes", "true", "1")
            elif d == "@bg":
                v = parse_int(tok[1]) if len(tok) > 1 else None
                if v is None or not 0 <= v <= 3:
                    rep.err(loc, "@bg must be 0..3")
                else:
                    m.bg = v
            elif d == "@area":
                if len(tok) != 2 or tok[1] not in AREA_KEYS:
                    rep.err(loc, "@area <" + "|".join(AREA_KEYS) + ">")
                else:
                    m.area = AREA_KEYS.index(tok[1]) + 1
            elif d == "@fill":
                m.fill = tok[1] if len(tok) > 1 else ""
            elif d == "@spawn":
                if len(tok) != 5:
                    rep.err(loc, "@spawn <name> <i> <j> <facing>")
                else:
                    f = parse_facing(tok[4])
                    i, j = parse_int(tok[2]), parse_int(tok[3])
                    if f is None or i is None or j is None:
                        rep.err(
                            loc,
                            f"bad spawn {line!r} (facing must be one of {' '.join(FACINGS)})",
                        )
                    elif any(s[0] == tok[1] for s in spawn_src):
                        rep.err(loc, f"duplicate spawn {tok[1]}")
                    else:
                        spawn_src.append((tok[1], i, j, f, loc))
            elif d == "@flag" or d == "@var":
                top = MAX_FLAG if d == "@flag" else 15
                v = parse_int(tok[2]) if len(tok) == 3 else None
                defs = m.flag_defs if d == "@flag" else m.var_defs
                if (
                    v is None
                    or not (1 if d == "@flag" else 0) <= v <= top
                    or not re.match(r"^\w+$", tok[1])
                ):
                    rep.err(
                        loc,
                        f"{d} <NAME> <{f'1..{MAX_FLAG}' if d == '@flag' else '0..15'}>",
                    )
                elif tok[1] in defs and defs[tok[1]] != v:
                    rep.err(loc, f"{d} {tok[1]} defined twice with different numbers")
                else:
                    defs[tok[1]] = v
            elif d == "@script":
                cur = parse_script_header(tok, loc, rep, m)
                section = "script" if cur is not None else "script_skip"
            elif d == "@encounters":
                opts = dict(t.split("=", 1) for t in tok[2:] if "=" in t)
                kind = tok[1] if len(tok) > 1 else ""
                rate = (
                    parse_int(opts["rate"]) if "rate" in opts else FISH_RATE.get(kind)
                )  # (the fishing tables have a default bite chance)
                enc_kind = kind if kind == "grass" or kind in WILD_TABLES else None
                if (
                    kind not in ("grass",) + WILD_TABLES
                    or rate is None
                    or not 1 <= rate <= 255
                ):
                    rep.err(
                        loc,
                        "@encounters grass|water rate=<1..255> | fish_old|fish_good|fish_super [rate=<1..255>]  (rate = the chance of a bite, 255 = always)",
                    )
                    enc_kind = None
                elif (
                    kind == "grass"
                    and m.enc_rate
                    or kind != "grass"
                    and kind in m.tables
                ):
                    rep.err(
                        loc,
                        f"second @encounters {kind} table (only one per kind per map)",
                    )
                    enc_kind = None
                elif kind == "grass":
                    m.enc_rate = rate
                else:
                    m.tables[kind] = (rate, [])
                section = "enc" if enc_kind else "enc_skip"
            elif d == "@open_rate":
                v = parse_int(tok[1]) if len(tok) == 2 else None
                if v is None or not 0 <= v <= 255:
                    rep.err(
                        loc,
                        "@open_rate <0..255>  (contract 63: encounter when rand8 < n on a step onto open ground; 0 = none)",
                    )
                elif m.open_rate:
                    rep.err(loc, "a map has at most one @open_rate")
                else:
                    m.open_rate = v
            elif d == "@roam":
                # @roam <n> [table=grass] [area=<chars>] [period=<ms>]   (contract 55: roaming wild creatures, the pilot)
                opts = {
                    x.split("=", 1)[0]: x.split("=", 1)[1] for x in tok[2:] if "=" in x
                }
                nn = parse_int(tok[1]) if len(tok) > 1 else None
                per = parse_int(opts["period"]) if "period" in opts else ROAM_PERIOD
                if (
                    nn is None
                    or not 1 <= nn <= ROAM_MAX
                    or per is None
                    or not 250 <= per <= 255 * 250
                    or set(opts) - {"table", "area", "period"}
                    or len(tok) - 2 != len(opts)
                ):
                    rep.err(
                        loc,
                        f"@roam <1..{ROAM_MAX}> [table=grass] [area=<legend chars>] [period=<250..63750 ms>]",
                    )
                elif opts.get("table", "grass") != "grass":
                    rep.err(
                        loc,
                        "@roam table= is only `grass` for now (the map's tall-grass table)",
                    )
                elif m.roam:
                    rep.err(loc, "a map has at most one @roam")
                else:
                    m.roam = (nn, "grass", opts.get("area", ""), per, loc)
            elif d == "@end":
                if prev_section not in ("script", "script_skip", "enc", "enc_skip"):
                    rep.err(loc, "@end without @script / @encounters")
                section = None
                cur = None
            elif d == "@legend":
                section = "legend"
            elif d == "@grid":
                section = "grid"
            elif d == "@warp":
                # @warp <char> -> <map> spawn=<name> [dirs=..] [nofade] [oneway] (oneway, C9 contract 50.2: the target has no way back on purpose, the League four rooms: no "has no warp back" warning)
                if len(tok) < 5 or tok[2] != "->" or len(tok[1]) != 1:
                    rep.err(loc, "@warp <char> -> <map> spawn=<name>")
                    continue
                opts = dict(t.split("=", 1) for t in tok[4:] if "=" in t)
                if "spawn" not in opts:
                    rep.err(loc, "@warp needs spawn=<name>")
                    continue
                req = 0xFF
                if "dirs" in opts:
                    req = parse_dirs(opts["dirs"])
                    if req is None:
                        rep.err(loc, f"bad dirs={opts['dirs']}")
                        req = 0xFF
                warps_src.append(
                    (
                        tok[1],
                        tok[3],
                        opts["spawn"],
                        1 if "nofade" in tok[4:] else 0,
                        req,
                        loc,
                        "oneway" in tok[4:],
                    )
                )
            elif d == "@pad":
                # @pad <i> <j> -> [<map>] <i2> <j2> [face=DIR]
                t = tok[1:]
                opts = {x.split("=", 1)[0]: x.split("=", 1)[1] for x in t if "=" in x}
                t = [x for x in t if "=" not in x]
                nums = (
                    [parse_int(t[0]), parse_int(t[1])] if len(t) >= 2 else [None, None]
                )
                if len(t) in (5, 6) and t[2] == "->":
                    tmap = t[3] if len(t) == 6 else ""
                    n2 = [parse_int(t[-2]), parse_int(t[-1])]
                    f = parse_facing(opts.get("face", "S"))
                    if (
                        None in nums
                        or None in n2
                        or f is None
                        or (len(t) == 6 and parse_int(t[3]) is not None)
                    ):
                        rep.err(loc, "@pad <i> <j> -> [<map>] <i2> <j2> [face=DIR]")
                    else:
                        m.pads_src.append(
                            (nums[0], nums[1], tmap, n2[0], n2[1], f, loc)
                        )
                else:
                    rep.err(loc, "@pad <i> <j> -> [<map>] <i2> <j2> [face=DIR]")
            elif d == "@fly":
                # @fly <slot> <town_key> <i> <j> [face=DIR]
                opts = {
                    x.split("=", 1)[0]: x.split("=", 1)[1] for x in tok[1:] if "=" in x
                }
                t = [x for x in tok[1:] if "=" not in x]
                vals = (
                    [parse_int(t[0]), parse_int(t[2]), parse_int(t[3])]
                    if len(t) == 4
                    else None
                )
                f = parse_facing(opts.get("face", "S"))
                if (
                    vals is None
                    or None in vals
                    or f is None
                    or not 0 <= vals[0] < FLY_SLOTS
                    or not re.match(r"^\w+$", t[1])
                ):
                    rep.err(
                        loc,
                        f"@fly <slot 0..{FLY_SLOTS - 1}> <town_key> <i> <j> [face=DIR]",
                    )
                elif FLY_FLAG_BASE + vals[0] in BUILTIN_FLAGS.values():
                    rep.err(
                        loc,
                        f"@fly slot {vals[0]}: its visited flag {FLY_FLAG_BASE + vals[0]} is a builtin story flag (contract 72)",
                    )
                elif m.fly:
                    rep.err(loc, "a map has at most one @fly")
                else:
                    m.fly = (vals[0], t[1], vals[1], vals[2], f, loc)
            else:
                rep.err(loc, f"unknown directive {d}")
            continue
        if section == "enc_skip":
            continue
        if section == "enc":
            if len(tok) != 4:
                rep.err(loc, "encounter slot: <species> <lv_min> <lv_max> <weight>")
                continue
            try:
                sid = parse_species(tok[0])
            except ValueError as e:
                rep.err(loc, str(e))
                continue
            lo, hi, wt = (parse_int(t) for t in tok[1:])
            if None in (lo, hi, wt) or not (1 <= lo <= hi <= 100) or not 1 <= wt <= 255:
                rep.err(
                    loc,
                    "encounter slot needs 1 <= lv_min <= lv_max <= 100 and a weight of 1..255",
                )
                continue
            if sid in affinity_sets()[1]:
                rep.err(
                    loc,
                    f"{tok[0]} is the LUNAR form of an affinity pair (assets/data/affinity.toml, contract 66): the wild tables list the Solar counterpart (a Lunar player meets {tok[0]} there)",
                )
                continue
            slots = m.enc if enc_kind == "grass" else m.tables[enc_kind][1]
            if len(slots) >= MAX_ENC:
                rep.err(loc, f"more than {MAX_ENC} encounter slots")
                continue
            slots.append((sid, lo, hi, wt, tok[0]))
            continue
        if section == "script_skip":
            continue
        if section == "script":
            parse_script_line(cur, line, tok, loc, rep)
            continue
        if section == "legend":
            ch = tok[0]
            if len(ch) != 1:
                rep.err(loc, f"legend char must be a single character, got {ch!r}")
                continue
            if ch in legend:
                rep.err(loc, f"duplicate legend char {ch!r}")
                continue
            attrs = {}
            for t in tok[1:]:
                if "=" not in t:
                    rep.err(loc, f"legend attribute {t!r} is not key=value")
                    continue
                k, v = t.split("=", 1)
                if k not in LEGEND_KEYS:
                    rep.err(loc, f"unknown legend attribute {k!r}")
                    continue
                attrs[k] = v
            attrs["_loc"] = loc
            legend[ch] = attrs
        elif section == "grid":
            if len(line.split()) != 1:
                rep.err(loc, "grid rows must not contain spaces")
                continue
            grid.append((line, loc))
        else:
            rep.err(loc, f"unexpected line outside @legend/@grid: {line!r}")
    if not seen_w:
        rep.err(where, "missing @size")
        return None
    if len(grid) != m.h:
        rep.err(where, f"grid has {len(grid)} rows, @size says {m.h}")
        return None
    ok = True
    for j, (row, loc) in enumerate(grid):
        if len(row) != m.w:
            rep.err(loc, f"grid row has {len(row)} chars, @size says {m.w}")
            ok = False
            continue
        for i, ch in enumerate(row):
            if ch not in legend:
                rep.err(loc, f"unknown character {ch!r} at (i={i}, j={j})")
                ok = False
    if not ok:
        return None
    if section in ("script", "script_skip", "enc", "enc_skip"):
        rep.err(where, "@script / @encounters without @end")
    m.chars = [row for row, _ in grid]
    build_map(m, legend, warps_src, spawn_src, cat, rep)
    finish_scripts(m, rep)
    return m


def trig_name(t):
    return next((k for k, v in SCRT_NAMES.items() if v == t), str(t))


# ---- generated NPC scripts: trainers who see the hero and items on the ground (contract 18) ------------------------------
def flag_value(m, tok):
    """A flag given in a legend attribute: a number 1..MAX_FLAG, an @flag name declared earlier in this map's file, or a builtin name (badge_*, mod_*, on_bike, on_surf); None when it is none of them."""
    v = parse_int(tok)
    if v is None:
        v = m.flag_defs.get(tok, BUILTIN_FLAGS.get(tok))
    return v if v is not None and 0 <= v <= MAX_FLAG else None


def _gen_flag(m, n, a, key, loc, rep):
    """The flag of a generated NPC: the legend's explicit one (`beaten_flag=` / `hide_flag=`: a number 1..1023, or for a trainer
    also an @flag name of this map) -> n.flag_tok, else an automatic name (n.flag_auto) that alloc_auto_flags numbers later.
    """
    tok = a.get(key)
    if tok is None:
        n.flag_auto = (
            f"{'beat' if key == 'beaten_flag' else 'item'}_{m.name}_{n.i}_{n.j}"
        )
        n.flag_tok = n.flag_auto
        return True
    v = flag_value(m, tok)
    if v is None or not 1 <= v <= MAX_FLAG:
        rep.err(
            loc,
            f"{key}= must be a flag number 1..{MAX_FLAG} or an @flag name of this map",
        )
        return False
    n.flag_tok = tok
    return True


def gen_npc_script(m, n, a, kind, k, loc, cat, rep):
    """Adds the script of a `trainer=` / `item=` NPC (the k-th NPC of m) to m.scripts and returns its name (None = error)."""
    name = f"{kind}_{k}"
    if len(m.scripts) >= 255 or any(x.name == name for x in m.scripts):
        rep.err(loc, f"cannot add script {name}")
        return None
    pre = []  # assembler lines of the script
    if kind == "trainer":
        try:
            tid = parse_trainer(a["trainer"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        rng = parse_int(a.get("range", str(TRAINER_RANGE)))
        if rng is None or not 1 <= rng <= 15:
            rep.err(loc, "range= must be 1..15 (tiles)")
            return None
        f = parse_facing(a.get("face", "S"))
        spr = cat.sprites[n.name]
        if f is not None and spr["ndirs"] == 4 and f & 1:
            rep.err(
                loc,
                f"actor {n.name} has 4 directions: a trainer's face= must be S, E, N or W (its sight follows the facing), got {a.get('face')}",
            )
            return None
        if spr["nframes"] < 2 and not m.name.startswith(
            "test_"
        ):  # (the test fixtures use the 8-direction vecino for the odd facings)
            rep.warn(
                loc,
                f"trainer sprite {n.name} has no walk frames: it will slide up to the hero",
            )
        if not _gen_flag(m, n, a, "beaten_flag", loc, rep):
            return None
        pre_key, after_key = (
            a.get("text") or "gl.trainer.pre",
            a.get("after") or "gl.trainer.after",
        )
        n.gen = "trainer"
        m.balloon = True
        fl, me = n.flag_tok, f"#{k}"
        # the camera cuts to the trainer while it shows the balloon and walks (a trainer 3 - 4 tiles up / down the screen is off the 64 px
        # view), and back to the hero once it stands next to him
        lines = [
            f"jf {fl} after",
            f"trainer_spot {me}",
            "jv 0 1 near",
            f"cam_npc {me}",
            "wait 600",
            f"trainer_come {me}",
            "wait_idle",
            "near:",
            "cam_hero",
            f"npc_face_hero {me}",
            f"hero_face_npc {me}",
            f"say {pre_key}",
            f"battle {tid} 0",
            "jv 0 1 won",
            "blackout",
            "end",
            "won:",
            f"setf {fl}",
            f"say {after_key}",
            "end",
            "after:",
            f"npc_face_hero {me}",
            f"say {after_key}",
        ]
    elif kind == "sleeper":
        # A sleeping creature on the road (C5, contract 42.4): a solid NPC with a fixed wild battle. Without the key item `flauta` it is a text; with it the player is asked
        # to play it, a jingle plays, the creature wakes and the wild battle (SPECIES at LEVEL) starts. After the battle (won, caught or ran; a defeat is the blackout, the script is cut)
        # the NPC's hide flag is set: it is gone for good.
        try:
            sp = parse_species(a["sleeper"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        lv = parse_int(a.get("level", ""))
        if lv is None or not 1 <= lv <= 100:
            rep.err(loc, "sleeper= needs level=1..100")
            return None
        if not _gen_flag(m, n, a, "hide_flag", loc, rep):
            return None
        n.gen = "sleeper"
        fl = n.flag_tok
        lines = [
            "has_item flauta 0",
            "jv 0 1 flute",
            "say gl.c5a.sleep.look",
            "end",
            "flute:",
            "ask gl.c5a.sleep.ask",
            "jv 0 1 play",
            "end",
            "play:",
            f"sfx {SLEEPER_FX}",
            "say gl.c5a.sleep.woke",
            f"wild {sp} {lv} 0",
            f"setf {fl}",
        ]
    elif kind == "legend":
        # A legendary creature standing on the map (C7 / C9, contract 52): a solid visible NPC that turns to the hero, cries (the effect LEGEND_FX), says one line
        # (gl.c52.cry) and starts a fixed wild battle SPECIES at LEVEL FLAGGED LEGENDARY (the wild level with bit 7: battle_open.c plays the `legendario` track, the battle plugin plays the intro). Won (fainted)
        # or caught: the NPC's hide flag is set, it is gone for good. RAN: the flag stays clear, it is still there. A defeat is the blackout (the script is cut, it stays).
        try:
            sp = parse_species(a["legend"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        lv = parse_int(a.get("level", ""))
        if lv is None or not 1 <= lv <= 100:
            rep.err(loc, "legend= needs level=1..100")
            return None
        if not _gen_flag(m, n, a, "hide_flag", loc, rep):
            return None
        n.gen = "legend"
        lines = [
            f"npc_face_hero #{k}",
            f"sfx {LEGEND_FX}",
            "say gl.c52.cry",
            f"wild_set {sp} {lv + 128}",
            "wild_go 0",
            "jv 0 2 stay",
            "jv 0 4 stay",
            f"setf {n.flag_tok}",
            "stay:",
        ]
    elif kind == "keydoor":
        # A card-key door (C6, contract 46.2): a solid NPC. Without the key item the text says it is locked; with it a sound plays, the NPC's hide flag is set (the door
        # is gone for good) and a text says it opens. The key is not consumed.
        try:
            it = parse_item(a["keydoor"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        if not _gen_flag(m, n, a, "hide_flag", loc, rep):
            return None
        n.gen = "keydoor"
        lines = [
            f"has_item {it} 0",
            "jv 0 1 open",
            f"item_var 0 {it}",
            "say gl.c6.door.locked",
            "end",
            "open:",
            f"item_var 0 {it}",
            f"sfx {KEYDOOR_FX}",
            f"setf {n.flag_tok}",
            "say gl.c6.door.open",
        ]
    elif kind == "gift":
        # A gift to choose among N (C6, contract 46.4): every gift NPC of the group has the SAME hide_flag; taking one asks "Do you want X?" (the species picture shows), gives it
        # (a full party sends it to the PC box: give_box) and sets the flag, which hides the whole group. NO changes nothing. The species name rides in text variable {v0}.
        try:
            sp = parse_species(a["gift"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        lv = parse_int(a.get("level", ""))
        if lv is None or not 1 <= lv <= 100:
            rep.err(loc, "gift= needs level=1..100")
            return None
        if not _gen_flag(m, n, a, "hide_flag", loc, rep):
            return None
        n.gen = "gift"
        lines = [
            f"setv 0 {sp}",
            "name_var 0 0",
            "mon_show 0",
            "ask gl.c6.gift.ask",
            "jv 0 1 yes",
            "end",
            "yes:",
            f"give {sp} {lv + 128}",
            "jv 0 0 fail",
            f"setf {n.flag_tok}",
            f"sfx {GIFT_FX}",
            "say gl.c6.gift.got",
            "end",
            "fail:",
            "say gl.c6.gift.fail",
        ]
    elif kind == "cut":
        # A cuttable bush (C3-A, contract 32): a solid NPC. Without the Cortador module (flag 88) or the Oleaje badge (flag 81) it only says the first
        # text; with both it asks, and YES plays the effect (a blink, the "hit" sound) and hides the NPC with npc_hide: that override lasts until the next map load,
        # so the bush grows back when the map is entered again.
        n.gen = "cut"
        me = f"#{k}"
        lines = [
            "jnf mod_cortador no",
            "jnf badge_marea no",
            "ask gl.c3a.cut.ask",
            "jv 0 1 yes",
            "end",
            "yes:",
            "sfx hit",
            f"npc_hide {me}",
            "wait 80",
            f"npc_show {me}",
            "wait 80",
            f"npc_hide {me}",
            "wait 80",
            f"npc_show {me}",
            "wait 80",
            f"npc_hide {me}",
            "say gl.c3a.cut.done",
            "end",
            "no:",
            "say gl.c3a.cut.no",
        ]
    else:
        try:
            it = parse_item(a["item"])
        except ValueError as e:
            rep.err(loc, str(e))
            return None
        qty = parse_int(a.get("qty", "1"))
        if qty is None or not 1 <= qty <= 99:
            rep.err(loc, "qty= must be 1..99")
            return None
        if not _gen_flag(m, n, a, "hide_flag", loc, rep):
            return None
        n.gen = "item"
        fl = n.flag_tok
        lines = [
            f"give_item {it} {qty}",
            "jv 0 0 full",
            f"item_var 0 {it}",
            "say gl.found",
            f"setf {fl}",
            "end",
            "full:",
            "say gl.found.full",
        ]
    sc = Script(name=name, trigger=SCRT_NAMES["npc"], loc=loc, map=m.name)
    m.scripts.append(sc)
    for ln in lines:
        parse_script_line(sc, ln, ln.split(), loc, rep)
    return name


def gen_hidden(m, a, i, j, loc, cflag, rep):
    """F2 (contract 74): legend `hidden=<item> [qty=N] [hide_flag=<flag>]` = an item that is not drawn: OK while facing its (walkable) tile gives it once. It is a face-mode INT_SCRIPT
    interaction (`Interact.flag` = its pick-up flag, so the engine and the Buscatesoros can tell it from the other scripts) + the script hid_<i>_<j> with the condition `if !FLAG`.
    """
    try:
        it = parse_item(a["hidden"])
    except ValueError as e:
        rep.err(loc, str(e))
        return
    qty = parse_int(a.get("qty", "1"))
    if qty is None or not 1 <= qty <= 99:
        rep.err(loc, "qty= must be 1..99")
        return
    if (
        any(
            k in a
            for k in (
                "object",
                "door",
                "npc",
                "interact",
                "item",
                "ledge",
                "pad",
                "script",
                "text",
                "trainer",
                "cut",
                "boulder",
                "hole",
                "sleeper",
                "legend",
                "keydoor",
                "gift",
            )
        )
        or a.get("solid") == "yes"
    ):
        rep.err(
            loc,
            "hidden= is a plain floor tile with an item: no object= door= npc= interact= solid= ... on it",
        )
        return
    if cflag & (C_SOLID | C_WARP | C_TRIGGER | C_WATER):
        rep.err(
            loc,
            "hidden= needs a walkable ground tile (not solid, water, a door or a trigger)",
        )
        return
    name = f"hid_{i}_{j}"
    if len(m.scripts) >= 255 or any(x.name == name for x in m.scripts):
        rep.err(loc, f"cannot add script {name}")
        return
    h = HiddenItem(i, j, it, qty, None)
    if "hide_flag" in a:
        v = flag_value(m, a["hide_flag"])
        if v is None or not 1 <= v <= MAX_FLAG:
            rep.err(
                loc,
                f"hide_flag= must be a flag number 1..{MAX_FLAG} or an @flag name of this map",
            )
            return
        h.flag_tok = a["hide_flag"]
    else:
        h.flag_auto = h.flag_tok = f"item_{m.name}_{i}_{j}"
    sc = Script(
        name=name,
        trigger=SCRT_NAMES["interact"],
        cond=h.flag_tok,
        cond_value=0,
        loc=loc,
        map=m.name,
    )
    m.scripts.append(sc)
    for ln in (
        f"give_item {it} {qty}",
        "jv 0 0 full",
        f"item_var 0 {it}",
        f"sfx {GIFT_FX}",
        "say gl.found",
        f"setf {h.flag_tok}",
        "end",
        "full:",
        "say gl.found.full",
    ):
        parse_script_line(sc, ln, ln.split(), loc, rep)
    v = flag_value(m, h.flag_tok) if not h.flag_auto else 0
    h.interact = Interact(i, j, 0xFF, 0, IK_SCRIPT, "", v or 0, name)
    m.interacts.append(h.interact)
    m.hidden.append(h)


def build_map(m, legend, warps_src, spawn_src, cat, rep):
    W, H = m.w, m.h
    where = m.name
    ch_at = lambda i, j: m.chars[j][i]
    texts = []

    def addtext(k):
        if k and k not in texts:
            texts.append(k)

    # ---- ground (names; bank indices assigned at the end)
    ground_names = [None] * (W * H)
    cf = [0] * (W * H)
    if m.fill and cat.resolve(m.fill) is None:
        rep.err(where, f"@fill: unknown tile {m.fill!r}")
    for j in range(H):
        for i in range(W):
            a = legend[ch_at(i, j)]
            t = a.get("tile", m.fill)
            ldir = None
            if "ledge" in a:
                ldir = LEDGE_DIRS.get(a["ledge"].upper())
                if ldir is None:
                    rep.err(
                        f"{where}({i},{j})",
                        f"ledge= must be one of {' '.join(LEDGE_DIRS)} (a world axis), got {a['ledge']!r}",
                    )
                    continue
                if any(k in a for k in ("object", "door", "npc", "interact")):
                    rep.err(
                        f"{where}({i},{j})",
                        "a ledge legend entry cannot also have object= / door= / npc= / interact=",
                    )
                t = a.get("tile", "ledge_" + a["ledge"].lower())
            if not t:
                rep.err(
                    f"{where}({i},{j})",
                    f"char {ch_at(i, j)!r} has no tile= and the map has no @fill",
                )
                continue
            if t == "none":
                continue
            r = cat.resolve(t, i, j)
            if r is None or r[0] != "one":
                rep.err(f"{where}({i},{j})", f"unknown tile {t!r}")
                continue
            s = cat.sprites[r[1]]
            if s["kind"] != "tile":
                rep.err(f"{where}({i},{j})", f"{t!r} is not a tile")
                continue
            ground_names[j * W + i] = r[1]
            cf[j * W + i] = s["_cf"]
            if (
                "pad" in a
            ):  # C6 (contract 46.3): a teleport pad cell; the @pad line gives its target
                if a["pad"] not in ("yes", "1"):
                    rep.err(f"{where}({i},{j})", "pad= must be yes")
                elif a.get("solid") == "yes" or any(
                    k in a
                    for k in (
                        "object",
                        "door",
                        "interact",
                        "npc",
                        "item",
                        "cut",
                        "boulder",
                        "hole",
                        "sleeper",
                        "legend",
                        "trainer",
                        "keydoor",
                        "gift",
                        "ledge",
                    )
                ):
                    rep.err(
                        f"{where}({i},{j})",
                        "a pad legend entry is a plain floor tile: no solid= object= door= interact= npc= ... on it",
                    )
                else:
                    m.pad_cells.add((i, j))
            if a.get("solid") == "yes" and not any(
                k in a
                for k in (
                    "npc",
                    "item",
                    "cut",
                    "boulder",
                    "hole",
                    "sleeper",
                    "legend",
                    "trainer",
                    "keydoor",
                    "gift",
                )
            ):
                cf[
                    j * W + i
                ] |= C_SOLID  # C5 (contract 42.5): an invisible wall: a plain floor tile that blocks (`solid=yes` on a legend entry without an NPC)
            if s.get("_slide") is not None:
                if s.get("_bike"):
                    m.bike_slides[(i, j)] = s[
                        "_slide"
                    ]  # a bike slope pushes only a rider: the reachability checks see plain floor
                else:
                    m.slides[(i, j)] = s["_slide"]
            if ldir is not None:
                cf[j * W + i] |= C_LEDGE
                m.ledges.append((i, j, ldir))

    # ---- objects: claim footprint rectangles
    claimed = {}
    obj_list = []  # (name, i0, j0, fw, fh)
    door_cells = set()
    interact_cells = {}

    def obj_attr(a):
        return a.get("object") or a.get("door")

    for a in legend.values():
        if "object" in a and "door" in a:
            rep.err(a["_loc"], "legend entry has both object= and door=")
        if "npc" in a and "door" in a:
            rep.err(a["_loc"], "legend entry has both npc= and door=")
        if "skirt" in a and "object" not in a:
            rep.err(a["_loc"], "skirt= needs object=")

    for j in range(H):
        for i in range(W):
            a = legend[ch_at(i, j)]
            logical = obj_attr(a)
            if not logical:
                continue
            if (i, j) in claimed:
                continue
            dm = cat.dims(logical, i, j)
            if dm is None:
                rep.err(f"{where}({i},{j})", f"unknown object {logical!r}")
                claimed[(i, j)] = None
                continue
            fw, fh = dm
            bad = False
            for dj in range(fh):
                for di in range(fw):
                    ci, cj = i + di, j + dj
                    if not (ci < W and cj < H):
                        bad = True
                        continue
                    aa = legend[ch_at(ci, cj)]
                    if obj_attr(aa) != logical or (ci, cj) in claimed:
                        bad = True
            if bad:
                rep.err(
                    f"{where}({i},{j})",
                    f"object {logical!r} needs a {fw}x{fh} (i x j) rectangle of "
                    f"its chars starting at the min corner here; the cells do not match",
                )
                claimed[(i, j)] = None
                continue
            for dj in range(fh):
                for di in range(fw):
                    claimed[(i + di, j + dj)] = logical
            r = cat.resolve(logical, i, j)
            # doors inside this footprint
            res_name = r[1] if r and r[0] == "one" else None
            dts = (
                [tuple(d) for d in cat.sprites[res_name]["door_tiles"]]
                if res_name
                else []
            )
            for dj in range(fh):
                for di in range(fw):
                    aa = legend[ch_at(i + di, j + dj)]
                    if "door" in aa:
                        if (di, dj) not in dts:
                            rep.err(
                                f"{where}({i + di},{j + dj})",
                                f"door tile of {logical!r} must be one of door_tiles {dts} "
                                f"(relative cell {(di, dj)})",
                            )
                        else:
                            door_cells.add((i + di, j + dj))
            # skirts first (drawn behind), then the object
            for dj in range(fh):
                for di in range(fw):
                    aa = legend[ch_at(i + di, j + dj)]
                    if "skirt" in aa:
                        sr = cat.resolve(aa["skirt"], i + di, j + dj)
                        if (
                            sr is None
                            or sr[0] != "one"
                            or cat.sprites[sr[1]]["kind"] != "object"
                            or (cat.sprites[sr[1]]["fw"], cat.sprites[sr[1]]["fh"])
                            != (1, 1)
                        ):
                            rep.err(
                                f"{where}({i + di},{j + dj})",
                                f"skirt {aa['skirt']!r} must be a 1x1 object",
                            )
                        else:
                            obj_list.append((sr[1], i + di, j + dj, 1, 1))
            if r is None:
                rep.err(f"{where}({i},{j})", f"unknown object {logical!r}")
            elif r[0] == "one":
                s = cat.sprites[r[1]]
                if s["kind"] != "object":
                    rep.err(
                        f"{where}({i},{j})", f"{logical!r} -> {r[1]} is not an object"
                    )
                else:
                    obj_list.append((r[1], i, j, s["fw"], s["fh"]))
                    for dj in range(fh):
                        for di in range(fw):
                            cf[(j + dj) * W + i + di] |= s["_cf"]
                            if s["_cf"] & C_SOLID:
                                cf[
                                    (j + dj) * W + i + di
                                ] &= (
                                    ~C_WATER
                                )  # C5 (contract 42.1): water under a solid object (a rock in the sea) is not surfable
            else:  # tiled list
                names = r[1]
                k = 0
                for dj in range(fh):
                    for di in range(fw):
                        s = cat.sprites.get(names[k])
                        k += 1
                        if s is None:
                            continue
                        obj_list.append((s["name"], i + di, j + dj, 1, 1))
                        cf[(j + dj) * W + i + di] |= s["_cf"]
                        if s["_cf"] & C_SOLID:
                            cf[(j + dj) * W + i + di] &= ~C_WATER
            # unresolved cells keep cflags of the sprite for tiled lists above

    # ---- doors: need a warp; clear solid, set warp
    warp_chars = {w[0] for w in warps_src}
    for i, j in sorted(door_cells, key=lambda c: (c[1], c[0])):
        ch = ch_at(i, j)
        if ch not in warp_chars:
            rep.err(
                f"{where}({i},{j})",
                f"door char {ch!r} has no @warp (use object= + interact= for a "
                f"non-warp door)",
            )
        cf[j * W + i] &= ~C_SOLID
        cf[j * W + i] |= C_WARP
        m.doors.append((i, j))

    # ---- warps
    for ch, tmap, tspawn, flags, req, loc, oneway in warps_src:
        if ch not in legend:
            rep.err(loc, f"@warp char {ch!r} is not in the legend")
            continue
        n = 0
        for j in range(H):
            for i in range(W):
                if ch_at(i, j) != ch:
                    continue
                n += 1
                if cf[j * W + i] & C_SOLID:
                    rep.err(f"{where}({i},{j})", "warp on a solid tile")
                cf[j * W + i] |= C_WARP
                m.warps.append(Warp(i, j, tmap, tspawn, flags, req, oneway=oneway))
        if n == 0:
            rep.err(loc, f"@warp char {ch!r} does not appear in the grid")

    # ---- C6 (contract 46.3): teleport pads. Every pad=yes cell has exactly one @pad line; the line makes it a warp whose target is another pad cell (resolved by link_and_validate)
    seen_pads = set()
    for pi, pj, tmap, ti, tj, pf, loc in m.pads_src:
        if (pi, pj) not in m.pad_cells:
            rep.err(
                loc, f"@pad ({pi},{pj}): that cell is not a legend entry with pad=yes"
            )
            continue
        if (pi, pj) in seen_pads:
            rep.err(loc, f"@pad ({pi},{pj}) is declared twice")
            continue
        seen_pads.add((pi, pj))
        if cf[pj * W + pi] & (C_SOLID | C_WARP):
            rep.err(loc, f"@pad ({pi},{pj}): the cell is solid or already a warp")
            continue
        cf[pj * W + pi] |= C_WARP
        w_ = Warp(pi, pj, tmap or m.name, "", 0, 0xFF)
        w_.pad = (ti, tj, pf)
        m.warps.append(w_)
    for pi, pj in sorted(m.pad_cells - seen_pads):
        rep.err(f"{where}({pi},{pj})", "a pad=yes cell has no @pad line")

    # ---- interactions
    for j in range(H):
        for i in range(W):
            a = legend[ch_at(i, j)]
            if "hidden" in a:
                gen_hidden(m, a, i, j, f"{where}({i},{j})", cf[j * W + i], rep)
                continue
            if "interact" not in a:
                continue
            loc = f"{where}({i},{j})"
            mode = {"look": 0, "step": 1}.get(a["interact"])
            if mode is None:
                rep.err(loc, f"interact= must be look or step, got {a['interact']!r}")
                continue
            kind = {"text": IK_TEXT, "flag": IK_FLAG, "push": IK_PUSH}.get(
                a.get("kind", "text")
            )
            if kind is None:
                rep.err(loc, f"kind= must be text, flag or push, got {a['kind']!r}")
                continue
            sname = a.get("script", "")
            if sname:
                sc = next((x for x in m.scripts if x.name == sname), None)
                want = SCRT_NAMES["step" if mode == 1 else "interact"]
                if sc is None:
                    rep.err(
                        loc,
                        f"script={sname!r} is not defined in this map (@script {sname} ...)",
                    )
                    continue
                if sc.trigger not in (want, 0):
                    rep.err(
                        loc,
                        f"script {sname!r} is declared {trig_name(sc.trigger)} but used by a "
                        f"{'step' if mode == 1 else 'look'} interaction",
                    )
                    continue
                if "text" in a or "kind" in a:
                    rep.err(loc, "script= replaces text=/kind=")
                dm = 0xFF
                if "dirs" in a:
                    dm = parse_dirs(a["dirs"])
                    if dm is None:
                        rep.err(loc, f"bad dirs={a['dirs']}")
                        dm = 0xFF
                if mode == 1:
                    if cf[j * W + i] & C_SOLID:
                        rep.err(loc, "step interaction on a solid tile")
                    cf[j * W + i] |= C_TRIGGER
                m.interacts.append(Interact(i, j, dm, mode, IK_SCRIPT, "", 0, sname))
                continue
            key = a.get("text")
            if not key:
                lg = obj_attr(a)
                r = cat.resolve(lg, i, j) if lg else None
                if r and r[0] == "one":
                    key = cat.sprites[r[1]].get("text")
            if not key:
                rep.err(
                    loc,
                    "interaction has no text= (and the object has no catalog default)",
                )
                continue
            dm = 0xFF
            if "dirs" in a:
                dm = parse_dirs(a["dirs"])
                if dm is None:
                    rep.err(loc, f"bad dirs={a['dirs']}")
                    dm = 0xFF
            flag = parse_int(a["flag"]) if "flag" in a else 0
            if (
                flag is None
                or (kind == IK_FLAG and "flag" not in a)
                or not 0 <= flag <= MAX_FLAG
            ):
                rep.err(loc, f"kind=flag needs flag=<n> (1..{MAX_FLAG})")
                flag = 0
            if mode == 1:
                if cf[j * W + i] & C_SOLID:
                    rep.err(loc, "step interaction on a solid tile")
                cf[j * W + i] |= C_TRIGGER
            m.interacts.append(Interact(i, j, dm, mode, kind, key, flag))
            addtext(key)

    # ---- npcs
    for j in range(H):
        for i in range(W):
            a = legend[ch_at(i, j)]
            if not any(
                k in a
                for k in (
                    "npc",
                    "item",
                    "cut",
                    "boulder",
                    "hole",
                    "sleeper",
                    "legend",
                    "trainer",
                    "keydoor",
                    "gift",
                )
            ):
                continue
            loc = f"{where}({i},{j})"
            gen_kind = next(
                (
                    k
                    for k in (
                        "item",
                        "cut",
                        "boulder",
                        "hole",
                        "sleeper",
                        "legend",
                        "trainer",
                        "keydoor",
                        "gift",
                    )
                    if k in a
                ),
                "",
            )
            gen_script = None
            if gen_kind:
                if (
                    "npc" in a
                    and gen_kind in ("item", "cut", "boulder", "hole")
                    or "npc" not in a
                    and gen_kind in ("trainer", "sleeper", "legend")
                ):
                    rep.err(
                        loc,
                        "item= / cut= / boulder= / hole= take no npc= (the pouch / bush / rock / pit is the actor); trainer= and sleeper= need npc= (keydoor= and gift= may have one)",
                    )
                    continue
                if (
                    any(
                        k in a
                        for k in ("script", "talk_flag", "behavior", "period", "radius")
                    )
                    or (
                        gen_kind in ("item", "boulder", "hole")
                        and any(k in a for k in ("text", "face", "trainer"))
                    )
                    or (
                        gen_kind in ("cut", "boulder", "hole")
                        and any(k in a for k in ("show_flag", "hide_flag"))
                    )
                    or (
                        gen_kind
                        in (
                            "cut",
                            "boulder",
                            "hole",
                            "sleeper",
                            "legend",
                            "keydoor",
                            "gift",
                        )
                        and any(k in a for k in ("text", "trainer"))
                    )
                    or (gen_kind in ("keydoor", "gift") and "show_flag" in a)
                ):
                    rep.err(
                        loc,
                        f"{gen_kind}= generates the NPC's script / behaviour: no script= talk_flag= behavior= radius= "
                        f"(and for item= / cut= / boulder= / hole= no text= face= trainer=, show_flag= hide_flag=)",
                    )
                    continue
            else:
                for k in ("range", "after", "beaten_flag", "qty"):
                    if k in a:
                        rep.err(
                            loc,
                            f"{k}= needs trainer= (range / after / beaten_flag) or item= (qty)",
                        )
            if gen_kind not in ("sleeper", "legend", "gift") and "level" in a:
                rep.err(loc, "level= needs sleeper=, legend= or gift=")
            actor = (
                ITEM_SPRITE
                if gen_kind == "item"
                else (
                    (a["cut"] if a["cut"] not in ("yes", "1", "") else CUT_SPRITE)
                    if gen_kind == "cut"
                    else (
                        (
                            a["boulder"]
                            if a["boulder"] not in ("yes", "1", "")
                            else BOULDER_SPRITE
                        )
                        if gen_kind == "boulder"
                        else (
                            HOLE_SPRITE
                            if gen_kind == "hole"
                            else (
                                (a.get("npc") or KEYDOOR_SPRITE)
                                if gen_kind == "keydoor"
                                else (
                                    (a.get("npc") or ITEM_SPRITE)
                                    if gen_kind == "gift"
                                    else a["npc"]
                                )
                            )
                        )
                    )
                )
            )
            r = cat.resolve(actor)
            if r is None or r[0] != "one" or cat.sprites[r[1]]["kind"] != "actor":
                rep.err(loc, f"unknown actor {actor!r}")
                continue
            f = parse_facing(a.get("face", "S"))
            if f is None:
                rep.err(loc, f"bad face={a.get('face')}")
                f = 0
            # an NPC may stand ON a piece of furniture (legend `object=<name> npc=<actor>`: a prop such as a Crystal on
            # a bench); the render order puts it in front of that object
            if (cf[j * W + i] & C_WARP and gen_kind != "hole") or (
                cf[j * W + i] & C_SOLID and "object" not in a
            ):
                rep.err(loc, "NPC placed on a solid/warp tile")
            n = Npc(
                spr=-1,
                name=r[1],
                i=i,
                j=j,
                facing=f,
                ch=ch_at(i, j),
                ident=a.get("id", ""),
            )
            if gen_kind in (
                "boulder",
                "hole",
            ):  # C5 (contract 42.2): no script, no text; a hole's hide flag is the hole's flag (hole=FLAG)
                n.gen = gen_kind
                if gen_kind == "hole":
                    hv = flag_value(m, a["hole"])
                    if not hv:
                        rep.err(
                            loc,
                            f"hole= needs a flag: a number 1..{MAX_FLAG} or an @flag name declared above the legend",
                        )
                        continue
                    n.hide_flag = hv
            elif (
                gen_kind
            ):  # the generated sight / pick-up script (contract 18); the common code below sees a plain script NPC
                gen_script = gen_npc_script(
                    m, n, a, gen_kind, len(m.npcs), loc, cat, rep
                )
                if gen_script is None:
                    continue
                a = {k: v for k, v in a.items() if k not in ("text",)}
                a["script"] = gen_script
            n.text_key = a.get("text", "")
            n.script = a.get("script", "")
            if n.script:
                sc = next((x for x in m.scripts if x.name == n.script), None)
                if sc is None:
                    rep.err(loc, f"script={n.script!r} is not defined in this map")
                elif sc.trigger not in (SCRT_NAMES["npc"], 0):
                    rep.err(
                        loc,
                        f"script {n.script!r} is declared {trig_name(sc.trigger)}, not npc",
                    )
                if n.text_key or "talk_flag" in a:
                    rep.err(loc, "npc script= replaces text= / talk_flag=")
            elif not n.text_key and gen_kind not in ("boulder", "hole"):
                rep.err(loc, "npc has no text= (or script=)")
            for k, attr in (
                ("talk_flag", "talk_flag"),
                ("show_flag", "show_flag"),
                ("hide_flag", "hide_flag"),
            ):
                if k in a:
                    v = flag_value(m, a[k])
                    if v is None:
                        rep.err(
                            loc,
                            f"bad {k} (0..{MAX_FLAG}, or an @flag name declared above)",
                        )
                    else:
                        setattr(n, attr, v)
            if a.get("behavior", "static") == "turn":
                n.behavior = NPCB_TURN
                p = parse_int(a.get("period", "2000")) or 2000
                n.param = max(1, min(255, p // 250))
            elif a.get("behavior", "static") == "wander":
                # low nibble 2 = wander, high nibble = radius in tiles (1..15) around the NPC's map tile; param = period / 250 ms
                rad = parse_int(a.get("radius", "2"))
                if rad is None or not 1 <= rad <= 15:
                    rep.err(loc, "wander needs radius=1..15")
                    rad = 2
                n.behavior = NPCB_WANDER | (rad << 4)
                p = parse_int(a.get("period", "2000")) or 2000
                n.param = max(1, min(255, p // 250))
            elif a.get("behavior", "static") != "static":
                rep.err(loc, "behavior must be static, turn or wander")
            n.flags = 0 if a.get("solid", "yes") == "no" else 1
            n.gate = a.get("gate", "no") == "yes"
            if (
                gen_kind == "boulder"
            ):  # C5 (contract 42.2): a solid rock the Empuje module pushes
                n.behavior, n.flags = NPCB_BOULDER, 1
            elif (
                gen_kind == "hole"
            ):  # a pit: NOT solid (a boulder rolls onto it), vanishes when its flag is set
                n.behavior, n.flags = NPCB_HOLE, 0
            if gen_kind == "trainer":
                n.behavior = NPCB_TRAINER | (
                    parse_int(a.get("range", str(TRAINER_RANGE))) << 4
                )
                if n.talk_flag:
                    rep.err(loc, "internal: trainer talk_flag")
                if (
                    not n.flag_auto
                ):  # beaten_flag= given: a number (1..255) or an @flag name of this map
                    n.talk_flag = (
                        parse_int(n.flag_tok)
                        if parse_int(n.flag_tok) is not None
                        else m.flag_defs.get(n.flag_tok, BUILTIN_FLAGS.get(n.flag_tok))
                    )  # (C9: a builtin name such as league_e1 is valid too)
            if n.talk_flag and not n.script:
                if n.text_key.endswith(".a"):
                    n.text_key2 = n.text_key[:-1] + "b"
                else:
                    rep.err(
                        loc,
                        f"talk_flag set: text key {n.text_key!r} must end with '.a' "
                        f"(the second talk uses the '.b' key)",
                    )
            if n.script:
                n.flags |= NPCF_SCRIPT
            m.npcs.append(n)
            addtext(n.text_key)
            addtext(n.text_key2)

    # ---- spawns
    for k, (nm, i, j, f, loc) in enumerate(spawn_src):
        if not (0 <= i < W and 0 <= j < H):
            rep.err(loc, f"spawn {nm} outside the map")
            continue
        m.spawns.append(Spawn(k, nm, i, j, f))
    for s in m.spawns:
        c = cf[s.j * W + s.i]
        if c & C_SOLID:
            rep.err(f"{where}", f"spawn {s.name} at ({s.i},{s.j}) is on a solid tile")
        if c & (C_WARP | C_TRIGGER):
            rep.err(
                f"{where}", f"spawn {s.name} at ({s.i},{s.j}) is on a warp/trigger tile"
            )
        if c & C_LEDGE:
            rep.err(f"{where}", f"spawn {s.name} at ({s.i},{s.j}) is on a ledge")
        if any(n.i == s.i and n.j == s.j and n.flags & 1 for n in m.npcs):
            rep.err(f"{where}", f"spawn {s.name} at ({s.i},{s.j}) is on a solid NPC")
    if not m.spawns:
        rep.warn(where, "map has no spawns")
    for (
        n
    ) in m.npcs:  # a trainer that faces a wall / the map edge can never see the hero
        if n.gen == "trainer":
            ni, nj = n.i + DI[n.facing], n.j + DJ[n.facing]
            if not (0 <= ni < W and 0 <= nj < H) or cf[nj * W + ni] & (
                C_SOLID | C_LEDGE
            ):
                rep.warn(
                    f"{where}({n.i},{n.j})",
                    f"trainer faces a blocked tile ({ni},{nj}): it can never see the hero",
                )

    # ---- roaming wild creatures (contract 55): the roam area (a bit mask) and the NPC slots; the species sprites join the bank below
    roam_names = []
    if m.roam:
        nn, _tab, area, per, rloc = m.roam
        mask, cells = bytearray((W * H + 7) // 8), []
        taken = (
            {(n.i, n.j) for n in m.npcs}
            | {(s_[1], s_[2]) for s_ in spawn_src}
            | {(it_.i, it_.j) for it_ in m.interacts}
        )  # (contract 63: nor an interaction tile - an item, a sign, a switch: a roamer must never block one)
        for j in range(
            1, H - 1
        ):  # (the area stays off the border rows / columns: a neighbour of an area cell is then inside the map, nucleo/map.h map_roam_bit)
            for i in range(1, W - 1):
                c = cf[j * W + i]
                ok = (
                    ch_at(i, j) in area
                    and not c
                    & (
                        C_SOLID
                        | C_WATER
                        | C_WARP
                        | C_TRIGGER
                        | C_LEDGE
                        | C_SLIDE
                        | C_COUNTER
                    )
                    if area
                    else not c
                    & (
                        C_SOLID
                        | C_GRASS
                        | C_WATER
                        | C_WARP
                        | C_TRIGGER
                        | C_LEDGE
                        | C_SLIDE
                        | C_COUNTER
                    )
                )  # (an explicit area may include the tall grass / a cave floor: the author's call)
                if ok and (i, j) not in taken:
                    mask[(j * W + i) >> 3] |= 1 << ((j * W + i) & 7)
                    cells.append((i, j))
        if not m.enc:
            rep.err(
                rloc,
                "@roam needs the map's `@encounters grass` table (its species and levels)",
            )
        elif len(m.npcs) + nn > 16:
            rep.err(
                rloc,
                f"@roam {nn}: the map has {len(m.npcs)} NPCs and the cap is 16 (nucleo/world.h WORLD_MAX_NPCS): {16 - len(m.npcs)} slot(s) are free",
            )
        elif len(cells) < 4 * nn:
            rep.err(
                rloc,
                f"@roam {nn}: the roam area has only {len(cells)} free walkable cell(s) (need at least {4 * nn})"
                + (
                    ""
                    if area
                    else ": no `area=` given, so tall grass / water are excluded"
                ),
            )
        else:
            # contract 66: a slot whose species is the Solar form of an affinity pair never roams (its actor index byte is ROAM_NONE): a Lunar player would meet the counterpart on touching it
            roam_names = [
                None if sid_ in affinity_sets()[0] else f"roam_{nm}"
                for sid_, _lo, _hi, _wt, nm in m.enc
            ]
            bad = [
                x
                for x in dict.fromkeys(roam_names)
                if x is not None and x not in cat.sprites
            ]
            if bad:
                rep.err(
                    rloc,
                    f"no roam actor for {', '.join(bad)}: run python3 tools/roam_ow.py build (assets/catalog_roam.toml)",
                )
                roam_names = []
            elif all(x is None for x in roam_names):
                rep.err(
                    rloc,
                    "@roam: every species of the grass table is affinity-exclusive (contract 66): none can roam",
                )
                roam_names = []
            else:
                m.roam_mask = bytes(mask)
                hi_, hj_ = cells[0]
                for _r in range(
                    nn
                ):  # the LAST nn NPC slots: a hidden NPC parked on the first area cell until the engine places it (behavior 6, solid, no text / script / flags)
                    m.npcs.append(
                        Npc(
                            spr=-1,
                            name=next(x for x in roam_names if x),
                            i=hi_,
                            j=hj_,
                            facing=0,
                            behavior=NPCB_ROAM,
                            flags=1,
                            param=per // 250,
                            gen="roam",
                            show_flag=ROAM_OFF_FLAG,
                        )
                    )

    # ---- bank + binary-semantic arrays
    bank, bidx = [], {}

    def bi(name):
        if name not in bidx:
            bidx[name] = len(bank)
            bank.append(name)
        return bidx[name]

    kind_order = {"tile": 0, "object": 1, "actor": 2}
    used = []
    for n in ground_names:
        if n:
            used.append(n)
    used += [o[0] for o in obj_list]
    used += [n.name for n in m.npcs]
    used += [x for x in roam_names if x]
    for n in sorted(
        set(used), key=lambda x: (kind_order[cat.sprites[x]["kind"]], used.index(x))
    ):
        bi(n)
    if (
        m.balloon
    ):  # the "!" balloon: the LAST bank sprite (header flag MAPF_BALLOON), contract 18
        if (
            BALLOON_SPRITE not in cat.sprites
            or cat.sprites[BALLOON_SPRITE]["kind"] != "object"
        ):
            rep.err(
                where,
                f"trainers need the catalog object {BALLOON_SPRITE!r} (the balloon)",
            )
        else:
            bi(BALLOON_SPRITE)
    m.bank = bank
    m.ground = [NO_TILE if n is None else bidx[n] for n in ground_names]
    m.cflags = cf
    objs = [Obj(bidx[nm], nm, i0, j0, fw, fh) for (nm, i0, j0, fw, fh) in obj_list]
    objs.sort(key=lambda o: (o.i0 + o.fw - 1) + (o.j0 + o.fh - 1))  # stable
    m.objects = objs
    for n in m.npcs:
        n.spr = bidx[n.name]
    m.roam_spr = [ROAM_NONE if x is None else bidx[x] for x in roam_names]
    m.text_keys = texts
    if len(bank) > 255:
        rep.err(where, "more than 255 bank sprites")
    if m.enc and not m.enc_rate:
        rep.err(where, "encounter slots without a rate")
    if m.enc_rate and not m.enc:
        rep.err(where, "@encounters has no slots")
    if m.open_rate and not m.enc:
        rep.err(
            where,
            "@open_rate needs the map's `@encounters grass` table (its species and levels)",
        )
    if m.enc_rate and not any(c & C_GRASS for c in cf):
        rep.warn(where, "@encounters but the map has no tall grass (C_GRASS)")
    if m.enc_rate == 0 and any(c & C_GRASS for c in cf):
        rep.warn(where, "tall grass but no @encounters table")
    for kind, (_rate, slots) in m.tables.items():
        if not slots:
            rep.err(where, f"@encounters {kind} has no slots")
        elif not any(c & C_WATER for c in cf):
            rep.warn(
                where, f"@encounters {kind} but the map has no water tile (C_WATER)"
            )


# ---- world ----------------------------------------------------------------------------------
def load_world(root=ROOT, rep=None):
    rep = rep or Report()
    cat = Catalog(root / "assets" / "catalog.toml", rep)
    entries, start = [], None
    mp = root / "maps" / "maps.txt"
    for ln, raw in enumerate(open(mp), 1):
        line = strip_comment(raw)
        if not line:
            continue
        mm = re.match(r"^start\s*=\s*(\w+):(\w+)$", line)
        if mm:
            start = (mm.group(1), mm.group(2))
            continue
        mm = re.match(r"^(\d+)\s+(\w+)$", line)
        if not mm:
            rep.err(f"maps.txt:{ln}", f"cannot parse {line!r}")
            continue
        entries.append((int(mm.group(1)), mm.group(2)))
    ids = [e[0] for e in entries]
    if len(set(ids)) != len(ids) or len({e[1] for e in entries}) != len(entries):
        rep.err("maps.txt", "duplicate map id or name")
    maps = []
    for mid, name in sorted(entries):
        p = root / "maps" / f"{name}.map"
        if not p.is_file():
            rep.err("maps.txt", f"map file {p} not found")
            continue
        m = parse_map(p, cat, rep, name)
        if m is None:
            continue
        m.id = mid
        if m.name != name:
            rep.err(p.name, f"@map {m.name} does not match maps.txt name {name}")
        maps.append(m)
    if start is None:
        rep.err("maps.txt", "missing 'start = <map>:<spawn>'")
    w = World(cat, maps, start or ("", ""), rep)
    w.fly_texts = fly_label_lengths()
    link_and_validate(w, root)
    return w


def fly_label_lengths():
    """{gl.area.* key: characters of its (master language) text}: the Planeador's list label must fit (contract 46.1)."""
    import textc

    texts = textc.load(textc.master_path())
    return {
        k: sum(len(l) for l in v[0])
        for k, v in texts.items()
        if k.startswith("gl.area.")
    }


def script_warp_targets(m):
    """Names of the maps a map's scripts warp to (such a `warp <map> ...` instruction counts as the way back for the
    reciprocity warning; a map that is only left by a script does not warn about its own door).
    """
    return {
        ins.args[0]
        for sc in m.scripts
        for ins in sc.ins
        if ins.op == "warp" and ins.args
    }


def explicit_flag_ids(world):
    """Every flag number the sources use by hand (@flag names, npc flags, interact flags, numbers in scripts)."""
    used = set()
    names = {}
    for m in world.maps:
        names.update(m.flag_defs)
        used |= set(m.flag_defs.values())
    for m in world.maps:
        for n in m.npcs:
            used |= {n.show_flag, n.hide_flag, n.talk_flag}
        used |= {it.flag for it in m.interacts}
        for sc in m.scripts:
            toks = [sc.cond] if sc.cond else []
            toks += [
                a for i in sc.ins for kd, a in zip(i.kinds, i.args) if kd == "flag"
            ]
            for t in toks:
                v = names.get(t, parse_int(t))
                if v:
                    used.add(v)
    used.discard(0)
    return used


def alloc_auto_flags(world, root):
    """Numbers the flags of trainers (beaten) and ground items (taken) that did not name one. The real maps' numbers come from
    AUTO_FLAG_POOL and are remembered in maps/auto_flags.txt (`name id`, never reassigned, so a save keeps meaning the same
    trainer / item after content edits); the test maps (id >= TEST_MAP_BASE) take FIXTURE_FLAG_POOL and are not remembered.
    """
    rep = world.report
    todo = [(m, n) for m in world.maps for n in m.npcs if n.flag_auto] + [
        (m, h) for m in world.maps for h in m.hidden if h.flag_auto
    ]  # (F2, contract 74: a hidden item has no NPC, it takes its number the same way)
    if not todo:
        return
    path = Path(root) / "maps" / AUTO_FLAGS_FILE
    lock = {}
    if path.is_file():
        for ln, raw in enumerate(open(path, encoding="utf-8"), 1):
            t = strip_comment(raw).split()
            if not t:
                continue
            v = parse_int(t[1]) if len(t) == 2 else None
            if v is None or v not in AUTO_FLAG_VALID or t[0] in lock:
                rep.err(
                    f"{AUTO_FLAGS_FILE}:{ln}",
                    f"expected `<name> <id of the automatic pool (see AUTO_FLAG_POOL / LEGACY_AUTO_POOL)>` (unique name)",
                )
                continue
            lock[t[0]] = v
    used = explicit_flag_ids(world)
    for nm, v in lock.items():
        if v in used:
            rep.err(
                AUTO_FLAGS_FILE,
                f"{nm} = {v} collides with a flag the maps use by hand: pick another number or delete the line",
            )
    taken = used | set(lock.values())
    changed = False
    for m, n in todo:
        nm = n.flag_auto
        if m.id >= TEST_MAP_BASE:
            pool, v = FIXTURE_FLAG_POOL, None
        else:
            pool, v = AUTO_FLAG_POOL, lock.get(nm)
        if v is None:
            v = next((x for x in pool if x not in taken), None)
            if v is None:
                rep.err(
                    m.name,
                    f"no free automatic flag left in the pool ({len(pool)} flags)",
                )
                continue
            taken.add(v)
            if m.id < TEST_MAP_BASE:
                lock[nm] = v
                changed = True
        m.flag_defs[nm] = v
        if isinstance(n, HiddenItem):
            n.interact.flag = v
        elif n.gen == "trainer":
            n.talk_flag = v
        else:
            n.hide_flag = v
    if changed and path.parent.is_dir():
        path.write_text(
            "# Automatic flags of trainers (beaten) and ground items (taken): `name id`, written by tools/mapc.py, never reassigned\n"
            "# (a save keeps meaning the same trainer / item). Delete a line only for a trainer / item that is gone for good.\n"
            + "".join(f"{k} {v}\n" for k, v in lock.items()),
            encoding="utf-8",
        )  # (file order kept, new names at the end: a merge of two agents' lines stays a plain append)


def link_and_validate(w, root):
    rep, maps = w.report, {m.name: m for m in w.maps}
    # warps
    for m in w.maps:
        for wp in m.warps:
            loc = f"{m.name}({wp.i},{wp.j})"
            tm = maps.get(wp.target_map)
            if tm is None:
                rep.err(loc, f"warp target map {wp.target_map!r} not in maps.txt")
                continue
            if (
                wp.pad is not None
            ):  # C6 (contract 46.3): a teleport pad: the target is another pad cell
                ti, tj, pf = wp.pad
                if (ti, tj) not in tm.pad_cells:
                    rep.err(
                        loc, f"pad target {tm.name}({ti},{tj}) is not a pad=yes cell"
                    )
                    continue
                if tm is m and (ti, tj) == (wp.i, wp.j):
                    rep.err(loc, "a pad cannot lead to itself")
                    continue
                ei, ej = ti + DI[pf], tj + DJ[pf]
                if tm.blocked_at(ei, ej):
                    rep.warn(
                        loc,
                        f"pad target {tm.name}({ti},{tj}) faces {FACINGS[pf]} but the tile in front ({ei},{ej}) is blocked: the hero would arrive facing a wall",
                    )
                wp.target_map_id, wp.target_i, wp.target_j, wp.target_facing = (
                    tm.id,
                    ti,
                    tj,
                    pf,
                )
                continue
            sp = tm.spawn(wp.target_spawn)
            if sp is None:
                rep.err(
                    loc,
                    f"warp target spawn {wp.target_map}:{wp.target_spawn} does not exist",
                )
                continue
            if tm.cflags[sp.j * tm.w + sp.i] & C_WARP:
                rep.err(loc, f"target spawn {wp.target_map}:{sp.name} is on a warp")
            wp.target_map_id, wp.target_i, wp.target_j, wp.target_facing = (
                tm.id,
                sp.i,
                sp.j,
                sp.facing,
            )
        targets = {wp.target_map for wp in m.warps if wp.pad is None and not wp.oneway}
        for t in targets:
            tm = maps.get(t)
            if (
                tm
                and not any(x.target_map == m.name and x.pad is None for x in tm.warps)
                and m.name not in script_warp_targets(tm)
            ):
                rep.warn(m.name, f"warps to {t} but {t} has no warp back to {m.name}")
    # C6 (contract 46.1): the Planeador's towns: unique slots, an existing label text, a landing tile the hero can stand on
    slots = {}
    for m in w.maps:
        if m.fly is None:
            continue
        slot, key, fi, fj, ff, floc = m.fly
        if slot in slots:
            rep.err(floc, f"@fly slot {slot} is also used by {slots[slot]}")
        slots[slot] = m.name
        if (
            not (0 <= fi < m.w and 0 <= fj < m.h)
            or m.blocked_at(fi, fj)
            or m.cflags[fj * m.w + fi] & (C_WARP | C_TRIGGER)
        ):
            rep.err(
                floc,
                f"@fly landing tile ({fi},{fj}) is outside the map, blocked, or a door / trigger tile",
            )
        if m.interior or m.cave or m.dark:
            rep.err(
                floc,
                "a Planeador town must be an outdoor map (not @interior / @cave / @dark)",
            )
        if f"gl.area.{key}" not in w.fly_texts:
            rep.err(
                floc,
                f"@fly town_key {key!r}: the text gl.area.{key} does not exist (assets/text/<lang>.txt)",
            )
        elif w.fly_texts[f"gl.area.{key}"] > FLY_LABEL_MAX:
            rep.err(
                floc,
                f"@fly: the name gl.area.{key} is {w.fly_texts[f'gl.area.{key}']} characters, the list holds {FLY_LABEL_MAX}",
            )
    # C6 (contract 46.3): the teleport pads are edges of the reachability model: a pad that can be reached puts the hero on its target pad (which may be in another map). Iterate to a fixed point.
    for _ in range(64):
        changed = False
        for m in w.maps:
            if not any(wp.pad for wp in m.warps):
                continue
            seen, _stand = bfs(m)
            for wp in m.warps:
                if wp.pad and (wp.i, wp.j) in seen:
                    tm = maps[wp.target_map]
                    if (wp.target_i, wp.target_j) not in tm.extra_seeds:
                        tm.extra_seeds.add((wp.target_i, wp.target_j))
                        changed = True
        if not changed:
            break
    # scripts: global names, then assemble every map once to validate operands (text ids need the plan)
    alloc_auto_flags(w, root)
    check_script_names(w)
    for m in w.maps:
        assemble_scripts(w, m, rep)
        used = {i.script for i in m.interacts if i.script} | {
            n.script for n in m.npcs if n.script
        }
        for sc in m.scripts:
            for ins in sc.ins:
                if ins.op == "call" and ins.args:
                    used.add(ins.args[0])
        for sc in m.scripts:
            if (
                sc.trigger not in (SCRT_NAMES["on_enter"], SCRT_NAMES["called"])
                and sc.name not in used
            ):
                rep.warn(
                    m.name,
                    f"script {sc.name} ({trig_name(sc.trigger)}) is not used by any "
                    f"legend script=, npc or call",
                )
    sm, ss = w.start
    if w.start != ("", ""):
        if sm not in maps or maps[sm].spawn(ss) is None:
            rep.err("maps.txt", f"start {sm}:{ss} does not exist")
    # text keys (the master language owns the key set; `textc.py lint` checks the other languages)
    import mkfont

    es = Path(mkfont.get_lang().text_path)
    if es.is_file():
        keys = set(
            re.findall(r"^\[([^\]\s]+)\]\s*$", es.read_text(encoding="utf-8"), re.M)
        )
        for m in w.maps:
            for k in m.text_keys:
                if k not in keys:
                    rep.err(m.name, f"text key [{k}] missing in {es.relative_to(root)}")
        for k in AREA_KEYS:
            if f"gl.area.{k}" not in keys:
                rep.err(
                    "areas", f"text key [gl.area.{k}] missing in {es.relative_to(root)}"
                )
    else:
        n = sum(len(m.text_keys) for m in w.maps)
        rep.warn("texts", f"{es.name} not found: {n} text keys unchecked")
    for m in w.maps:
        check_ledges(m, rep)
        check_slides(m, rep)
        check_reach(m, rep)
    # art availability note
    missing = sorted(
        {
            s
            for m in w.maps
            for s in m.bank
            if w.catalog.sprites[s]["kind"] != "actor" and w.catalog.art(s) is None
        }
    )
    if missing:
        rep.note(
            f"no .lv.png yet for {len(missing)} sprite(s) (placeholders in preview): "
            + ", ".join(missing)
        )


# ---- reachability ----------------------------------------------------------------------------
def slide_run(m, i, j):
    """C4-A (contract 35.3): the hero arrives on the arrow floor (i, j). Returns (path, why): path = the tiles he is pushed onto, one after the other (the last one is where he stops), why =
    'free' (a tile that does not push), 'wall' (the next tile is blocked: the corner rule applies to the screen-cardinal facings), 'warp' (a warp / trigger tile: arriving there fires it),
    'cycle' (the next tile was visited already in this run: nucleo/world.c ends a run when it is back on its first tile, so a pure loop stops there; 'rho' = it falls into a loop that does not
    contain its first tile, which the engine could only end with its 64-push cap: an error).
    """
    path, seen, pos = [], {(i, j)}, (i, j)
    while True:
        f = m.slides.get(pos)
        if f is None:
            return path, "free"
        ni, nj = pos[0] + DI[f], pos[1] + DJ[f]
        if m.blocked_at(ni, nj) or (
            not f & 1 and (m.blocked_at(ni, pos[1]) or m.blocked_at(pos[0], nj))
        ):
            return path, "wall"
        if (ni, nj) in seen:
            return path, "cycle" if (ni, nj) == (i, j) else "rho"
        seen.add((ni, nj))
        path.append((ni, nj))
        pos = (ni, nj)
        if m.cflags[nj * m.w + ni] & (C_WARP | C_TRIGGER):
            return path, "warp"


def bfs(m):
    """Returns (seen, standable): seen = tiles the hero can arrive on; standable = seen and not a
    warp/trigger (arriving there teleports/pushes, so they are not expanded).
    Ledges (one-way): the ledge tile itself is never entered; from the tile before it, facing its jump direction (a world
    axis), the hero hops over it and lands on the tile beyond (nucleo/world.c try_hop).
    """
    W = m.w
    # C5 (contract 42.1): `@surf yes` = the map is crossed by surfing too: its water tiles (C_WATER, not under a solid object) count as walkable for the reachability model
    blk = lambda i, j: m.blocked_at(i, j) and not (
        m.surf
        and 0 <= i < W
        and 0 <= j < m.h
        and bool(m.cflags[j * W + i] & C_WATER)
        and not any(
            n.i == i
            and n.j == j
            and n.flags & 1
            and n.gen not in ("cut", "boulder", "sleeper", "legend", "keydoor", "roam")
            for n in m.npcs
        )
    )
    # C4b (contract 41): a step trigger whose script is CONDITIONAL (`@script x step if !FLAG`: the ghost on the tower stairs, the rival's ambush line) does not end the hero's walk for the reachability model: once
    # its condition is clear it does nothing, so what lies beyond it is reachable (an unconditional trigger and a warp still end it)
    scr = {s_.name: s_ for s_ in getattr(m, "scripts", [])}
    soft = {
        (it.i, it.j)
        for it in getattr(m, "interacts", [])
        if it.mode == 1
        and it.script
        and scr.get(it.script) is not None
        and scr[it.script].cond
    }
    term = (
        lambda i, j: bool(m.cflags[j * W + i] & (C_WARP | C_TRIGGER))
        and (i, j) not in soft
    )
    ledge = {(i, j): d for i, j, d in m.ledges}
    seen, stand = set(), set()
    q = []
    for si, sj in [(s_.i, s_.j) for s_ in m.spawns] + sorted(
        getattr(m, "extra_seeds", ())
    ):  # (C6: a teleport pad of a reachable pad lands on its target cell)
        if not blk(si, sj):
            seen.add((si, sj))
            stand.add((si, sj))
            q.append((si, sj))
    while q:
        i, j = q.pop()
        for f in range(8):
            di, dj = DI[f], DJ[f]
            ni, nj = i + di, j + dj
            if ledge.get((ni, nj)) is not None:
                if ledge[(ni, nj)] == f:  # the jump direction: land on the tile beyond
                    ni, nj = ni + di, nj + dj
                    if blk(ni, nj):
                        continue
                else:
                    continue
            elif blk(ni, nj):
                continue
            elif di and dj and (blk(i + di, j) or blk(i, j + dj)):
                continue  # strict corner rule (STRICT_CORNERS 1 in nucleo/config.h, contract §5)
            if (ni, nj) in seen:
                continue
            seen.add((ni, nj))
            if (ni, nj) in m.slides and not term(
                ni, nj
            ):  # an arrow floor: the hero is pushed on, he stops at the end of the run (C4-A)
                path, _why = slide_run(m, ni, nj)
                seen.update(path)
                if path and not term(*path[-1]) and path[-1] not in stand:
                    stand.add(path[-1])
                    q.append(path[-1])
                elif not path:
                    stand.add((ni, nj))
                    q.append((ni, nj))
                continue
            if not term(ni, nj):
                stand.add((ni, nj))
                q.append((ni, nj))
    return seen, stand


def check_ledges(m, rep):
    where = m.name
    for i, j, d in m.ledges:
        li, lj = i + DI[d], j + DJ[d]
        if m.blocked_at(li, lj) or m.cflags[lj * m.w + li] & (C_WARP | C_TRIGGER):
            rep.err(
                where,
                f"ledge ({i},{j}) {FACINGS[d]}: the landing tile ({li},{lj}) is blocked / a door / a trigger",
            )
        ai, aj = i - DI[d], j - DJ[d]
        if m.blocked_at(ai, aj):
            rep.warn(
                where,
                f"ledge ({i},{j}) {FACINGS[d]}: the tile it is jumped from ({ai},{aj}) is blocked (unusable)",
            )


def check_slides(m, rep):
    """Arrow floors (C4-A): every run must end on a tile that does not push, a wall or a warp, or be a pure loop (a warning: the engine stops the hero when he is back on the first tile)."""
    loops = set()
    for i, j in sorted(m.slides):
        path, why = slide_run(m, i, j)
        if why == "rho":
            rep.err(
                m.name,
                f"arrow floors: stepping on ({i},{j}) pushes the hero into a loop that does not contain ({i},{j}) (a run ends only when it is back on its first tile): close the loop or open it",
            )
        elif why == "cycle" and frozenset(path + [(i, j)]) not in loops:
            loops.add(frozenset(path + [(i, j)]))
            rep.warn(
                m.name,
                f"arrow floors loop: stepping on ({i},{j}) pushes the hero round the loop once and stops him back on ({i},{j}) (nucleo/world.c)",
            )
    for s_ in m.spawns:
        if (s_.i, s_.j) in m.slides:
            rep.warn(
                m.name,
                f"spawn {s_.name} at ({s_.i},{s_.j}) is on an arrow floor (it pushes only when a step arrives on it)",
            )


def check_reach(m, rep):
    seen, stand = bfs(m)
    where = m.name

    def probe(i, j, dirmask=0xFF):
        for f in range(8):
            ni, nj = i - DI[f], j - DJ[f]  # standing at (ni,nj) facing f hits (i,j)
            if (ni, nj) in stand and dirmask & (1 << f):
                return True
        return False

    for wp in m.warps:
        if (wp.i, wp.j) not in seen:
            rep.err(where, f"warp at ({wp.i},{wp.j}) is unreachable from the spawns")
    for it in m.interacts:
        if it.mode == 1:
            if (it.i, it.j) not in seen:
                rep.err(
                    where,
                    f"step trigger at ({it.i},{it.j}) is unreachable from the spawns",
                )
        elif not probe(it.i, it.j, it.dirmask):
            rep.err(
                where,
                f"interaction at ({it.i},{it.j}) cannot be reached/faced from the spawns",
            )
    for n in m.npcs:
        if n.gen != "roam" and not probe(n.i, n.j):
            rep.err(
                where,
                f"NPC {n.name} at ({n.i},{n.j}) cannot be reached from the spawns",
            )


# ---- preview --------------------------------------------------------------------------------
def lvl_color(l):
    t = max(0, min(3, l)) / 3.0
    return tuple(int(round(ORANGE[k] + (INK[k] - ORANGE[k]) * t)) for k in range(3))


def art_to_rgba(art):
    """lv.png (grays 255/170/85/0, alpha) -> LCD-look RGBA image."""
    from PIL import Image

    src = art["img"]
    out = Image.new("RGBA", src.size, (0, 0, 0, 0))
    sp, op = src.load(), out.load()
    for y in range(src.size[1]):
        for x in range(src.size[0]):
            r, g, b, a = sp[x, y]
            if a < 128:
                continue
            lv = int(round((255 - g) / 85.0))
            op[x, y] = lvl_color(lv) + (255,)
    return out


PH_HEIGHT = {
    "wall": 26,
    "fence": 10,
    "skirt": 6,
    "sign": 14,
    "tree": 38,
    "rock": 8,
    "house": 34,
    "lab": 38,
    "bed": 8,
    "table": 9,
    "bookshelf": 26,
    "tv": 12,
    "pc": 14,
    "machine": 22,
}
PH_NOLABEL = ("wall", "fence", "skirt", "tree", "rock", "fb_table")


def ph_height(name):
    n = name[3:] if name.startswith("fb_") else name
    for k, v in PH_HEIGHT.items():
        if n.startswith(k):
            return v
    return 16


class Canvas:
    def __init__(self, m, S=2):
        from PIL import Image, ImageDraw

        self.m, self.S = m, S
        self.pad = 24
        self.padt = 64
        self.padb = 40
        self.ox = (m.h - 1) * 14 + 14 + self.pad
        self.oy = 7 + self.padt
        self.W = (m.w + m.h) * 14 + 2 * self.pad
        self.H = (m.w + m.h - 2) * 7 + 14 + self.padt + self.padb
        self.img = Image.new("RGBA", (self.W, self.H), (236, 214, 178, 255))
        self.d = ImageDraw.Draw(self.img)

    def P(self, a, b):
        return (self.ox + (a - b) * 14, self.oy + (a + b) * 7)

    def centre(self, i, j):
        return self.P(i, j)

    def diamond(self, i, j, fill, outline=None):
        cx, cy = self.centre(i, j)
        pts = [(cx, cy - 7), (cx + 14, cy), (cx, cy + 7), (cx - 14, cy)]
        self.d.polygon(pts, fill=fill, outline=outline)

    def paste(self, im, x, y):
        self.img.paste(im, (int(x), int(y)), im)


def render_preview(w, m, out_png, S=2):
    from PIL import Image, ImageDraw, ImageFont

    cat = w.catalog
    cv = Canvas(m, S)
    d = cv.d
    labels = []  # (x, y, text, rgb) in 1x coords, drawn after upscaling

    # map extent outline
    top, right = cv.P(-0.5, -0.5), cv.P(m.w - 0.5, -0.5)
    bot, left = cv.P(m.w - 0.5, m.h - 0.5), cv.P(-0.5, m.h - 0.5)
    d.polygon([top, right, bot, left], fill=lvl_color(m.bg) + (255,))

    def tile_placeholder(name, i, j):
        s = cat.sprites[name]
        fl = s.get("fill", 0)
        fl = 0 if fl is None else fl
        cv.diamond(i, j, lvl_color(fl) + (255,), lvl_color(min(3, fl + 1)) + (255,))
        cx, cy = cv.centre(i, j)
        if name == "flowers":
            for dx, dy in ((-5, -1), (4, 2), (-1, 3)):
                d.point((cx + dx, cy + dy), fill=INK + (255,))
        elif name.startswith("grass"):
            d.point(
                (cx + (zlib.crc32(name.encode()) % 5) - 2, cy),
                fill=lvl_color(2) + (255,),
            )
        elif name == "mat":
            d.line([(cx - 6, cy), (cx + 6, cy)], fill=lvl_color(0) + (255,))

    # ground
    order = sorted(((i + j, i - j, i, j) for j in range(m.h) for i in range(m.w)))
    for _, _, i, j in order:
        b = m.ground[j * m.w + i]
        if b == NO_TILE:
            continue
        name = m.bank[b]
        art = cat.art(name)
        if art:
            s = cat.sprites[name]
            fl = s.get("fill")
            if fl is not None:
                cv.diamond(i, j, lvl_color(fl) + (255,))
            cx, cy = cv.centre(i, j)
            cv.paste(art_to_rgba(art), cx - art["ax"], cy - art["ay"])
        else:
            tile_placeholder(name, i, j)

    # drawables: objects + npcs interleaved by front tile
    draws = []
    for o in m.objects:
        draws.append(((o.i0 + o.fw - 1) + (o.j0 + o.fh - 1), 0, ("obj", o)))
    for n in m.npcs:
        if n.gen != "roam":
            draws.append((n.i + n.j, 1, ("npc", n)))
    draws.sort(key=lambda t: (t[0], t[1]))
    for _, _, (kind, o) in draws:
        if kind == "obj":
            art = cat.art(o.name)
            i1, j1 = o.i0 + o.fw - 1, o.j0 + o.fh - 1
            if art:
                cx, cy = cv.centre(i1, j1)
                ax_pt = (cx, cy + 7)
                cv.paste(art_to_rgba(art), ax_pt[0] - art["ax"], ax_pt[1] - art["ay"])
            else:
                H = ph_height(o.name)
                a0, b0, a1, b1 = (
                    o.i0 - 0.5,
                    o.j0 - 0.5,
                    o.i0 + o.fw - 0.5,
                    o.j0 + o.fh - 0.5,
                )
                B = {
                    k: cv.P(*p)
                    for k, p in (
                        ("t", (a0, b0)),
                        ("r", (a1, b0)),
                        ("b", (a1, b1)),
                        ("l", (a0, b1)),
                    )
                }
                up = lambda p: (p[0], p[1] - H)
                ink = INK + (255,)
                d.polygon(
                    [B["l"], B["b"], up(B["b"]), up(B["l"])],
                    fill=lvl_color(1) + (255,),
                    outline=ink,
                )
                d.polygon(
                    [B["b"], B["r"], up(B["r"]), up(B["b"])],
                    fill=lvl_color(2) + (255,),
                    outline=ink,
                )
                d.polygon(
                    [up(B["t"]), up(B["r"]), up(B["b"]), up(B["l"])],
                    fill=lvl_color(0) + (255,),
                    outline=ink,
                )
                if not o.name.startswith(PH_NOLABEL):
                    cx, cy = (up(B["t"])[0] + up(B["b"])[0]) / 2, (
                        up(B["t"])[1] + up(B["b"])[1]
                    ) / 2
                    labels.append((cx, cy, o.name.replace("fb_", ""), INK, "m"))
        else:
            cx, cy = cv.centre(o.i, o.j)
            fx, fy = cx, cy + 4
            ink = INK + (255,)
            d.ellipse([fx - 5, fy - 3, fx + 5, fy + 1], fill=lvl_color(2) + (255,))
            d.rectangle(
                [fx - 3, fy - 14, fx + 3, fy - 3],
                fill=lvl_color(1) + (255,),
                outline=ink,
            )
            d.ellipse(
                [fx - 3, fy - 21, fx + 3, fy - 14],
                fill=lvl_color(0) + (255,),
                outline=ink,
            )
            # facing tick
            f = o.facing
            sx, sy = (DI[f] - DJ[f]) * 4, (DI[f] + DJ[f]) * 2
            d.line([(fx, fy - 9), (fx + sx, fy - 9 + sy)], fill=ink)

    # upscale then draw markers + labels at S x
    img = cv.img.resize((cv.W * S, cv.H * S), Image.NEAREST).convert("RGB")
    dd = ImageDraw.Draw(img)
    try:
        f_small = ImageFont.load_default(size=11)
        f_tiny = ImageFont.load_default(size=9)
    except TypeError:
        f_small = f_tiny = ImageFont.load_default()

    def txt(x, y, s, rgb, font=f_small, anchor="mm", halo=True):
        if halo:
            for ox_, oy_ in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                dd.text(
                    (x * S + ox_, y * S + oy_),
                    s,
                    fill=(255, 255, 255),
                    font=font,
                    anchor=anchor,
                )
        dd.text((x * S, y * S), s, fill=rgb, font=font, anchor=anchor)

    for x, y, s, rgb, _ in labels:
        txt(x, y, s, rgb, f_tiny, halo=False)

    MAG, GRN, CYA, YEL, RED = (
        (230, 0, 160),
        (0, 170, 60),
        (0, 150, 230),
        (250, 220, 0),
        (225, 30, 30),
    )
    for wp in m.warps:
        cx, cy = cv.centre(wp.i, wp.j)
        pts = [(cx - 6, cy - 4), (cx + 6, cy - 4), (cx, cy + 5)]
        dd.polygon([(x * S, y * S) for x, y in pts], fill=MAG, outline=(255, 255, 255))
        txt(
            cx,
            cy - 11,
            f"-> {wp.target_map.replace('villa_', '')}:{wp.target_spawn}",
            MAG,
            f_tiny,
        )
    for it in m.interacts:
        cx, cy = cv.centre(it.i, it.j)
        col = YEL if it.mode == 0 else RED
        r = 5
        dd.polygon(
            [
                (cx * S, (cy - r) * S),
                ((cx + r) * S, cy * S),
                (cx * S, (cy + r) * S),
                ((cx - r) * S, cy * S),
            ],
            fill=col,
            outline=(0, 0, 0),
        )
        txt(cx, cy, "!" if it.mode == 0 else "T", (0, 0, 0), f_tiny, halo=False)
        txt(cx, cy + 12, it.text_key, (60, 40, 0), f_tiny)
    for n in (n for n in m.npcs if n.gen != "roam"):
        cx, cy = cv.centre(n.i, n.j)
        dd.rectangle(
            [(cx - 7) * S, (cy - 24) * S, (cx + 7) * S, (cy + 6) * S],
            outline=CYA,
            width=2,
        )
        txt(cx, cy - 29, f"{n.name} {FACINGS[n.facing]}", (0, 90, 160), f_tiny)
    for s in m.spawns:
        cx, cy = cv.centre(s.i, s.j)
        dd.ellipse(
            [(cx - 5) * S, (cy - 3) * S, (cx + 5) * S, (cy + 3) * S],
            outline=GRN,
            width=3,
        )
        fx, fy = (DI[s.facing] - DJ[s.facing]) * 9, (DI[s.facing] + DJ[s.facing]) * 4.5
        dd.line([(cx * S, cy * S), ((cx + fx) * S, (cy + fy) * S)], fill=GRN, width=3)
        txt(cx, cy + 12, f"spawn {s.name}", (0, 110, 40), f_tiny)

    # legend/title
    title = f"{m.name}  {m.w}x{m.h}  id {m.id}   (i = down-right, j = down-left)"
    dd.text((8, 6), title, fill=INK, font=f_small)
    ly = 24
    for col, s in (
        (MAG, "warp"),
        (GRN, "spawn (+ facing)"),
        (CYA, "npc"),
        (YEL, "look interaction"),
        (RED, "step trigger"),
    ):
        dd.rectangle([8, ly + 2, 18, ly + 10], fill=col)
        dd.text((24, ly), s, fill=INK, font=f_tiny)
        ly += 13
    Path(out_png).parent.mkdir(parents=True, exist_ok=True)
    img.save(out_png)


def render_grid(m, out_png):
    from PIL import Image, ImageDraw, ImageFont

    C = 30
    mg = 22
    img = Image.new("RGB", (m.w * C + mg + 4, m.h * C + mg + 4), (250, 250, 250))
    d = ImageDraw.Draw(img)
    try:
        f = ImageFont.load_default(size=15)
        f2 = ImageFont.load_default(size=9)
    except TypeError:
        f = f2 = ImageFont.load_default()
    npcs = {(n.i, n.j): n for n in m.npcs if n.gen != "roam"}
    spawns = {(s.i, s.j): s for s in m.spawns}
    ints = {(it.i, it.j): it for it in m.interacts}
    for i in range(m.w):
        d.text(
            (mg + i * C + C // 2, 8), str(i), fill=(90, 90, 90), font=f2, anchor="mm"
        )
    for j in range(m.h):
        d.text(
            (9, mg + j * C + C // 2), str(j), fill=(90, 90, 90), font=f2, anchor="mm"
        )
    for j in range(m.h):
        for i in range(m.w):
            c = m.cflags[j * m.w + i]
            if c & C_SOLID:
                bg = (150, 150, 155)
            elif c & C_WARP:
                bg = (240, 150, 215)
            elif c & C_TRIGGER:
                bg = (245, 140, 140)
            elif c & C_LEDGE:
                bg = (200, 170, 120)
            elif c & C_GRASS:
                bg = (200, 235, 190)
            elif m.ground[j * m.w + i] == NO_TILE:
                bg = (60, 60, 60)
            else:
                bg = (235, 220, 185)
            x, y = mg + i * C, mg + j * C
            d.rectangle([x, y, x + C - 1, y + C - 1], fill=bg, outline=(120, 120, 120))
            ch = m.chars[j][i]
            d.text((x + C // 2, y + C // 2), ch, fill=(0, 0, 0), font=f, anchor="mm")
            if (i, j) in ints:
                d.rectangle(
                    [x + 1, y + 1, x + 8, y + 8],
                    fill=(250, 220, 0) if ints[(i, j)].mode == 0 else (225, 30, 30),
                )
            if (i, j) in npcs:
                d.rectangle(
                    [x + 2, y + 2, x + C - 3, y + C - 3], outline=(0, 150, 230), width=3
                )
            if (i, j) in spawns:
                s = spawns[(i, j)]
                d.ellipse([x + 4, y + C - 12, x + 12, y + C - 4], fill=(0, 170, 60))
                d.text(
                    (x + C - 3, y + C - 3),
                    FACINGS[s.facing],
                    fill=(0, 100, 30),
                    font=f2,
                    anchor="rd",
                )
    Path(out_png).parent.mkdir(parents=True, exist_ok=True)
    img.save(out_png)


# ---- scripts ---------------------------------------------------------------------------------------
# Opcodes and operand kinds; the numbers must match nucleo/script.h (mapdump --verify checks that).
# kinds: u8 steps dir flag var npc script (1 byte) | text label ms (2 bytes LE; label = signed rel16
# from the end of the instruction) | map (1 byte, resolved to the map id)
OPS = {
    "end": (0x00, []),
    "say": (0x01, ["text"]),
    "ask": (0x02, ["text"]),
    "jmp": (0x03, ["label"]),
    "jf": (0x04, ["flag", "label"]),
    "jnf": (0x05, ["flag", "label"]),
    "jv": (0x06, ["var", "u8", "label"]),
    "setf": (0x07, ["flag"]),
    "clrf": (0x08, ["flag"]),
    "setv": (0x09, ["var", "u8"]),
    "addv": (0x0A, ["var", "u8"]),
    "wait": (0x0B, ["ms"]),
    "npc_walk": (0x0C, ["npc", "dir", "steps"]),
    "npc_face": (0x0D, ["npc", "dir"]),
    "npc_face_hero": (0x0E, ["npc"]),
    "npc_show": (0x0F, ["npc"]),
    "npc_hide": (0x10, ["npc"]),
    "hero_walk": (0x11, ["dir", "steps"]),
    "hero_face": (0x12, ["dir"]),
    "cam_npc": (0x13, ["npc"]),
    "cam_hero": (0x14, []),
    "fade_out": (0x15, []),
    "fade_in": (0x16, []),
    "warp": (0x17, ["map", "u8", "u8", "dir"]),
    "lock": (0x18, []),
    "unlock": (0x19, []),
    "call_native": (0x1A, ["u8", "u8", "u8"]),
    "npc_place": (0x1B, ["npc", "u8", "u8", "dir"]),
    "npc_walk_nb": (0x1C, ["npc", "dir", "steps"]),
    "wait_idle": (0x1D, []),
    "jnv": (0x1E, ["var", "u8", "label"]),
    "hero_face_npc": (0x1F, ["npc"]),
    "call": (0x20, ["script"]),
}
OP_BY_CODE = {v[0]: k for k, v in OPS.items()}
WIDE_KINDS = {"text", "label", "ms"}
NATIVE_IDS = {
    "choose_starter": 1,
    "rival_pick": 2,
    "give": 3,
    "battle": 4,
    "heal_party": 5,
    "name_select": 6,
    "name_var": 7,
    "mon_show": 8,
    "wild_set": 9,
    "wild_go": 10,
    "give_item": 11,
    "shop": 12,
    "take_item": 13,
    "has_item": 14,
    "money_add": 15,
    "money_take": 16,
    "trainer_spot": 17,
    "trainer_come": 18,
    "blackout": 19,
    "item_var": 20,
    "nick_prompt": 21,
    "pc": 22,
    "world": 23,
}
# kinds: money = yen, a multiple of 10 (the operand is yen / 10, one u8: up to 2550); shop = shops.toml id or name
NATIVE_KINDS = {
    1: ["var"],
    2: ["var", "var"],
    3: ["species", "u8"],
    4: [
        "trainerid",
        "var",
    ],  # 4 = battle: a trainers.toml id NAME or number (C8, contract 51: ids move when chapters merge)
    5: [],
    6: ["u8"],
    7: ["u8", "var"],
    8: ["var"],
    9: ["species", "u8"],
    10: ["var"],
    11: ["item", "u8"],
    12: ["shop"],
    13: ["item", "var"],
    14: ["item", "var"],
    15: ["money"],
    16: ["money", "var"],
    17: ["npc"],
    18: ["npc"],
    19: [],
    20: ["u8", "item"],
    21: ["u8"],
    22: [],
    23: ["u8", "u8"],
}
# C3-A (contract 32): the first operand of native 23 (`world`) is `operation << 4 | VAR` (natives.h C3A_*). Operand kinds "vn<op>" encode a var with that operation; "vq5" is the same for the
# daycare query (it fills VAR .. VAR + 2, so VAR <= 13); "textlo" / "texthi" are the low / high byte of a text id (the trade's nickname).
VN_KINDS = {f"vn{op}": (op, 15) for op in range(9)}
VN_KINDS["vn12"] = (12, 15)  # C4-A: caught_count
VN_KINDS["vn13"] = (13, 15)  # C4-A: elevator
VN_KINDS["vn15"] = (
    15,
    15,
)  # contract 65: `bond tier|heal VAR` (native 22 op 15, the casino plugin's CAS_BOND)
PLUS_KINDS = {
    "vp15": (15, 14, 1)
}  # contract 74: `bond ot SLOTVAR` (native 22 op 15): the slot is read from VAR, the answer written to VAR + 1 (like the link macros' second result)
VN_KINDS["vn14"] = (
    14,
    15,
)  # C9 (contract 50.3): the ending plugin (native 22 op 14; nucleo/ending_host.h EN_OP)
# C4-C (contract 37.3): `coins_buy VAR` = native pc 16+VAR, `slots VAR` = pc 32+VAR, `prizes SHOP VAR` = pc 48+VAR SHOP, `coins_get VAR` = pc 64+VAR (operation, operands, usage)
CASINO_MACROS = {
    "coins_buy": (1, 1, "coins_buy VAR"),
    "slots": (2, 1, "slots VAR"),
    "prizes": (3, 2, "prizes SHOP VAR"),
    "coins_get": (4, 1, "coins_get VAR"),
    "reserva_start": (5, 1, "reserva_start VAR"),
    "reserva_end": (6, 1, "reserva_end VAR"),
    "reserva_left": (7, 1, "reserva_left VAR"),
}  # 43.3
# IR LINK (contract 45): `link trade VAR`, `link battle VAR TRAINER`, `link selftest VAR` = native 22 `pc` with the link operations 9 / 10 / 11 (operation << 4 | VAR; the casino has 1..8, 5..8 being the Reserva's); a trade is
# followed by the evolution operation 12 on VAR + 1 (LK_OP_*, nucleo/link_host.h). VAR must be 0..14 (VAR + 1 receives the second result). Operand kinds "vlN" = (operation, highest VAR, offset added to the VAR).
LINK_KINDS = {
    "vl1": (9, 14, 0),
    "vl2": (10, 14, 0),
    "vl3": (11, 14, 0),
    "vl4": (12, 14, 1),
}
LINK_USAGE = "link trade VAR | link battle VAR TRAINER | link selftest VAR  (VAR 0..14; VAR + 1 gets the second result)"
# C9 (contract 50.3): `ending run VAR` (the Hall of Legacy -> the record -> the save at home -> the credits), `ending browse VAR` (the Hall of Legacy browser), `ending credits VAR` (the credits alone), `ending title`
# (back to the title; resident) = native 22 `pc` with the operation 14 << 4 | VAR and the sub-operation EN_* in the second operand (nucleo/ending_host.h). VAR = 0 could not open, 1 done. `pc` (the PC terminal) is
# the box screen followed by the browser when the PC menu asked for it (flag league_hof_req).
BOND_SUBOPS = {
    "tier": 0,
    "heal": 1,
    "ot": 2,
    "diploma": 3,
}  # contract 65 (nucleo/casino_host.h BOND_*): friendship; `bond tier VAR` = the maintainer's tier 0..4 (255 no creature), `bond heal VAR` = +1 friendship per creature the Centro is about to heal
CLOCK_SUBOPS = {
    "hour": 0,
    "night": 1,
    "cave": 2,
}  # contract 68 (nucleo/legend.h CLK_*): the Flipper's clock, read in the casino plugin; `clock hour VAR` = 0..23, `clock night VAR` = 20:00 .. 05:59, `clock cave VAR` = 05:00 .. 06:59 or 23:00 .. 00:59
ENDING_SUBOPS = {"run": 0, "browse": 1, "credits": 2}
ENDING_TITLE = 3
ENDING_USAGE = "ending run VAR | ending browse VAR | ending credits VAR | ending title"
ELEV_MAX_ROWS = 5  # the elevator list: 2..5 rows; World.tr[2k], tr[2k + 1] = the text id of row k, tr[2 * ELEV_MAX_ROWS] = the row count (natives.h C4A_*)
VN_KINDS["vq5"] = (5, 13)
MAX_VARS = 16


def ins_size(op):
    return 1 + sum(2 if k in WIDE_KINDS else 1 for k in OPS[op][1])


def parse_script_header(tok, loc, rep, m):
    if len(tok) < 2 or not re.match(r"^\w+$", tok[1]):
        rep.err(loc, "@script <name> [on_enter|interact|npc|step|called] [if [!]FLAG]")
        return None
    sc = Script(name=tok[1], loc=loc, map=m.name)
    if any(x.name == sc.name for x in m.scripts):
        rep.err(loc, f"duplicate script {sc.name}")
        return None
    k = 2
    while k < len(tok):
        t = tok[k].lower()
        if t == "enter":
            t = "on_enter"
        if t in SCRT_NAMES:
            sc.trigger = SCRT_NAMES[t]
        elif t == "if" and k + 1 < len(tok):
            k += 1
            f = tok[k]
            if f.startswith("!"):
                sc.cond_value, f = 0, f[1:]
            if not f:
                rep.err(loc, "if needs a flag")
            sc.cond = f
        else:
            rep.err(loc, f"unexpected {tok[k]!r} in @script header")
        k += 1
    if len(m.scripts) >= 255:
        rep.err(loc, "too many scripts (255 max)")
    m.scripts.append(sc)
    return sc


def parse_script_line(sc, line, tok, loc, rep):
    if len(tok) == 1 and tok[0].endswith(":"):
        lab = tok[0][:-1]
        if not re.match(r"^\w+$", lab):
            rep.err(loc, f"bad label {tok[0]!r}")
        elif lab in sc.labels:
            rep.err(loc, f"duplicate label {lab}")
        else:
            sc.labels[lab] = len(sc.ins)
        return
    mn, args = tok[0].lower(), tok[1:]
    if (
        mn == "link" and args and args[0].lower() in ("trade", "battle", "selftest")
    ):  # IR LINK (contract 45)
        what = args[0].lower()
        if what == "battle":
            if len(args) != 3:
                rep.err(loc, LINK_USAGE)
                return
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", args[1], args[2]],
                    ["u8", "vl2", "trainerid"],
                    loc,
                )
            )
        else:
            if len(args) != 2:
                rep.err(loc, LINK_USAGE)
                return
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", args[1], "0"],
                    ["u8", "vl1" if what == "trade" else "vl3", "u8"],
                    loc,
                )
            )
            if (
                what == "trade"
            ):  # then the evolution of the creature received (VAR + 1 = its slot, 255 = none)
                sc.ins.append(
                    Ins("call_native", ["22", args[1], "0"], ["u8", "vl4", "u8"], loc)
                )
        return
    if mn == "ending":  # C9 (contract 50.3)
        if args and args[0].lower() == "title" and len(args) == 1:
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", "0", str(ENDING_TITLE)],
                    ["u8", "vn14", "u8"],
                    loc,
                )
            )
        elif len(args) == 2 and args[0].lower() in ENDING_SUBOPS:
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", args[1], str(ENDING_SUBOPS[args[0].lower()])],
                    ["u8", "vn14", "u8"],
                    loc,
                )
            )
        else:
            rep.err(loc, ENDING_USAGE)
        return
    if (
        mn == "pc" and not args
    ):  # C9 (contract 50.4): the PC terminal = the box screen, then the Hall of Legacy browser if the PC menu's "Salon del Legado" asked for it (a flag the PC plugin sets as it closes)
        sc.ins.append(Ins("call_native", ["22", "0", "0"], ["u8", "u8", "u8"], loc))
        lab = f"_pc_hof_{len(sc.ins)}"
        sc.ins.append(Ins("jnf", ["league_hof_req", lab], ["flag", "label"], loc))
        sc.ins.append(Ins("clrf", ["league_hof_req"], ["flag"], loc))
        sc.ins.append(
            Ins(
                "call_native",
                ["22", "0", str(ENDING_SUBOPS["browse"])],
                ["u8", "vn14", "u8"],
                loc,
            )
        )
        sc.labels[lab] = len(sc.ins)
        return
    if mn in NATIVE_IDS:
        nid = NATIVE_IDS[mn]
        kinds = NATIVE_KINDS[nid]
        if len(args) != len(kinds):
            rep.err(loc, f"{mn} takes {len(kinds)} operand(s)")
            return
        sc.ins.append(
            Ins(
                "call_native",
                [str(nid)] + args + ["0"] * (2 - len(args)),
                ["u8"] + kinds + ["u8"] * (2 - len(kinds)),
                loc,
            )
        )
        return
    if (
        mn == "wild"
    ):  # wild SPECIES LEVEL VAR = wild_set SPECIES LEVEL + wild_go VAR (contract 8.4)
        if len(args) != 3:
            rep.err(loc, "wild takes 3 operands: SPECIES LEVEL VAR")
            return
        lv = parse_int(args[1])
        if lv is None or not 1 <= lv <= 100:
            rep.err(loc, f"wild level must be 1..100, got {args[1]!r}")
            return
        sc.ins.append(
            Ins("call_native", ["9", args[0], args[1]], ["u8", "species", "u8"], loc)
        )
        sc.ins.append(
            Ins("call_native", ["10", args[2], "0"], ["u8", "var", "u8"], loc)
        )
        return
    if (
        mn == "respawn"
    ):  # respawn [MAP] SPAWN = setv respawn_map <map id> + setv respawn_spawn <spawn id> (the blackout goes there, 18.3)
        if len(args) not in (1, 2):
            rep.err(loc, "respawn [MAP] SPAWN")
            return
        tm, spn = (args[0], args[1]) if len(args) == 2 else (sc.map, args[0])
        sc.ins.append(Ins("setv", ["respawn_map", tm], ["var", "mapid"], loc))
        sc.ins.append(
            Ins("setv", ["respawn_spawn", f"{tm}:{spn}"], ["var", "spawnid"], loc)
        )
        return
    # C3-A (contract 32) macros over native 23 `world` (operation << 4 | VAR in the first operand, natives.h C3A_*)
    # C4-A (contract 35.5): caught_count VAR (op 12), elevator VAR KEY1..KEY5 (stores the labels in World.tr, op 13 opens the list)
    if mn == "caught_count":
        if len(args) != 1:
            rep.err(loc, "caught_count VAR")
            return
        sc.ins.append(
            Ins("call_native", ["23", args[0], "0"], ["u8", "vn12", "u8"], loc)
        )
        return
    if (
        mn == "caught_ge"
    ):  # caught_ge VAR N: VAR = 1 when at least N species are caught, else 0 (the VM has no `greater than`)
        n = parse_int(args[1]) if len(args) == 2 else None
        if len(args) == 2 and args[1] in (
            "full",
            "full-1",
        ):  # C9 (contract 56): `full` = every species of species.toml but the mythical gift (Genesa), the Registro's complete mark; follows the species table when chapters merge; contract 79: `full-1` = one less (at most one missing)
            n = len(species_ids()) - 1 - (args[1] == "full-1")
        if n is None or not 1 <= n <= 255:
            rep.err(loc, "caught_ge VAR N|full|full-1 (N = 1..255)")
            return
        sc.ins.append(
            Ins("call_native", ["23", args[0], str(n)], ["u8", "vn12", "u8"], loc)
        )
        return
    if (
        mn == "clock"
    ):  # contract 68: clock hour|night|cave VAR = native pc 208+VAR (op 13, the casino plugin's CAS_LEGEND), sub-operation 0 / 1 / 2: VAR = the hour 0..23 / 1 when it is night (20:00 .. 05:59) / 1 in the Gruta del Eclipse's windows (05:00 .. 06:59, 23:00 .. 00:59)
        if len(args) != 2 or args[0] not in CLOCK_SUBOPS:
            rep.err(loc, "clock hour|night|cave VAR")
            return
        sc.ins.append(
            Ins(
                "call_native",
                ["22", args[1], str(CLOCK_SUBOPS[args[0]])],
                ["u8", "vn13", "u8"],
                loc,
            )
        )
        return
    if (
        mn == "bond"
    ):  # contract 65: bond tier VAR | bond heal VAR = native pc 240+VAR, sub-operation 0 / 1 (the casino plugin: friendship); contract 74: bond ot VAR (VAR = a party slot -> VAR + 1 = 1 own / 0 traded / 255 none), bond diploma
        if args[:1] == ["diploma"] and len(args) == 1:
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", "0", str(BOND_SUBOPS["diploma"])],
                    ["u8", "vn15", "u8"],
                    loc,
                )
            )
            return
        if len(args) != 2 or args[0] not in BOND_SUBOPS or args[0] == "diploma":
            rep.err(
                loc,
                "bond tier VAR | bond heal VAR | bond ot SLOTVAR (SLOTVAR 0..14; SLOTVAR + 1 gets the answer) | bond diploma",
            )
            return
        sc.ins.append(
            Ins(
                "call_native",
                ["22", args[1], str(BOND_SUBOPS[args[0]])],
                ["u8", "vp15" if args[0] == "ot" else "vn15", "u8"],
                loc,
            )
        )
        return
    # C4-C (contract 37.3 / 38.7): the casino operations of native 22 `pc` (first operand = operation << 4 | VAR, the same encoding as native 23's "vnN" kinds; natives_shop.c)
    if mn in CASINO_MACROS:
        op, nargs, usage = CASINO_MACROS[mn]
        if len(args) != nargs:
            rep.err(loc, usage)
            return
        if (
            mn == "prizes"
        ):  # prizes SHOP VAR: SHOP = a `prizes`-table shop of assets/data/shops.toml (its id name or number)
            sc.ins.append(
                Ins(
                    "call_native",
                    ["22", args[1], args[0]],
                    ["u8", f"vn{op}", "shop"],
                    loc,
                )
            )
        else:
            sc.ins.append(
                Ins("call_native", ["22", args[0], "0"], ["u8", f"vn{op}", "u8"], loc)
            )
        return
    if mn == "elevator":
        if not 3 <= len(args) <= 1 + ELEV_MAX_ROWS:
            rep.err(
                loc,
                f"elevator VAR KEY1 .. KEY{ELEV_MAX_ROWS}: the result variable and 2..{ELEV_MAX_ROWS} floor labels (text keys)",
            )
            return
        for k, key in enumerate(args[1:]):
            sc.ins.append(
                Ins(
                    "call_native",
                    ["23", str(32 + 2 * k), key],
                    ["u8", "u8", "textlo"],
                    loc,
                )
            )
            sc.ins.append(
                Ins(
                    "call_native",
                    ["23", str(33 + 2 * k), key],
                    ["u8", "u8", "texthi"],
                    loc,
                )
            )
        sc.ins.append(
            Ins(
                "call_native",
                ["23", str(32 + 2 * ELEV_MAX_ROWS), str(len(args) - 1)],
                ["u8", "u8", "u8"],
                loc,
            )
        )
        sc.ins.append(
            Ins("call_native", ["23", args[0], "0"], ["u8", "vn13", "u8"], loc)
        )
        return
    C3A_MACROS = {
        "pick_party": 1,
        "daycare_deposit": 1,
        "daycare_withdraw": 1,
        "daycare_query": 1,
        "rand": 2,
        "copyv": 2,
        "addvv": 2,
        "sfx": 1,
        "music": 1,
        "music_map": 0,
        "trade": 4,
    }
    if mn in C3A_MACROS:
        usage = {
            "pick_party": "pick_party VAR",
            "daycare_deposit": "daycare_deposit VAR",
            "daycare_withdraw": "daycare_withdraw VAR",
            "daycare_query": "daycare_query VAR",
            "rand": "rand VAR N",
            "copyv": "copyv DST SRC",
            "addvv": "addvv DST SRC",
            "sfx": "sfx NAME|N",
            "music": "music NAME|N",
            "music_map": "music_map",
            "trade": "trade WANT_SPECIES GIVE_SPECIES NICKNAME_TEXT_KEY VAR",
        }
        if len(args) != C3A_MACROS[mn]:
            rep.err(loc, usage[mn])
            return
        nat = lambda a, b, ka, kb: sc.ins.append(
            Ins("call_native", ["23", a, b], ["u8", ka, kb], loc)
        )
        if mn == "pick_party":  # VAR = the chosen slot, 255 = cancelled
            nat(args[0], "0", "vn0", "u8")
        elif (
            mn == "daycare_deposit"
        ):  # the picker, then the deposit of the chosen slot: VAR = 1 done, 0 refused (care full / the last creature), 255 cancelled
            nat(args[0], "0", "vn0", "u8")
            nat(args[0], "0", "vn3", "u8")
        elif (
            mn == "daycare_withdraw"
        ):  # VAR = 1 done (the fee is charged), 0 the party is full / nothing in care, 2 not enough money
            nat(args[0], "0", "vn4", "u8")
        elif (
            mn == "daycare_query"
        ):  # VAR, VAR+1, VAR+2 = occupied, species, levels gained; {v1} = the fee, {v2} = the levels (the script: name_var 0 VAR+1)
            nat(args[0], "0", "vq5", "u8")
        elif mn == "rand":
            nat(args[0], args[1], "vn6", "u8")
        elif mn == "copyv":
            nat(args[0], args[1], "vn7", "var")
        elif mn == "addvv":
            nat(args[0], args[1], "vn8", "var")
        elif (
            mn == "music"
        ):  # a music track by its `music` name of assets/music/index.txt (or its id): it replaces what plays (the rival's theme, a jingle)
            nat("160", args[0], "u8", "music")
        elif mn == "music_map":  # the loaded map's own track again
            nat("176", "0", "u8", "u8")
        elif (
            mn == "sfx"
        ):  # a sound effect by its `fx` name of assets/music/index.txt (or its number, 0 = the first effect)
            nat("144", args[0], "u8", "sfx")
        else:  # trade WANT GIVE NICK VAR: four argument stores, then the trade picker (VAR: 1 traded, 0 wrong species, 255 cancelled)
            nat("32", args[0], "u8", "species")
            nat("33", args[1], "u8", "species")
            nat("34", args[2], "u8", "textlo")
            nat("35", args[2], "u8", "texthi")
            nat(args[3], "0", "vn1", "u8")
        return
    if (
        mn == "give_box"
    ):  # C6 (contract 46.4): give SPECIES LEVEL, but a full party sends the creature to the PC box (var 0: 1 party, 2 box, 0 refused)
        lv = parse_int(args[1]) if len(args) == 2 else None
        if lv is None or not 1 <= lv <= 100:
            rep.err(loc, "give_box SPECIES LEVEL (1..100)")
            return
        sc.ins.append(
            Ins(
                "call_native",
                ["3", args[0], str(lv + 128)],
                ["u8", "species", "u8"],
                loc,
            )
        )
        return
    if (
        mn == "setv_sp"
    ):  # C7 (contract 53): setv VAR <species id> BY NAME (the species ids move when chapters merge): `setv_sp sp sealily` + `name_var 0 sp` shows the name, `mon_show sp` the picture
        if len(args) != 2:
            rep.err(loc, "setv_sp VAR SPECIES")
            return
        sc.ins.append(Ins("setv", [args[0], args[1]], ["var", "species"], loc))
        return
    if mn == "native":
        if not 1 <= len(args) <= 3:
            rep.err(loc, "native <id|name> [a] [b]")
            return
        nid = NATIVE_IDS.get(args[0].lower(), args[0])
        sc.ins.append(
            Ins(
                "call_native",
                [str(nid)] + args[1:] + ["0"] * (3 - len(args)),
                ["u8", "u8", "u8"],
                loc,
            )
        )
        return
    if mn not in OPS:
        rep.err(loc, f"unknown opcode {tok[0]!r}")
        return
    kinds = list(OPS[mn][1])
    if mn == "warp" and len(args) == 2 and args[1].startswith("spawn="):
        sc.ins.append(Ins("warp", [args[0], args[1][6:]], ["map", "spawn"], loc))
        return
    if len(args) != len(kinds):
        rep.err(
            loc,
            f"{mn} takes {len(kinds)} operand(s) ({' '.join(kinds)}), got {len(args)}",
        )
        return
    sc.ins.append(Ins(mn, args, kinds, loc))


def finish_scripts(m, rep):
    """After build_map: the text keys the scripts say/ask belong to the map's text table."""
    for sc in m.scripts:
        if not sc.ins:
            rep.err(sc.loc, f"script {sc.name} is empty")
        for ins in sc.ins:
            for kind, a in zip(ins.kinds, ins.args):
                if kind in ("text", "textlo", "texthi") and a not in m.text_keys:
                    m.text_keys.append(a)


class ScriptCtx:
    """Name resolution for one map's scripts (flags / vars are global, npcs / scripts per map)."""

    def __init__(self, world, m):
        self.world, self.m = world, m
        self.flags = dict(getattr(world, "flag_defs", None) or m.flag_defs)
        self.vars = dict(getattr(world, "var_defs", None) or m.var_defs)

    def num(self, tok, lo, hi, what):
        if tok.startswith(
            "sp:"
        ):  # C9 (contract 56): `setv 0 sp:genesa` = the species id of that name, so a script survives the species ids moving when chapters merge
            return parse_species(tok[3:])
        v = parse_int(tok)
        if v is None or not lo <= v <= hi:
            raise ValueError(f"{what} must be {lo}..{hi}, got {tok!r}")
        return v

    def flag(self, tok):
        if tok in self.flags:
            return self.flags[tok]
        if parse_int(tok) is None:
            raise ValueError(f"unknown flag {tok!r} (number 1..1023 or a @flag name)")
        return self.num(tok, 1, MAX_FLAG, "flag")

    def var(self, tok):
        if tok in self.vars:
            return self.vars[tok]
        if parse_int(tok) is None:
            raise ValueError(f"unknown var {tok!r} (number 0..15 or a @var name)")
        return self.num(tok, 0, MAX_VARS - 1, "var")

    def npc(self, tok):
        npcs = self.m.npcs
        if re.match(r"^#\d+$", tok):
            k = int(tok[1:])
            if k >= len(npcs):
                raise ValueError(f"npc {tok} does not exist ({len(npcs)} npcs)")
            return k
        for pred in (
            lambda n: n.ident == tok,
            lambda n: n.ch == tok,
            lambda n: n.name == tok,
        ):
            hit = [k for k, n in enumerate(npcs) if pred(n)]
            if len(hit) == 1:
                return hit[0]
            if len(hit) > 1:
                raise ValueError(
                    f"npc {tok!r} is ambiguous ({len(hit)} match): give them an id=<name> "
                    f"in the legend or use #index"
                )
        raise ValueError(
            f"unknown npc {tok!r} (legend char, id=, actor name or #index)"
        )

    def script(self, tok):
        for k, sc in enumerate(self.m.scripts):
            if sc.name == tok:
                return k
        raise ValueError(f"unknown script {tok!r}")

    def dirn(self, tok):
        f = parse_facing(tok)
        return f if f is not None else self.num(tok, 0, 7, "dir")

    def map(self, tok):
        t = self.world.by_name(tok) if self.world else None
        if t is None:
            raise ValueError(f"unknown map {tok!r}")
        return t.id


def assemble_scripts(world, m, rep, plan=None):
    """-> (table bytes, code bytes) of m's scripts (None when it has none). Errors go to rep.
    plan = TextPlan for real text ids (None: ids 0, only for validation)."""
    if not m.scripts:
        return None
    ctx = ScriptCtx(world, m)
    table, code = bytearray(), bytearray()
    for sc in m.scripts:
        where = f"{m.name}:{sc.name}"
        base = len(code)
        addr, addrs = 0, []
        for ins in sc.ins:
            addrs.append(addr)
            addr += ins_size(ins.op)
        total = addr
        for lab, idx in sc.labels.items():
            if idx > len(sc.ins):
                rep.err(where, f"label {lab} out of range")
        flag_cond = 0
        if sc.cond:
            try:
                flag_cond = ctx.flag(sc.cond)
            except ValueError as e:
                rep.err(sc.loc, str(e))
        for n, ins in enumerate(sc.ins):
            try:
                vals = []
                if ins.op == "warp" and ins.kinds == ["map", "spawn"]:
                    tm = world.by_name(ins.args[0]) if world else None
                    if tm is None:
                        raise ValueError(f"unknown map {ins.args[0]!r}")
                    spn = tm.spawn(ins.args[1])
                    if spn is None:
                        raise ValueError(f"map {tm.name} has no spawn {ins.args[1]!r}")
                    vals = [tm.id, spn.i, spn.j, spn.facing]
                else:
                    for kind, tok in zip(ins.kinds, ins.args):
                        if kind == "u8":
                            vals.append(ctx.num(tok, 0, 255, "operand"))
                        elif kind == "species":
                            vals.append(parse_species(tok))
                        elif kind == "item":
                            vals.append(parse_item(tok))
                        elif kind == "shop":
                            vals.append(parse_shop(tok))
                        elif kind == "money":
                            y = parse_int(tok)
                            if y is None or y % 10 or not 0 <= y <= 2550:
                                raise ValueError(
                                    f"money must be a multiple of 10 yen, 0..2550 (the operand is yen / 10), got {tok!r}"
                                )
                            vals.append(y // 10)
                        elif kind == "mapid":
                            mid = ctx.map(tok)
                            if mid == 0:
                                raise ValueError(
                                    "map id 0 cannot be a respawn point (var respawn_map 0 means 'home')"
                                )
                            vals.append(mid)
                        elif kind == "spawnid":
                            tmn, _, spn = tok.rpartition(":")
                            tm2 = world.by_name(tmn) if world else None
                            sp2 = tm2.spawn(spn) if tm2 else None
                            if sp2 is None:
                                raise ValueError(f"unknown spawn {tok!r} (map:spawn)")
                            vals.append(sp2.id)
                        elif kind == "steps":
                            vals.append(ctx.num(tok, 1, 255, "steps"))
                        elif kind == "ms":
                            vals.append(ctx.num(tok.removesuffix("ms"), 0, 65535, "ms"))
                        elif kind == "dir":
                            vals.append(ctx.dirn(tok))
                        elif kind == "flag":
                            vals.append(ctx.flag(tok))
                        elif kind == "var":
                            vals.append(ctx.var(tok))
                        elif kind == "npc":
                            vals.append(ctx.npc(tok))
                        elif kind == "script":
                            vals.append(ctx.script(tok))
                        elif kind == "map":
                            vals.append(ctx.map(tok))
                        elif kind in VN_KINDS:
                            op, hi = VN_KINDS[kind]
                            v = ctx.var(tok)
                            if v > hi:
                                raise ValueError(f"var {tok!r} must be 0..{hi} here")
                            vals.append(v | op << 4)
                        elif kind in LINK_KINDS or kind in PLUS_KINDS:
                            op, hi, plus = (
                                LINK_KINDS[kind]
                                if kind in LINK_KINDS
                                else PLUS_KINDS[kind]
                            )
                            v = ctx.var(tok)
                            if v > hi:
                                raise ValueError(
                                    f"var {tok!r} must be 0..{hi} here (VAR + 1 receives the second result)"
                                )
                            vals.append((v + plus) | op << 4)
                        elif kind == "trainerid":
                            vals.append(parse_trainer(tok))
                        elif kind == "sfx":
                            vals.append(parse_sfx(tok))
                        elif kind == "music":
                            vals.append(parse_sfx(tok, "music"))
                        elif kind in ("textlo", "texthi"):
                            if plan is None:
                                vals.append(0)
                            else:
                                if tok not in plan.gl and tok not in plan.local.get(
                                    m.name, []
                                ):
                                    raise ValueError(
                                        f"text key {tok!r} not in the text plan"
                                    )
                                tid = plan.id_of(m, tok)
                                vals.append(tid & 255 if kind == "textlo" else tid >> 8)
                        elif kind == "text":
                            if plan is None:
                                vals.append(0)
                            else:
                                if tok not in plan.gl and tok not in plan.local.get(
                                    m.name, []
                                ):
                                    raise ValueError(
                                        f"text key {tok!r} not in the text plan"
                                    )
                                vals.append(plan.id_of(m, tok))
                        elif kind == "label":
                            if tok not in sc.labels:
                                raise ValueError(f"unknown label {tok!r}")
                            tgt = (
                                addrs[sc.labels[tok]]
                                if sc.labels[tok] < len(sc.ins)
                                else total
                            )
                            vals.append(tgt - (addrs[n] + ins_size(ins.op)))
                        else:
                            raise ValueError(f"internal: kind {kind}")
                code += encode_ins(ins.op, vals)
            except ValueError as e:
                rep.err(ins.loc, str(e))
                code += bytes(ins_size(ins.op))
        if len(code) - base != total:
            raise AssertionError("script size mismatch")
        if len(code) > 0xFFFF:
            rep.err(where, "script code exceeds 65535 bytes")
        table += pack_script_def(
            base & 0xFFFF, total, sc.trigger, flag_cond, sc.cond_value if sc.cond else 0
        )
    return bytes(table), bytes(code)


def npc_flags_byte(n):
    """NpcDef.flags on disk: bit0 solid, bit1 script, bits 2..7 = bits 8..9 of the show / hide / talk flag ids (contract 40.2)."""
    return (
        n.flags
        | (n.show_flag >> 8) << 2
        | (n.hide_flag >> 8) << 4
        | (n.talk_flag >> 8) << 6
    )


def pack_script_def(off, length, trigger, cond, cond_value):
    """ScriptDef (8 B): the condition flag's bits 8..9 are the last byte (cond_hi; the old `reserved`, contract 40.2)."""
    return struct.pack(
        "<HHBBBB", off, length, trigger, cond & 255, cond_value, cond >> 8
    )


def encode_ins(op, vals):
    out = bytearray([OPS[op][0]])
    if (
        op in FLAG_OPS
    ):  # flag id bits 8..9 ride in bits 6..7 of the opcode byte (contract 40.3)
        out[0] |= (vals[0] >> 8) << 6
        vals = [vals[0] & 255] + list(vals[1:])
    for kind, v in zip(OPS[op][1], vals):
        out += (
            struct.pack("<h", v)
            if kind == "label"
            else (struct.pack("<H", v) if kind in WIDE_KINDS else bytes([v]))
        )
    return bytes(out)


def decode_script(code):
    """Bytes of one script -> [(addr, op name, [raw operand values]), ...]; label values are signed
    rel16. Raises ValueError on a bad opcode or a truncated instruction."""
    out, a = [], 0
    while a < len(code):
        name = OP_BY_CODE.get(code[a] & 0x3F)
        if name is None or (code[a] & 0xC0 and name not in FLAG_OPS):
            raise ValueError(f"bad opcode {code[a]:#04x} at {a}")
        size = ins_size(name)
        if a + size > len(code):
            raise ValueError(f"truncated {name} at {a}")
        vals, k = [], a + 1
        for kind in OPS[name][1]:
            if kind == "label":
                vals.append(struct.unpack_from("<h", code, k)[0])
                k += 2
            elif kind in WIDE_KINDS:
                vals.append(struct.unpack_from("<H", code, k)[0])
                k += 2
            else:
                vals.append(code[k])
                k += 1
        if name in FLAG_OPS:
            vals[0] |= (code[a] >> 6) << 8
        out.append((a, name, vals))
        a += size
    return out


def encode_script(ins_list):
    return b"".join(encode_ins(name, vals) for _a, name, vals in ins_list)


def check_script_names(world):
    """Aggregates the @flag / @var names of every map (they are global) into world.flag_defs /
    world.var_defs and reports clashes."""
    rep = world.report
    for attr, what in (("flag_defs", "flag"), ("var_defs", "var")):
        agg = dict(BUILTIN_FLAGS if what == "flag" else BUILTIN_VARS)
        byid = {v: k for k, v in BUILTIN_FLAGS.items()} if what == "flag" else {}
        for m in world.maps:
            for nm, v in getattr(m, attr).items():
                if nm in agg and agg[nm] != v:
                    rep.err(
                        m.name, f"@{what} {nm} = {v} but another map says {agg[nm]}"
                    )
                elif (
                    what == "var"
                    and nm not in BUILTIN_VARS
                    and v in BUILTIN_VARS.values()
                ):
                    rep.err(
                        m.name,
                        f"@var {nm} = {v}: that number belongs to the engine variable {[k for k, x in BUILTIN_VARS.items() if x == v][0]}",
                    )
                elif v in byid and byid[v] != nm and what == "flag":
                    rep.err(m.name, f"@{what} {nm} and {byid[v]} both use number {v}")
                agg[nm] = v
                byid.setdefault(v, nm)
        setattr(world, attr, agg)


# ---- binary emitter (contract 3.1 / 3.2) --------------------------------------------------------
import struct  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))

NO_TEXT = 0xFFFF
GLOBAL_BIT = 0x8000
HEADER_SIZE = 64
MAP_SIZE_WARN = 16 * 1024


def _ident(s):
    return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").upper()


class TextPlan:
    """Text id assignment. local[map] = ordered non-global keys of the map (id = index);
    gl = ordered global keys (id = 0x8000 | index, order of assets/text/es.txt)."""

    def __init__(self):
        self.local, self.gl, self.errors = {}, [], []

    def id_of(self, m, key):
        if not key:
            return NO_TEXT
        if key.startswith("gl."):
            return GLOBAL_BIT | self.gl.index(key)
        return self.local[m.name].index(key)


def _pair_key(k):
    return k[:-1] + "b" if k.endswith(".a") else None


def text_plan(world, texts=None):
    """Assign text ids. Talk-flag NPCs: their .a key is immediately followed by the .b key."""
    import textc

    plan = TextPlan()
    if texts is None:
        texts = textc.load()
    gl_keys = [k for k in texts if textc.is_global(k)]
    plan.gl = gl_keys
    pairs = set()
    for m in world.maps:
        for n in m.npcs:
            if n.talk_flag and n.text_key2:
                pairs.add((n.text_key, n.text_key2))
    for a, b in sorted(pairs):
        if textc.is_global(a):
            if (
                a not in gl_keys
                or b not in gl_keys
                or gl_keys.index(b) != gl_keys.index(a) + 1
            ):
                plan.errors.append(
                    f"global keys [{a}] and [{b}] must be adjacent (a then b) in es.txt"
                )
    for m in world.maps:
        lst = []
        for a, b in sorted(pairs):
            if not any(
                n.text_key == a and n.text_key2 == b for n in m.npcs
            ) or textc.is_global(a):
                continue
            if a in lst:
                if lst[lst.index(a) + 1 : lst.index(a) + 2] != [b]:
                    plan.errors.append(
                        f"{m.name}: [{a}] is not directly followed by [{b}]"
                    )
            elif b in lst:
                plan.errors.append(f"{m.name}: [{b}] placed before its pair [{a}]")
            else:
                lst += [a, b]
        for k in m.text_keys:
            if not textc.is_global(k) and k not in lst:
                lst.append(k)
        for k in lst:
            if k not in texts:
                plan.errors.append(f"{m.name}: text key [{k}] missing in es.txt")
        for k in m.text_keys:
            if textc.is_global(k) and k not in texts:
                plan.errors.append(f"{m.name}: global text key [{k}] missing in es.txt")
        if len(lst) > 0x7FFF:
            plan.errors.append(f"{m.name}: too many text keys")
        plan.local[m.name] = lst
    return plan


def get_plan(world):
    if getattr(world, "plan", None) is None:
        world.plan = text_plan(world)
    return world.plan


def flag_names(world):
    """{flag id: C identifier} for talk_flag / SET_FLAG interacts / show+hide flags (first name wins)."""
    names = {}
    for nm, fid in (getattr(world, "flag_defs", None) or {}).items():
        names.setdefault(fid, "FLAG_" + _ident(nm))
    for m in world.maps:
        for n in m.npcs:
            if n.talk_flag and n.gen != "trainer":
                names.setdefault(n.talk_flag, "FLAG_TALKED_" + _ident(n.name))
        for it in m.interacts:
            if it.kind == IK_FLAG and it.flag:
                names.setdefault(it.flag, f"FLAG_{_ident(m.name)}_SET_{it.i}_{it.j}")
        for n in m.npcs:
            for f in (n.show_flag, n.hide_flag) + (
                (n.talk_flag,) if n.gen == "trainer" else ()
            ):
                if f:
                    names.setdefault(f, f"FLAG_USER_{f}")
    ctx = {}
    for m in world.maps:  # numeric flags used by scripts
        ctx[m.name] = ScriptCtx(world, m)
        for sc in m.scripts:
            toks = [sc.cond] if sc.cond else []
            toks += [a for i in sc.ins for k, a in zip(i.kinds, i.args) if k == "flag"]
            for t in toks:
                try:
                    f = ctx[m.name].flag(t)
                except ValueError:
                    continue
                names.setdefault(f, f"FLAG_USER_{f}")
    return dict(sorted(names.items()))


def _pad4(b):
    b.extend(b"\0" * ((-len(b)) % 4))


def extras_bytes(m):
    """The 'extras' section: u8 enc_rate, u8 n_enc, u8 n_ledges, u8 has_tail (bit 0 the C5 tail, bits 1..7 the roam block's length in 8-byte units), u8 open_rate (contract 63); n_enc x (species, lv_min, lv_max, weight);
    n_ledges x (i, j, jump facing); contract 55, has_tail >> 1 = length / 8: the roam area mask (ceil(w * h / 8) bytes) + n_enc bytes (the bank index of each slot's roam actor), padded to 8; C5 (contract 42.1) when has_tail & 1: u8 rate[4], u8 n[4] (water, fish_old, fish_good, fish_super) and the slots of those tables.
    """
    roam = m.roam_mask + bytes(m.roam_spr) if m.roam else b""
    roam += b"\0" * (-len(roam) % 8)
    assert len(roam) // 8 < 128
    out = bytearray(
        [
            m.enc_rate,
            len(m.enc),
            len(m.ledges),
            (1 if m.tables else 0) | (len(roam) // 8) << 1,
            m.open_rate,
        ]
    )
    for sid, lo, hi, wt, _nm in m.enc:
        out += bytes([sid, lo, hi, wt])
    for i, j, d in m.ledges:
        out += bytes([i, j, d])
    out += roam  # contract 55 (bits 1..7 of has_tail = its length / 8): between the ledges and the C5 tail: the roam area mask (ceil(w * h / 8) bytes), the bank index of each grass slot's roam actor (n_enc bytes), zero padding to 8
    if m.tables:
        tb = [m.tables.get(k, (0, [])) for k in WILD_TABLES]
        out += bytes([t[0] for t in tb]) + bytes([len(t[1]) for t in tb])
        for t in tb:
            for sid, lo, hi, wt, _nm in t[1]:
                out += bytes([sid, lo, hi, wt])
    return bytes(out)


def bank_animated(bank):
    """True when a TILE / OBJECT of the bank bytes has nframes > 1 (MAPF_ANIM, contract 22)."""
    return any(
        bank[8 + 16 * k] < 2 and bank[8 + 16 * k + 4] > 1 for k in range(bank[1])
    )


def map_flags(m, bank=b"\0\0"):
    return (
        (MAPF_INTERIOR if m.interior else 0)
        | (MAPF_BALLOON if m.balloon else 0)
        | (MAPF_ANIM if bank_animated(bank) else 0)
        | (MAPF_CAVE if m.cave else 0)
        | (MAPF_DARK if m.dark else 0)
        | (MAPF_GHOSTS if m.ghosts else 0)
        | (MAPF_FOG if m.fog else 0)
        | (MAPF_PITS if any(n.behavior == NPCB_HOLE for n in m.npcs) else 0)
    )


def emit_v1(m, world, bank=None, plan=None):
    """Serialize one Map into map_NN.bin bytes (contract 3.1). `bank` = bytes of the sprite bank
    for m.bank (default: built via build_map_bank)."""
    plan = plan or get_plan(world)
    if bank is None:
        bank = build_map_bank(world, m, getattr(world, "placeholder", False))[0]
    tid = lambda k: plan.id_of(m, k)
    script_id = lambda mm, name: next(
        k for k, x in enumerate(mm.scripts) if x.name == name
    )
    for what, n, lim in (
        ("objects", len(m.objects), 0xFFFF),
        ("npcs", len(m.npcs), 16),  # 16 = WORLD_MAX_NPCS (nucleo/world.h)
        ("warps", len(m.warps), 255),
        ("interacts", len(m.interacts), 255),
        ("spawns", len(m.spawns), 255),
    ):
        if n > lim:
            raise ValueError(f"{m.name}: too many {what} ({n} > {lim})")
    out = bytearray(HEADER_SIZE)
    offs = {}

    def section(name, data):
        _pad4(out)
        offs[name] = len(out)
        out.extend(data)

    section("ground", bytes(m.ground))
    section("cflags", bytes(m.cflags))
    section(
        "objs",
        b"".join(struct.pack("<4B", o.spr, o.i0, o.j0, o.flags) for o in m.objects),
    )
    section(
        "npcs",
        b"".join(
            struct.pack(
                "<6BH4B",
                n.spr,
                n.i,
                n.j,
                n.facing,
                n.behavior,
                npc_flags_byte(n),
                (
                    script_id(m, n.script)
                    if n.script
                    else (tid(n.text_key) if n.text_key else NO_TEXT)
                ),
                n.show_flag & 255,
                n.hide_flag & 255,
                n.param,
                n.talk_flag & 255,
            )
            for n in m.npcs
        ),
    )
    section(
        "warps",
        b"".join(
            struct.pack(
                "<8B",
                w.i,
                w.j,
                w.target_map_id,
                w.target_i,
                w.target_j,
                w.target_facing,
                w.flags,
                w.req_dirs,
            )
            for w in m.warps
        ),
    )
    section(
        "interacts",
        b"".join(
            struct.pack(
                "<5BHB",
                it.i,
                it.j,
                it.dirmask,
                it.mode,
                it.kind | (it.flag >> 8) << 6,
                script_id(m, it.script) if it.script else tid(it.text_key),
                it.flag & 255,
            )
            for it in m.interacts
        ),
    )
    section(
        "spawns",
        b"".join(struct.pack("<4B", s.id, s.i, s.j, s.facing) for s in m.spawns),
    )
    if (
        m.enc_rate or m.open_rate or m.ledges or m.tables or m.roam
    ):  # extras (contract 13): encounter table + one-way ledges (+ the C5 tail), before the scripts
        section("extras", extras_bytes(m))
    sc_bin = assemble_scripts(world, m, world.report, plan)
    if world.report.errors:
        raise ValueError(f"{m.name}: script errors: {world.report.errors[0]}")
    if sc_bin:
        if len(m.scripts) > 255:
            raise ValueError(f"{m.name}: too many scripts")
        section("scripts", sc_bin[0] + sc_bin[1])
    section("bank", bank)
    _pad4(out)
    # v2 (actor streaming, contract 3.1): u16 at 18 = bytes of the RESIDENT part of the bank / 4 (the bank header's `size`); the actor pixel data after it
    # stays on the card. The rest of the header is unchanged.
    bank_res = struct.unpack_from("<I", bank, 4)[0]
    assert bank_res % 4 == 0 and bank_res // 4 < 0x10000 and bank_res <= len(bank)
    hdr = struct.pack(
        "<4sBBBBBBBBHBBBBH",
        b"NMAP",
        MAP_VERSION,
        m.id,
        m.w,
        m.h,
        m.bg | (4 if m.nofollow else 0),
        map_flags(m, bank),
        m.area,
        (m.fly[0] + 1) if m.fly else 0,
        len(m.objects),
        len(m.npcs),
        len(m.warps),
        len(m.interacts),
        len(m.spawns),
        bank_res // 4,
    )
    hdr += struct.pack(
        "<8I",
        *(
            offs[k]
            for k in (
                "ground",
                "cflags",
                "objs",
                "npcs",
                "warps",
                "interacts",
                "spawns",
                "bank",
            )
        ),
    )
    assert offs.get("extras", 0) // 4 < 0x10000
    hdr += struct.pack("<I", len(out)) + struct.pack(
        "<IHH", offs.get("scripts", 0), len(m.scripts), offs.get("extras", 0) // 4
    )
    assert len(hdr) == HEADER_SIZE, len(hdr)
    out[:HEADER_SIZE] = hdr
    return bytes(out)


def emit_text(world, m, texts=None, lang=None):
    """txt_NN.bin of map m for `lang` (default: the master language)."""
    import textc, mkfont

    lang = lang or mkfont.get_lang()
    texts = texts or textc.load(lang.text_path)
    return textc.build_table(texts, get_plan(world).local[m.name], lang)


def emit_text_gl(world, texts=None, lang=None):
    """txt_gl.bin for `lang` (default: the master language)."""
    import textc, mkfont

    lang = lang or mkfont.get_lang()
    texts = texts or textc.load(lang.text_path)
    return textc.build_table(texts, get_plan(world).gl, lang)


def emit_header_h(world):
    """nucleo/gen/maps_gen.h text."""
    plan = get_plan(world)
    L = ["/* Generated by tools/mapc.py build - do not edit. */", "#pragma once", ""]
    L.append("/* map ids */")
    for m in world.maps:
        L.append(f"#define MAP_{_ident(m.name)} {m.id}")
    real = [
        m.id for m in world.maps if m.id < TEST_MAP_BASE
    ]  # ids >= 200 are engine test fixtures
    L.append(f"#define MAP_COUNT {max(real) + 1 if real else 0}")
    sm, ss = world.start
    L += [
        "",
        "/* zone-name banner areas (@area, header area_id; text gl.area.<key>, contract 22) */",
    ]
    for k, key in enumerate(AREA_KEYS):
        L.append(f"#define AREA_{_ident(key)} {k + 1}")
    L.append(f"#define AREA_COUNT {len(AREA_KEYS)}")
    L.append(
        f"#define TXT_GL_AREA_LAST TXT_GL_AREA_{_ident(AREA_KEYS[-1])}   /* C9 (contract 56): the last zone banner text; world.c asserts the gl.area.* texts are consecutive up to it (every chapter that adds an area key moves it) */"
    )
    L += [
        "",
        "/* new-game position */",
        f"#define START_MAP MAP_{_ident(sm)}",
        f"#define START_SPAWN SPAWN_{_ident(sm)}_{_ident(ss)}",
    ]
    smap = world.by_name(sm)
    sspn = smap.spawn(ss) if smap else None
    if sspn:  # the blackout puts the hero back here (world.c)
        L += [
            f"#define START_I {sspn.i}",
            f"#define START_J {sspn.j}",
            f"#define START_FACING {sspn.facing}",
        ]
    L += ["", "/* spawn ids (per map) */"]
    for m in world.maps:
        for s in m.spawns:
            L.append(f"#define SPAWN_{_ident(m.name)}_{_ident(s.name)} {s.id}")
    L += ["", "/* global flag ids (save flags[32]) */"]
    for fid, nm in flag_names(world).items():
        L.append(f"#define {nm} {fid}")
    L += ["", "/* script variables (@var) */"]
    for nm, v in sorted(
        (getattr(world, "var_defs", None) or {}).items(), key=lambda kv: kv[1]
    ):
        L.append(f"#define VAR_{_ident(nm)} {v}")
    L += ["", "/* script ids (per map) */"]
    for m in world.maps:
        for k, sc in enumerate(m.scripts):
            L.append(f"#define SCRIPT_{_ident(m.name)}_{_ident(sc.name)} {k}")
    L += ["", "/* global text ids (txt_gl.bin) */"]
    for k in plan.gl:
        L.append(f"#define TXT_{_ident(k)} 0x{GLOBAL_BIT | plan.gl.index(k):04X}")
    # C6 (contract 46.1): the Planeador's towns (@fly), one row per slot: {map, landing i, j, facing, label text id}; text id 0 = the slot is unused
    fly = sorted((m.fly[0], m, m.fly) for m in world.maps if m.fly)
    L += [
        "",
        "/* Planeador towns (@fly; nucleo/scene_card.c FLY[]): visited flag 984 + slot, the header byte `fly` = 1 + slot */",
        f"#define FLY_N {fly[-1][0] + 1 if fly else 1}",
        "#define FLY_TABLE_INIT \\",
    ]
    rows = [
        f"    [{sl}] = {{MAP_{_ident(m.name)}, {f[2]}, {f[3]}, {f[4]}, TXT_{_ident('gl.area.' + f[1])}}}"
        for sl, m, f in fly
    ] or ["    [0] = {0, 0, 0, 0, 0}"]
    L.append(", \\\n".join(rows))
    L.append("")
    return "\n".join(L)


# ---- placeholder art (in memory only) -----------------------------------------------------------
def _ph_finish(
    entry_name, kind, lv_img, mk_img, ax, ay, ndirs=1, nframes=1, frames=None
):
    import spritec

    w, h = lv_img.size
    lp, mp = lv_img.load(), mk_img.load()
    lv = [[lp[x, y] & 3 for x in range(w)] for y in range(h)]
    mk = [[1 if mp[x, y] else 0 for x in range(w)] for y in range(h)]
    return lv, mk, w, h


def _ph_tile(entry):
    from PIL import Image, ImageDraw
    import spritec

    lv, mk = Image.new("L", (28, 15), 0), Image.new("L", (28, 15), 0)
    pts = [(14, 0), (27, 7), (14, 14), (0, 7)]
    fill = entry.fill
    dl, dm = ImageDraw.Draw(lv), ImageDraw.Draw(mk)
    dm.polygon(pts, fill=255)
    dl.polygon(pts, fill=0 if fill is None else fill, outline=3)
    l, k, w, h = _ph_finish(entry.name, "tile", lv, mk, 14, 7)
    return spritec.Sprite(entry, w, h, 14, 7, [[(l, k)]], True)


def _ph_object(entry, height=24):
    from PIL import Image, ImageDraw
    import spritec

    fw, fh = entry.fw, entry.fh
    W, H = (fw + fh) * 14 + 1, (fw + fh) * 7 + height + 1
    ox, oy = fw * 14, H - 1  # bottom vertex pixel
    lv, mk = Image.new("L", (W, H), 0), Image.new("L", (W, H), 0)
    B = (ox, oy)
    Lb = (ox - fw * 14, oy - fw * 7)
    R = (ox + fh * 14, oy - fh * 7)
    T = (Lb[0] + R[0] - B[0], Lb[1] + R[1] - B[1])
    up = lambda p: (p[0], p[1] - height)
    faces = [
        ([Lb, B, up(B), up(Lb)], 1),
        ([B, R, up(R), up(B)], 2),
        ([up(T), up(R), up(B), up(Lb)], 0),
    ]
    dl, dm = ImageDraw.Draw(lv), ImageDraw.Draw(mk)
    for pts, level in faces:
        dm.polygon(pts, fill=255)
        dl.polygon(pts, fill=level, outline=3)
    l, k, w, h = _ph_finish(entry.name, "object", lv, mk, ox, oy)
    return spritec.Sprite(entry, w, h, ox, oy, [[(l, k)]], True)


def _ph_actor(entry):
    from PIL import Image, ImageDraw
    import spritec

    w, h = 16, 26
    nd = entry.ndirs
    nf = entry.nframes or 1
    frames = []
    for d in range(nd):
        f8 = d * (8 // nd)
        lv, mk = Image.new("L", (w, h), 0), Image.new("L", (w, h), 0)
        dl, dm = ImageDraw.Draw(lv), ImageDraw.Draw(mk)
        dm.rounded_rectangle([2, 0, 13, h - 1], radius=5, fill=255)
        dl.rounded_rectangle([2, 0, 13, h - 1], radius=5, fill=1, outline=3)
        dl.ellipse([3, 1, 12, 10], fill=0, outline=3)
        sx, sy = DI[f8] - DJ[f8], (DI[f8] + DJ[f8]) * 0.5
        cx, cy = int(round(7.5 + sx * 2.5)), int(round(6 + sy * 2))
        dl.rectangle([cx - 1, cy - 1, cx, cy], fill=3)
        l, k, _, _ = _ph_finish(entry.name, "actor", lv, mk, w // 2, h - 1)
        frames.append([(l, k)] * nf)
    return spritec.Sprite(entry, w, h, w // 2, h - 1, frames, True)


def make_placeholder(entry):
    return {"tile": _ph_tile, "object": _ph_object, "actor": _ph_actor}[entry.kind](
        entry
    )


def sprite_catalog(world):
    if getattr(world, "sprite_cat", None) is None:
        import spritec

        world.sprite_cat = spritec.load_catalog(
            str(world.catalog.path), extra=world.catalog.extras
        )
    return world.sprite_cat


def missing_sprites(world, names):
    """{name: reason} for sprites spritec cannot load from real art."""
    sc = sprite_catalog(world)
    out = {}
    for n in names:
        try:
            sc.sprite(n)
        except Exception as e:  # noqa
            out[n] = str(e)
    return out


def build_map_bank(world, m, placeholder=False, stream=True):
    """-> (bank bytes, [names substituted by placeholders]). stream=False: the same bank with every actor resident (tests: pixel identity)."""
    import spritec

    sc = sprite_catalog(world)
    miss = missing_sprites(world, m.bank)
    used = []
    if miss:
        if not placeholder:
            raise spritec.CatalogError("missing art: " + ", ".join(sorted(miss)))
        for n in sorted(miss):
            sc._sprites[n] = make_placeholder(sc.entry(n))
            used.append(n)
    data, index = spritec.build_bank(sc, m.bank, stream=stream)
    assert [index[n] for n in m.bank] == list(range(len(m.bank)))
    return data, used


def cmd_build(world, placeholder=False, out_dir=None):
    out_dir = Path(out_dir) if out_dir else ROOT / "nucleo" / "files"
    gen_h = ROOT / "nucleo" / "gen" / "maps_gen.h"
    import textc

    if world.report.errors:
        print_report(world)
        print("build aborted: fix the map errors first")
        return 1
    world.placeholder = placeholder
    import mkfont

    cfg = mkfont.load_cfg()
    langs = mkfont.enabled_langs(cfg)
    master_l = mkfont.get_lang(None, cfg)
    master = textc.load(master_l.text_path)
    ltexts = {}  # lang key -> texts
    for l in langs:
        if not os.path.isfile(l.text_path):
            print(
                f"ERROR text: language {l.key}: {os.path.relpath(l.text_path, ROOT)} not found"
            )
            return 1
        ltexts[l.key] = master if l is master_l else textc.load(l.text_path)
        terrs = textc.lint(ltexts[l.key], l, None if l is master_l else master)
        if terrs:
            for e in terrs:
                print(f"ERROR text {l.key}:", e)
            return 1
        for e in [] if l is master_l else textc.warnings(ltexts[l.key], l, master):
            print(f"WARNING text {l.key}:", e)
    plan = get_plan(world)
    if plan.errors:
        for e in plan.errors:
            print("ERROR text:", e)
        return 1
    # art availability (all maps at once so the list is complete)
    allnames = sorted({n for m in world.maps for n in m.bank})
    miss = missing_sprites(world, allnames)
    if miss and not placeholder:
        print(
            f"ERROR: {len(miss)} referenced sprite(s) have no art yet (use --placeholder to substitute):"
        )
        for n in sorted(miss):
            print(f"  {n}: {miss[n]}")
        return 1
    if miss:
        print(
            f"placeholder art (memory only) for {len(miss)} sprite(s): "
            + ", ".join(sorted(miss))
        )
    out_dir.mkdir(parents=True, exist_ok=True)
    # every enabled language needs its font built from the same texts (charset ids must match)
    for l in langs:
        fp = out_dir / "lang" / l.dir / "font.nfnt"
        if fp.is_file():
            got = mkfont.parse_font(fp.read_bytes()).charset_id
            if got != l.charset_id():
                print(
                    f"ERROR: {fp.relative_to(out_dir.parent) if out_dir.parent in fp.parents else fp} is stale "
                    f"(charset {got:08x}, texts need {l.charset_id():08x}): run tools/mkfont.py"
                )
                return 1
        elif out_dir == ROOT / "nucleo" / "files":
            print(f"ERROR: {fp.relative_to(ROOT)} missing: run tools/mkfont.py first")
            return 1
    wanted = set()  # paths relative to out_dir
    warn = 0
    print(
        "map  name          file_size  bank   ground+cflags  objs npcs  text  (bytes)"
    )
    for m in world.maps:
        bank, _ = build_map_bank(world, m, placeholder)
        data = emit_v1(m, world, bank=bank, plan=plan)
        fn = f"map_{m.id:02d}.bin"
        tfn = f"txt_{m.id:02d}.bin"
        tdata = {}
        for l in langs:
            tdata[l.key] = emit_text(world, m, ltexts[l.key], l)
            (out_dir / "lang" / l.dir).mkdir(parents=True, exist_ok=True)
            (out_dir / "lang" / l.dir / tfn).write_bytes(tdata[l.key])
            wanted.add(f"lang/{l.dir}/{tfn}")
        (out_dir / fn).write_bytes(data)
        wanted.add(fn)
        flag = ""
        if len(data) > MAP_SIZE_WARN:
            flag = f"  WARNING: > {MAP_SIZE_WARN // 1024} KB (RAM budget)"
            warn += 1
        print(
            f"{m.id:3d}  {m.name:<12} {len(data):9d} {len(bank):6d}   {2 * m.w * m.h:12d} "
            f"{len(m.objects):5d} {len(m.npcs):4d} {len(tdata[master_l.key]):6d}{flag}"
        )
    for l in langs:
        gdata = emit_text_gl(world, ltexts[l.key], l)
        (out_dir / "lang" / l.dir / "txt_gl.bin").write_bytes(gdata)
        wanted.add(f"lang/{l.dir}/txt_gl.bin")
        print(
            f"lang/{l.dir}/txt_gl.bin: {len(plan.gl)} global texts, {len(gdata)} bytes"
        )
    stale = list(out_dir.glob("map_*.bin")) + list(
        out_dir.glob("txt_*.bin")
    )  # txt_*: pre-M1.5 layout
    for l in langs:
        stale += list((out_dir / "lang" / l.dir).glob("txt_*.bin"))
    for p in stale:
        # (contract 49: the old v1 fixture map_99.bin of tools/make_test_map.py lives in build/m1_test now; an id-99 file in nucleo/files is a stale file like any other: MAP_COUNT has a hole at 99)
        if str(p.relative_to(out_dir)) not in wanted:
            p.unlink()
            print(f"removed stale {p.relative_to(out_dir)}")
    gen_h.parent.mkdir(parents=True, exist_ok=True)
    gen_h.write_text(emit_header_h(world))
    print(f"wrote {gen_h.relative_to(ROOT)}")
    if warn:
        print(f"WARNING: {warn} map file(s) exceed {MAP_SIZE_WARN // 1024} KB")
    return 0


# ---- CLI ------------------------------------------------------------------------------------
def print_report(w):
    rep = w.report
    for m in w.maps:
        print(
            f"map {m.id} {m.name:<12} {m.w}x{m.h}{' interior' if m.interior else ''}: "
            f"{len(m.objects)} objs, {len(m.npcs)} npcs, {len(m.warps)} warps "
            f"({len(m.doors)} doors), {len(m.interacts)} interacts, {len(m.spawns)} spawns, "
            f"{len(m.bank)} bank sprites, {len(m.text_keys)} text keys"
            + (f", {len(m.scripts)} scripts" if m.scripts else "")
        )
    for n in rep.notes:
        print("note:", n)
    for x in rep.warnings:
        print("WARNING:", x)
    for x in rep.errors:
        print("ERROR:", x)
    print(f"{len(rep.errors)} error(s), {len(rep.warnings)} warning(s)")


def cmd_refmaps(world, out_dir):
    """Test fixture: every map_NN.bin with ALL actors resident (no SPRF_STREAM), so the host tests can compare the streamed views with the
    resident ones pixel for pixel (tools/host_world/stream_check.c). Same format v2, same sections, only the bank layout differs.
    """
    if world.report.errors:
        print_report(world)
        return 1
    world.placeholder = True
    plan = get_plan(world)
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for m in world.maps:
        bank, _ = build_map_bank(world, m, True, stream=False)
        (out_dir / f"map_{m.id:02d}.bin").write_bytes(
            emit_v1(m, world, bank=bank, plan=plan)
        )
    print(f"refmaps: {len(world.maps)} all-resident maps in {out_dir}")
    return 0


def main(argv):
    if len(argv) < 2 or argv[1] not in ("check", "preview", "build", "refmaps"):
        print(__doc__)
        return 2
    w = load_world()
    if argv[1] == "check":
        print_report(w)
        return 1 if w.report.errors else 0
    if argv[1] == "refmaps":
        if len(argv) != 3:
            print(
                "usage: mapc.py refmaps <out_dir>   (test fixture: all actors resident)"
            )
            return 2
        return cmd_refmaps(w, argv[2])
    if argv[1] == "build":
        extra = argv[2:]
        bad = [a for a in extra if a != "--placeholder"]
        if bad:
            print("usage: mapc.py build [--placeholder]")
            return 2
        return cmd_build(w, placeholder="--placeholder" in extra)
    # preview
    if len(argv) < 3:
        print("usage: mapc.py preview <map|all> [out.png]")
        return 2
    if w.report.errors:
        print_report(w)
        print("(preview continues with the maps that parsed)")
    targets = w.maps if argv[2] == "all" else [w.by_name(argv[2])]
    if None in targets:
        print(f"unknown map {argv[2]!r}")
        return 2
    for m in targets:
        out = (
            Path(argv[3])
            if len(argv) > 3 and argv[2] != "all"
            else ROOT / "build" / "preview" / f"map_{m.name}.png"
        )
        render_preview(w, m, out)
        grid = out.with_name(out.stem + "_grid.png")
        render_grid(m, grid)
        print(f"wrote {out} and {grid}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
