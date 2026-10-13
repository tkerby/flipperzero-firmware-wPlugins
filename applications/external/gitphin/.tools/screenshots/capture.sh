#!/usr/bin/env bash
# Makes the README screenshots (docs/screenshots/*.png) of the web page, with
# made-up demo repos and the app's real route code running on the PC.
# Usage: tools/screenshots/capture.sh
# Needs gcc, zlib, git, python3, Node.js and Chromium (CHROMIUM=/path, default:
# Playwright's). Installs playwright-core into a temp folder.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$HERE/../.." && pwd)"
T="${TESTS:-$TOP/pc/tests}"
APP="$TOP/flipper_app"
W="$(mktemp -d)"; trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$W"' EXIT

# the route code on the host, as in pc/tests/run_route_test.sh (no sanitizers)
mkdir -p "$W/b"; cd "$W/b"
CF="-O1 -I$T/furi_stub -Wno-unused-parameter -include $T/malloc_wrap.h"
for f in hub_chunk hub_gitview hub_git hub_gitobj hub_inflate hub_gitdiff hub_gitsearch; do
    gcc $CF -c "$APP/$f.c" -o "$f.o"
done
gcc $CF -DHUB_EXT_QUERY -c "$APP/hub_gitext.c" -o hub_gitext.o
gcc -O1 -I"$T/furi_stub" -c "$T/gitio_stdio.c" -o gitio.o
gcc $CF -c "$T/furi_stub.c" -o furi_stub.o
gcc $CF -c "$T/route_test.c" -o route_test.o
gcc -O1 -c "$T/route_heap.c" -o route_heap.o
gcc *.o -lz -o route_test

"$HERE/make_demo.sh" "$W/root" >/dev/null
python3 -I "$HERE/demo_server.py" "$W/b/route_test" "$W/root" 8099 &
for _ in $(seq 50); do curl -s http://127.0.0.1:8099/api/info >/dev/null && break; sleep 0.1; done

cd "$W" && npm init -y >/dev/null && npm i -s playwright-core@1.55.0 >/dev/null
mkdir -p "$TOP/docs/screenshots"
NODE_PATH="$W/node_modules" node "$HERE/capture.cjs" http://127.0.0.1:8099 "$TOP/docs/screenshots"
