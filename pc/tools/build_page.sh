#!/bin/sh
# Usage: pc/tools/build_page.sh [OUT_DIR]
# Bundles flipper_app/web/ (see its README.md) into OUT_DIR/index.html and OUT_DIR/index.html.gz
# (gzip -9 -n). Default OUT_DIR: flipper_app/files. Run it before ufbt launch and commit the result.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/../../flipper_app/files}"
python3 -I "$here/build_page.py" "$out"
gzip -9 -n -c "$out/index.html" > "$out/index.html.gz"
