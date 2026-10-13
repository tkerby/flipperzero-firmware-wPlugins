#!/usr/bin/env bash
# Host tests for hub_gitdiff.c (diff, compare) and hub_gitcommit.c (commit_file,
# tree merge) against git.
# Usage: pc/tests/run_logic_test.sh      (KEEP=1 keeps the scratch dir)
# Needs gcc, git (2.38+ for merge-tree --write-tree), python3. Scratch:
# /tmp/claude-1000/logic_test. Builds the test repos with make_test_repo.sh.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/logic_test.XXXXXX)}"
rm -rf "$W"; mkdir -p "$W"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT

CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cd "$W"
# library code gets the malloc wrapper (heap peaks); the harness and I/O layers do not
for f in hub_inflate hub_gitobj hub_gitdiff hub_gitcommit hub_gitwrite hub_sha1; do
    gcc $CF -DHUB_TEST_COUNTERS -include "$HERE/malloc_wrap.h" -c "$APP/$f.c" -o "$f.o"
done
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/gitwrite_stdio.c" -o wio.o
gcc $CF -c "$HERE/logic_test.c" -o test.o
gcc $CF *.o -lz -o logic_test

"$HERE/make_test_repo.sh" "$W/repos" > make.log 2>&1 || { tail -20 make.log; exit 1; }
tail -2 make.log
python3 -I "$HERE/logic_check.py" "$W/logic_test" "$W/repos" "$W/work" 2>&1 | tee check.log
grep -q "python checks passed" check.log && echo "ALL TESTS PASSED" || { echo "TESTS FAILED"; exit 1; }
