#!/usr/bin/env bash
# Build + run the host unit tests (pure logic, no hardware). Used locally and in CI
# (.github/workflows/tests.yml). Any failing test exits non-zero.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
CXX="${CXX:-g++}"
fail=0
for src in test_*.cpp; do
    bin="/tmp/pwnpal-${src%.cpp}"
    echo "=== $src ==="
    "$CXX" -std=c++17 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined "$src" -o "$bin"
    "$bin" || fail=1
done
[ "$fail" -eq 0 ] && echo "ALL TESTS PASSED" || { echo "TESTS FAILED"; exit 1; }
