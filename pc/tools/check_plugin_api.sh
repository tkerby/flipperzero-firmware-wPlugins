#!/usr/bin/env bash
# Fails when a plugin (.fal) imports a hub_* core function that is not in the core's API table
# (flipper_app/src/http/hub_api_table.inc): such a plugin builds fine but cannot load on the
# Flipper ("Failed to resolve address for symbol"). Run after a uFBT build.
# Usage: pc/tools/check_plugin_api.sh [dir with the built .fal files]   (NM=/path/to/arm nm)
set -euo pipefail
TOP="$(cd "$(dirname "$0")/../.." && pwd)"
mapfile -t fals < <(find "${1:-$HOME/.ufbt/build}" "$TOP/flipper_app/dist" -name 'hub_pl_*.fal' 2>/dev/null | awk -F/ '!seen[$NF]++')
(( ${#fals[@]} )) || { echo "no hub_pl_*.fal found: build first (ufbt)"; exit 1; }
NM="${NM:-$(ls "$HOME"/.ufbt/toolchain/*/bin/arm-none-eabi-nm 2>/dev/null | head -1)}"
NM="${NM:-$(command -v arm-none-eabi-nm || true)}"
[[ -x "$NM" ]] || { echo "arm-none-eabi-nm not found (set NM=...)"; exit 1; }
imports=$(for f in "${fals[@]}"; do "$NM" -u "$f"; done | awk '{print $NF}' | grep '^hub_' | sort -u)
table=$(grep -oE 'API\([a-z_0-9]+\)' "$TOP/flipper_app/src/http/hub_api_table.inc" | sed 's/API(//;s/)//' | sort -u)
[[ -n "$imports" ]] || { echo "no hub_* imports read from ${#fals[@]} plugins: wrong nm?"; exit 1; }
missing=$(comm -23 <(echo "$imports") <(echo "$table"))
if [[ -n "$missing" ]]; then
    echo "plugins import core functions that are missing from hub_api_table.inc:"; echo "$missing"; exit 1
fi
echo "plugin API ok: ${#fals[@]} plugins, $(echo "$imports" | wc -l) imports, all in the core API table"
