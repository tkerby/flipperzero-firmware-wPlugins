#!/usr/bin/env bash
# Full content pipeline (contract section 6).  Usage: tools/build_assets.sh [--placeholder]
#   --placeholder is passed to `mapc.py build` (generated stand-in art for missing sprites).
# The last step is the memory-budget guard (contract 5.x): `tools/mem_budget.sh` = `ufbt build` of the dev (NUCLEO_DEBUG=1) and the release build + tools/mem_budget.py, because new maps /
# art change the heap the app needs while exploring (the map files are model inputs). It makes this script fail when a budget is broken.
# Skip it (no ufbt, or the app does not need rebuilding): NO_UFBT=1 tools/build_assets.sh   (then run `bash tools/mem_budget.sh` yourself before "done").
set -e
cd "$(dirname "$0")/.."
mkdir -p build   # the logs of the steps below (a fresh clone has no build/ folder)
# the project venv (`make setup`); without it (CI, a system python with requirements.txt installed) the python3 in PATH is used
if [ -f .venv/bin/activate ]; then source .venv/bin/activate; fi

if [ -f tools/gen_tiles.py ]; then
    echo "== gen_tiles"
    python3 tools/gen_tiles.py
fi

shopt -s nullglob
manifests=(assets/render/m1_*.toml)
if [ ${#manifests[@]} -gt 0 ]; then
    echo "== render_objects"
    for mf in "${manifests[@]}"; do
        python3 tools/render_objects.py "$mf"
    done
fi

echo "== mkbike (the hero on the bicycle: assets/art/hero_bike -> files/hero_bike.bin, hero.bin's format; contract 35.4 / 38.6; and the surfing hero: assets/art/hero_surf -> files/hero_surf.bin when the art exists, contract 42.1)"
python3 -I tools/mkbike.py
python3 -I tools/mkbike.py check
echo "== legend_ow (the legendary birds' overworld sprites: assets/art/elementis/<species>_c -> assets/art/npcs/<species>_ow, contract 52)"
python3 -I tools/legend_ow.py check
echo "== empuje_ow (the pushable rock and its pit: assets/art/objects_c5 PixelLab pictures -> assets/art/npcs/boulder_rock|hole_pit/south.png, contract 58)"
python3 -I tools/empuje_ow.py check
echo "== species sources (contract 58 / 66: every species of species.toml can be obtained on at least one affinity: wild / gift / trade / prize / fossil / evolution; Genesa excluded; the Solar / Lunar rules)"
python3 -I tools/species_sources_check.py
echo "== affinity test (contract 66: assets/data/affinity.toml, files/aff.bin, the checker's rules and the cross-affinity trade lists)"
python3 -I tools/affinity_test.py
echo "== trainer names (contract 54 / 58: one personal name per trainer in every language, no rival / hero preset)"
python3 -I tools/trainer_names_check.py
echo "== roam_ow (the roaming creatures' overworld actors: assets/art/elementis/<species>_c -> assets/art/npcs/<species>_roam + assets/catalog_roam.toml for the species of every @roam map's table, contract 55; \`python3 tools/roam_ow.py build\` writes them)"
python3 -I tools/roam_ow.py check
echo "== follow_ow (the follower's overworld sprites: assets/art/elementis/<species>_c -> files/follow.bin for ALL species, contract 69; \`python3 tools/follow_ow.py build\` writes it) (its content test runs after mapc: it reads the generated text ids and map headers)"
python3 -I tools/follow_ow.py check
echo "== mkfont"
python3 tools/mkfont.py
echo "== textc lint"
python3 tools/textc.py lint
if [ -f tools/i18n/check_translations.py ]; then
    python3 tools/i18n/check_translations.py
fi
echo "== regc (battle data)"
python3 tools/regc.py
python3 tools/regc.py check
echo "== discs and new items are obtainable (shops, prizes, pouches, gifts; contract 64)"
python3 -I tools/disc_sources_check.py > build/disc_sources.log && tail -1 build/disc_sources.log
echo "== monc (Elementi pictures)"
python3 -I tools/monc.py
python3 -I tools/monc.py check
echo "== titlec (title screen art -> files/title.bin)"
python3 -I tools/titlec.py
python3 -I tools/titlec.py check
echo "== mapc check"
python3 tools/mapc.py check
echo "== mapc casino macros test (coins_buy / slots / prizes / coins_get = native pc, contract 37.3 / 38.7)"
python3 -I tools/mapc_casino_test.py
echo "== mapc IR link macros test (link trade / battle / selftest = native 22 pc ops 9..12, contract 45.7)"
python3 -I tools/mapc_link_test.py
echo "== regc evolve_friend test (friendship evolutions = reg.bin evo_level | 0x80, hatchkip and dashkip, contract 65)"
python3 -I tools/regc_friend_test.py
echo "== mapc 10-bit flags test (contract 40: the flag operands, the spare bits, the automatic pool, the mansion floor)"
python3 -I tools/mapc_flags_test.py
echo "== mapc C5 world systems test (contract 42: water / fishing tables, boulders, pits, sleeper, invisible walls, bike slopes, @surf, mkbike surf)"
python3 -I tools/mapc_c5_test.py
echo "== mapc build $*"
python3 tools/mapc.py build "$@"
echo "== mapdump --verify"
python3 tools/mapdump.py --verify
echo "== areac (the Registro's AREA page: the zones of every species' wild tables for each affinity -> files/regarea.bin, contract 75; needs aff.bin from regc)"
python3 -I tools/areac.py selftest
python3 -I tools/areac.py
python3 -I tools/areac.py check
echo "== regmapc (the Atlas' region map: assets/data/regmap.toml + the map list -> files/regmap.bin, contract 74; build/regmap/regmap_preview.png)"
python3 -I tools/regmapc.py
python3 -I tools/regmapc.py check
echo "== follower test (contract 69: files/follow.bin has a sprite for every species, the texts gl.fol.*, @nofollow and its header bit on the ferry and the sea maps)"
python3 -I tools/follow_test.py
echo "== mapc C6 world systems test (contract 46: @fly, keydoor=, gift=, pad= + @pad, give_box, the reachability edges of the pads)"
python3 -I tools/mapc_c6_test.py
echo "== mapc C9 systems test (contract 50: the league flag names, ending macros, the PC browser tail of \`pc\`, @warp oneway, the door idiom)"
python3 -I tools/mapc_c9_test.py
echo "== mapc legendary encounters test (contract 52: the legend= idiom, its generated script and refusals, the catalog actors, the music / effect lines)"
python3 -I tools/mapc_legend_test.py
echo "== mapc roaming creatures test (contract 55: @roam, the roam area mask and the NPC slots, its refusals, the extras' roam block, the generated actors)"
python3 -I tools/mapc_roam_test.py
echo "== mapc C9 content test (contract 56: setv sp:<species>, caught_ge VAR full, the zone banner keys and texts, the flags 904..923, Ruta 23's guards, the Senda's gates, the League's doors and warps, the music lines)"
python3 -I tools/mapc_c9content_test.py
echo "== C9 pieces (contract 56: every piece of gen_tiles c9_pieces() is pixel-identical to the drawing of the approved mock) + the C7 species swap tool's selftest"
python3 -I tools/c9_pieces_check.py
python3 -I tools/c9_c7_swap.py --selftest
python3 -I tools/c10_pieces_check.py
echo "== shopc (shops.bin)"
python3 tools/shopc.py
python3 tools/shopc.py check
echo "== musc (music + sound effects -> files/music.bin + gen/music_gen.h; assets/music/*.txt; listen first with: python3 tools/musc.py --wav build/music_wav)"
python3 -I tools/musc.py
python3 -I tools/musc.py check
echo "== cryc (the species' cries -> files/cry.bin: a base pattern + root note + length per species, contract 75; listen first with: python3 tools/cryc.py --wav build/cry_wav)"
python3 -I tools/cryc.py selftest
python3 -I tools/cryc.py
python3 -I tools/cryc.py check
echo "== fxbake (battle effects -> files/fx.bin; tools/fxbake, needs a host C compiler: cc)"
make -s -C tools/fxbake
make -s -C tools/fxbake check
echo "== platbake (the battle platforms -> files/plat.bin; tools/platbake/plat_ref.h is the original builder, contract 73)"
make -s -C tools/platbake
make -s -C tools/platbake check
if [ -n "$NO_UFBT" ]; then
    echo "== mem_budget: SKIPPED (NO_UFBT=1): run 'bash tools/mem_budget.sh' before reporting done"
else
    echo "== mem_budget (ufbt build + memory budget guard)"
    bash tools/mem_budget.sh
fi
echo "build_assets: OK"
