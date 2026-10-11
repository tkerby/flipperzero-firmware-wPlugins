#!/usr/bin/env bash
# Runs the real route code (hub_git.c, hub_gitview.c, hub_gitext.c query build, hub_chunk.c) on the
# host with stand-ins for the firmware (furi_stub/, furi_stub.c) and checks every answer: HTTP/chunked
# framing, valid JSON, and the content against git, on all test repos.
# Usage: pc/tests/run_route_test.sh      (KEEP=1 keeps the scratch dir)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/route_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W/b"
export ASAN_OPTIONS=detect_leaks=0
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT
CF0="-g -O1 -fsanitize=address,undefined"
CF="-include $HERE/malloc_wrap.h -g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -I$HERE/furi_stub -Wno-unused-parameter"
cd "$W/b"
for f in hub_chunk hub_gitview hub_git hub_gitobj hub_inflate hub_gitdiff hub_gitsearch; do
    gcc $CF -c "$APP/$f.c" -o "$f.o"
done
gcc $CF -DHUB_EXT_QUERY -c "$APP/hub_gitext.c" -o hub_gitext.o
gcc $CF0 -I$HERE/furi_stub -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/furi_stub.c" -o furi_stub.o
gcc $CF -c "$HERE/route_test.c" -o route_test.o
gcc ${CF0} -c "$HERE/route_heap.c" -o route_heap.o
gcc $CF *.o -lz -o route_test
cd "$W"
"$HERE/make_test_repo.sh" "$W/repos" > make.log 2>&1 || { tail -20 make.log; exit 1; }
python3 -I "$HERE/route_check.py" "$W/b/route_test" "$W/repos" "$W/root" 2>&1 | tee check.log
if tail -1 check.log | grep -q "failures: 0"; then echo "ALL TESTS PASSED"; else echo "TESTS FAILED"; exit 1; fi
