#!/usr/bin/env bash
# Host tests for the commit graph (hub_gitgraph.c, hub_git_commit_meta), the one-pack
# idle packer and the file-read counts of compare/log with and without the graph.
# Usage: pc/tests/run_graph_test.sh      (KEEP=1 keeps the scratch dir)
# Needs gcc, git, python3, zlib headers. Scratch: /tmp/claude-1000/graph_test.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/graph_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W/lt" "$W/gp"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT
CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cd "$W/lt"
for f in hub_inflate hub_gitobj hub_gitdiff hub_gitcommit hub_gitwrite hub_sha1; do
    gcc $CF -DHUB_TEST_COUNTERS -include "$HERE/malloc_wrap.h" -c "$APP/$f.c" -o "$f.o"
done
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/gitwrite_stdio.c" -o wio.o
gcc $CF -c "$HERE/logic_test.c" -o test.o
gcc $CF *.o -lz -o logic_test
cd "$W/gp"
for f in hub_deflate hub_gitpack hub_gitgraph hub_gitobj hub_inflate; do
    gcc $CF -include "$HERE/malloc_wrap.h" -c "$APP/$f.c" -o $f.o
done
gcc $CF -c "$APP/hub_sha1.c" -o sha1.o
gcc $CF -c "$APP/hub_gitdocutil.c" -o docutil.o
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/gitpack_tool.c" -o tool.o
gcc $CF *.o -o gitpack_tool
cd "$W"
"$HERE/make_test_repo.sh" "$W/repos" > make.log 2>&1 || { tail -20 make.log; exit 1; }
python3 -I "$HERE/graph_check.py" "$W/lt/logic_test" "$W/gp/gitpack_tool" "$W/repos" "$W/work" 2>&1 | tee check.log
if tail -1 check.log | grep -q "failures: 0"; then echo "ALL TESTS PASSED"; else echo "TESTS FAILED"; exit 1; fi
