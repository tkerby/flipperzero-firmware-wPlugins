#!/usr/bin/env bash
# Build and run the host tests for hub_inflate.c / hub_gitobj.c.
# Usage: pc/tests/run_gitobj_test.sh        (KEEP=1 keeps the generated repos)
# Needs gcc, git, python3 and zlib headers. Scratch goes to /tmp/claude-1000/gitobj_test.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$(cd "$HERE/../../flipper_app" && pwd)"
mkdir -p /tmp/claude-1000; W="${WORK:-$(mktemp -d /tmp/claude-1000/gitobj_test.XXXXXX)}"
mkdir -p "$W"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
[[ -n "${KEEP:-}" ]] || trap 'rm -rf "$W"' EXIT

CF="-g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cd "$W"
# library code gets the malloc wrapper, harness and I/O layer do not
gcc $CF -include "$HERE/malloc_wrap.h" -c "$APP/hub_inflate.c" -o inflate.o
gcc $CF -include "$HERE/malloc_wrap.h" -c "$APP/hub_gitobj.c" -o gitobj.o
gcc $CF -c "$HERE/gitio_stdio.c" -o gitio.o
gcc $CF -c "$HERE/gitobj_test.c" -o test.o
gcc $CF inflate.o gitobj.o gitio.o test.o -lz -o gitobj_test

fail=0
echo "=== inflate vs zlib ==="
./gitobj_test --inflate || fail=1

[[ -d "$W/repos/onepack.git" ]] || "$HERE/make_test_repo.sh" "$W/repos"

for layout in loose onepack multi refdelta bigoff; do
    for limit in 24576 4096 300 1048576; do
        echo
        ./gitobj_test "$W/repos/$layout.git" --limit "$limit" || fail=1
    done
done

echo
echo "=== corruption (on copies) ==="
for layout in onepack multi refdelta; do
    rm -rf "$W/fuzz.git"
    cp -r "$W/repos/$layout.git" "$W/fuzz.git"
    chmod -R u+w "$W/fuzz.git"
    ./gitobj_test "$W/fuzz.git" --fuzz || fail=1
done

echo
if [[ $fail = 0 ]]; then echo "ALL TESTS PASSED"; else echo "TESTS FAILED"; fi
exit $fail
