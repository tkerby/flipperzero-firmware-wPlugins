#!/usr/bin/env bash
# Host test of hub_gitsearch.c against `git grep -n -i -I -F` and `git ls-tree`.
# Usage: pc/tests/run_search_test.sh      Scratch: /tmp/claude-1000/search_test
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/search_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT
CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cd "$W"
for f in util/hub_inflate obj/hub_gitobj ext/hub_gitsearch; do
    gcc $CF -include "$HERE/malloc_wrap.h" -c "$APP/src/$f.c" -o "${f##*/}.o"
done
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/search_test.c" -o test.o
gcc $CF *.o -lz -o search_test
"$HERE/make_test_repo.sh" "$W/repos" > make.log 2>&1 || { tail -20 make.log; exit 1; }
python3 -I "$HERE/search_check.py" "$W/search_test" "$W/repos" | tee check.log
grep -q " 0 failures" check.log && echo "ALL TESTS PASSED" || { echo "TESTS FAILED"; exit 1; }
