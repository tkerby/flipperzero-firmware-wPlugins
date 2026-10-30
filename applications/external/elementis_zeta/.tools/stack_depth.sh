#!/bin/bash
# Static stack depth of OUR code on the game thread (contract 5, Memory: thread stacks). Compiles the app + plugin sources with the
# ufbt flags plus -fcallgraph-info=su -fstack-usage into a scratch dir and runs tools/stack_depth.py on game_thread.
#   tools/stack_depth.sh                 deepest path overall
#   tools/stack_depth.sh plugin_load     deepest path that ends in plugin_load
# The firmware's frames below our calls (storage, printf, the ELF loader: ~3.5 KB on the plugin load) are NOT counted: add the
# measured S / T of the debug overlay.
set -e
cd "$(dirname "$0")/.."
if [ -f .venv/bin/activate ]; then source .venv/bin/activate; fi
export PATH=$HOME/.ufbt/toolchain/arm64-darwin/bin:$PATH
OUT=${OUT:-/tmp/nucleo_su}
mkdir -p "$OUT"
touch nucleo/world.c
CC=$(cd nucleo && ufbt -n build VERBOSE=1 2>&1 | grep -m1 'scene_world.c' | sed 's/^arm-none-eabi-gcc -o [^ ]* -c/arm-none-eabi-gcc -c/; s# [^ ]*/scene_world.c$##')
for f in nucleo/*.c; do
    b=$(basename "$f" .c)
    (cd "$OUT" && eval "$CC -fcallgraph-info=su -fstack-usage -o $b.o $PWD/../../$f" 2>/dev/null) || (cd "$OUT" && eval "$CC -fcallgraph-info=su -fstack-usage -o $b.o $OLDPWD/$f" 2>/dev/null) || true
done
if [ -n "$1" ]; then TO="--to $1"; fi
python3 tools/stack_depth.py "$OUT" game_thread --auto $TO
